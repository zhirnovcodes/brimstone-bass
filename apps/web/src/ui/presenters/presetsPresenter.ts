import { Subscriptions } from '../core/event'
import { Screen } from '../data/uiData'
import type { MainUiModel } from '../models/mainUiModel'
import type { PresetBridge } from '../skin'
import type { PresetsView } from '../views/presetsView'

/** Shown when the sound did not come from a preset. */
export const NO_PRESET_NAME = 'UNTITLED'

/**
 * Connects the PRESETS screen to the presets. The list reloads each time the screen opens and when the folder
 * changes; a row click selects, OPEN (or a double click) loads the selected preset. SAVE / LOAD use the user's
 * computer. Results show on the output line.
 */
export class PresetsPresenter {
    private readonly subs = new Subscriptions()
    private items: string[] = []
    private selected = -1
    private disposed = false
    private listing = 0 // the newest list() call: older answers are dropped

    constructor(
        private readonly presets: PresetBridge,
        private readonly ui: MainUiModel,
        private readonly view: PresetsView,
    ) {
        this.showName(presets.name())
        view.setItems([])
        view.setSelected(-1)
        this.reload()

        this.subs.add(presets.onName((name) => this.showName(name)))
        this.subs.add(presets.onListChanged(() => this.reload()))
        this.subs.add(
            ui.changed.on((change) => {
                if (change === 'screen' && ui.getCurrentScreen() === Screen.Presets) this.reload()
            }),
        )
        this.subs.add(view.itemClicked.on((index) => this.select(index)))
        this.subs.add(
            view.itemOpened.on((index) => {
                this.select(index)
                this.openSelected()
            }),
        )
        this.subs.add(view.openClicked.on(() => this.openSelected()))
        this.subs.add(view.saveClicked.on(() => this.run(presets.saveFile(), 'SAVED', 'NOT SAVED')))
        this.subs.add(view.loadClicked.on(() => this.run(presets.loadFile(), 'LOADED', 'NOT A PRESET')))
    }

    dispose(): void {
        this.disposed = true
        this.subs.dispose()
    }

    private reload(): void {
        const call = ++this.listing
        this.presets.list().then(
            (items) => {
                if (this.disposed || call !== this.listing) return
                const keep = this.items[this.selected]
                this.items = items
                this.view.setItems(items.map((name) => name.toUpperCase()))
                // Keep the selection; with none, select the current preset.
                const current = this.presets.name().toLowerCase()
                this.select(keep !== undefined ? items.indexOf(keep) : items.findIndex((n) => n.toLowerCase() === current))
            },
            () => {},
        )
    }

    private select(index: number): void {
        this.selected = index >= 0 && index < this.items.length ? index : -1
        this.view.setSelected(this.selected)
    }

    private openSelected(): void {
        const name = this.items[this.selected]
        if (name === undefined) {
            this.say('SELECT A PRESET')
            return
        }
        this.run(
            this.presets.open(name).then(() => name),
            'OPENED',
            'COULD NOT OPEN',
        )
    }

    /** Waits for a preset action and says how it went. null = the user cancelled: nothing to say. */
    private run(action: Promise<string | null>, done: string, failed: string): void {
        action.then(
            (name) => {
                if (name !== null) this.say(`${done} ${name}`)
            },
            () => this.say(failed),
        )
    }

    private say(text: string): void {
        if (!this.disposed) this.ui.setOutputText(text.toUpperCase())
    }

    private showName(name: string): void {
        this.view.setName(name ? name.toUpperCase() : NO_PRESET_NAME)
    }
}
