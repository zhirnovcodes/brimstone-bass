import { PARAMS } from '../../params'
import { Subscriptions } from '../core/event'
import { FX_ORDER_INDEX } from '../data/engineData'
import { fxReplaceOutput } from '../data/paramText'
import { NO_FX_BUTTON, SCREENS } from '../data/uiData'
import type { EngineDataModel } from '../models/engineDataModel'
import type { MainUiModel } from '../models/mainUiModel'
import type { EventView } from '../views/eventView'
import type { MainView } from '../views/mainView'

/**
 * Connects the main view to the UI model, and the event view to the output line:
 * hover or change a control → its output line shows at the bottom; leave it → the line clears.
 * While a control is active its line follows the engine (automation, LFO time switching to beats, ...).
 * An effects-order button reads e.g. "FIL. REPLACE" while an effect waits to be swapped, the chain otherwise.
 */
export class MainPresenter {
    private readonly subs = new Subscriptions()
    private active: number | null = null

    constructor(
        private readonly engine: EngineDataModel,
        private readonly ui: MainUiModel,
        private readonly view: MainView,
        events: EventView,
    ) {
        this.showScreen()
        view.setOutputText(ui.getOutputText())

        this.subs.add(view.prevClicked.on(() => ui.prevScreen()))
        this.subs.add(view.nextClicked.on(() => ui.nextScreen()))
        this.subs.add(
            ui.changed.on((change) => {
                if (change === 'screen') this.showScreen()
                if (change === 'output') view.setOutputText(ui.getOutputText())
                if (change === 'fxButton' && this.active !== null && isFxIndex(this.active)) this.refresh()
            }),
        )
        this.subs.add(events.paramHovered.on((index) => this.activate(index)))
        this.subs.add(events.paramChanged.on((index) => this.activate(index)))
        this.subs.add(
            events.paramLeft.on((index) => {
                if (this.active === index) {
                    this.active = null
                    ui.setOutputText('')
                }
            }),
        )
        // The active line follows its own value; LFO time also follows its sync toggle.
        this.subs.add(
            engine.changed.on((index) => {
                if (this.active !== null && dependsOn(this.active, index)) this.refresh()
            }),
        )
    }

    dispose(): void {
        this.subs.dispose()
    }

    private activate(index: number): void {
        this.active = index
        this.refresh()
    }

    private refresh(): void {
        if (this.active === null) return
        const fxOsc = FX_ORDER_INDEX.findIndex((i) => i === this.active)
        const pressed = fxOsc < 0 ? NO_FX_BUTTON : this.ui.getFxButtonPressed(fxOsc as 0 | 1)
        if (pressed !== NO_FX_BUTTON) {
            const order = this.engine.getOscillatorsData()[fxOsc === 0 ? 'osc1' : 'osc2'].fxOrder
            this.ui.setOutputText(fxReplaceOutput(order[pressed]))
        } else {
            this.ui.setOutputText(this.engine.getParamOutput(this.active))
        }
    }

    private showScreen(): void {
        const screen = this.ui.getCurrentScreen()
        this.view.setScreenIndex(screen)
        this.view.setCaptionText(SCREENS[screen])
    }
}

const isFxIndex = (index: number) => FX_ORDER_INDEX.some((i) => i === index)

/** Does the output line of UI index `active` change when `changed` changes? */
function dependsOn(active: number, changed: number): boolean {
    if (active === changed) return true
    const time = /^lfo(\d)Time$/.exec(PARAMS[active]?.id ?? '')
    return time !== null && PARAMS[changed]?.id === `lfo${time[1]}Sync`
}
