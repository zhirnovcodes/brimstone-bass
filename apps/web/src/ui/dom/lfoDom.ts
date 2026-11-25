import { INDEX } from '../../params'
import { Emitter } from '../core/event'
import type { Edit } from '../views/edit'
import type { LfoView } from '../views/lfoView'
import type { DomEvents } from './domEvents'
import { arrowDown, label, PixelButton, PixelField, PixelList, PixelSlider, SHADOW } from './widgets'

/** LFO1 / LFO2 / LFO3 page. Positions: Figma frame "5", LFO1 screen. */
export class LfoDomView implements LfoView {
    readonly openMapClicked = new Emitter<void>()
    readonly closeMapClicked = new Emitter<void>()
    readonly leftTypeClicked = new Emitter<void>()
    readonly rightTypeClicked = new Emitter<void>()
    readonly mapValueSelected = new Emitter<number>()
    readonly offsetChanged = new Emitter<Edit>()
    readonly retrigClicked = new Emitter<void>()
    readonly syncClicked = new Emitter<void>()

    private readonly map: PixelField
    private readonly type: PixelField
    private readonly offset: PixelSlider
    private readonly retrig: PixelButton
    private readonly sync: PixelButton
    private readonly mapButton: PixelButton
    private readonly list: PixelList
    private listOpen = false

    constructor(doc: Document, page: HTMLElement, events: DomEvents, lfo: 1 | 2 | 3) {
        const id = (name: string) => INDEX.get(`lfo${lfo}${name}`)!

        // MAP: field + drop-down button
        this.map = new PixelField(doc, { x: 41, y: 32, w: 75, h: 14 }, 49)
        this.mapButton = new PixelButton(doc, { x: 117, y: 31, w: 14, h: 14 }, arrowDown(doc, 4, 2), `LFO ${lfo} map list`, SHADOW.dark)
        this.mapButton.clicked.on(() => (this.listOpen ? this.closeMapClicked : this.openMapClicked).emit())
        this.list = new PixelList(doc, { box: { x: 41, y: 31, w: 74, h: 71 }, rows: 5, firstTop: 2, step: 13.25 }, `LFO ${lfo} map`, () => [this.mapButton.el])
        this.list.selected.on((i) => {
            this.mapValueSelected.emit(i)
            events.changed(id('Map'))
        })
        this.list.dismissed.on(() => this.closeMapClicked.emit())

        // TYPE: < field >
        const typeLeft = new PixelButton(doc, { x: 41, y: 50, w: 14, h: 14 }, '<', `LFO ${lfo} type: previous`, SHADOW.dark)
        this.type = new PixelField(doc, { x: 57, y: 51, w: 59, h: 14 }, 59, 10)
        const typeRight = new PixelButton(doc, { x: 117, y: 50, w: 14, h: 14 }, '>', `LFO ${lfo} type: next`, SHADOW.dark)
        typeLeft.clicked.on(() => {
            this.leftTypeClicked.emit()
            events.changed(id('Type'))
        })
        typeRight.clicked.on(() => {
            this.rightTypeClicked.emit()
            events.changed(id('Type'))
        })

        // OFFSET slider
        this.offset = new PixelSlider(doc, 56, 75, 76, { label: `LFO ${lfo} phase offset` })
        this.offset.edited.on((edit) => {
            this.offsetChanged.emit(edit)
            events.changed(id('Offset'))
        })

        // RETRIG, SYNC toggles
        this.retrig = new PixelButton(doc, { x: 7, y: 86, w: 52, h: 16 }, 'RETRIG', `LFO ${lfo} retrigger`, SHADOW.dark)
        this.sync = new PixelButton(doc, { x: 96, y: 86, w: 34, h: 16 }, 'SYNC', `LFO ${lfo} sync to BPM`, SHADOW.dark)
        this.retrig.clicked.on(() => {
            this.retrigClicked.emit()
            events.changed(id('Retrig'))
        })
        this.sync.clicked.on(() => {
            this.syncClicked.emit()
            events.changed(id('Sync'))
        })

        events.watch(this.map.el, id('Map'))
        events.watch(this.mapButton.el, id('Map'))
        for (const e of [typeLeft.el, this.type.el, typeRight.el]) events.watch(e, id('Type'))
        events.watch(this.offset.el, id('Offset'))
        events.watch(this.retrig.el, id('Retrig'))
        events.watch(this.sync.el, id('Sync'))

        page.append(
            label(doc, 'MAP', 19, 32, 12, true),
            this.map.el,
            this.mapButton.el,
            label(doc, 'TYPE', 22, 51, 12, true),
            typeLeft.el,
            this.type.el,
            typeRight.el,
            label(doc, 'OFFSET', 7, 70),
            this.offset.el,
            this.retrig.el,
            this.sync.el,
            this.list.el,
        )
    }

    openMap(): void {
        this.listOpen = true
        this.list.show()
    }
    hideMap(): void {
        this.listOpen = false
        this.list.hide()
    }
    setMapValue(index: number): void {
        this.list.setSelected(index)
    }
    setMapList(items: string[]): void {
        this.list.setItems(items)
    }
    setMapButtonOpen(): void {
        this.mapButton.el.classList.add('bsb-open')
    }
    setMapButtonClose(): void {
        this.mapButton.el.classList.remove('bsb-open')
    }

    getMap(): string {
        return this.map.value
    }
    setMap(text: string): void {
        this.map.value = text
    }
    getType(): string {
        return this.type.value
    }
    setType(text: string): void {
        this.type.value = text
    }
    getOffset(): number {
        return this.offset.value
    }
    setOffset(value: number): void {
        this.offset.value = value
    }
    getRetrig(): boolean {
        return this.retrig.isPressed()
    }
    setRetrig(on: boolean): void {
        this.retrig.setPressed(on)
    }
    getSync(): boolean {
        return this.sync.isPressed()
    }
    setSync(on: boolean): void {
        this.sync.setPressed(on)
    }

    dispose(): void {
        this.list.dispose()
    }
}
