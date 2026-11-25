import { INDEX } from '../../params'
import { Emitter } from '../core/event'
import type { Edit } from '../views/edit'
import type { MixView } from '../views/mixView'
import type { DomEvents } from './domEvents'
import { label, PixelButton, PixelField, PixelSlider } from './widgets'

/** MIX page. Positions: Figma frame "5", Mix window. */
export class MixDomView implements MixView {
    readonly typeLeftClicked = new Emitter<void>()
    readonly typeRightClicked = new Emitter<void>()
    readonly unisonValueChanged = new Emitter<Edit>()
    readonly voicesLeftClicked = new Emitter<void>()
    readonly voicesRightClicked = new Emitter<void>()

    private readonly type: PixelField
    private readonly unison: PixelSlider
    private readonly voices: PixelField

    constructor(doc: Document, page: HTMLElement, events: DomEvents) {
        const typeIndex = INDEX.get('mixType')!
        const unisonIndex = INDEX.get('unisonValue')!
        const voicesIndex = INDEX.get('unisonVoices')!
        const stepper = (x: number, y: number, text: '<' | '>', aria: string, out: Emitter<void>, index: number) => {
            const b = new PixelButton(doc, { x, y, w: 14, h: 14 }, text, aria)
            b.clicked.on(() => {
                out.emit()
                events.changed(index)
            })
            events.watch(b.el, index)
            return b.el
        }

        this.type = new PixelField(doc, { x: 52, y: 35, w: 62, h: 14 }, 54)
        this.unison = new PixelSlider(doc, 56, 71, 73, { label: 'Unison value' })
        this.unison.edited.on((edit) => {
            this.unisonValueChanged.emit(edit)
            events.changed(unisonIndex)
        })
        this.voices = new PixelField(doc, { x: 73, y: 87, w: 41, h: 14 }, 73, 12, true)
        events.watch(this.type.el, typeIndex)
        events.watch(this.unison.el, unisonIndex)
        events.watch(this.voices.el, voicesIndex)

        page.append(
            label(doc, 'TYPE', 19, 35, 12, true),
            stepper(36, 34, '<', 'Mix type: previous', this.typeLeftClicked, typeIndex),
            this.type.el,
            stepper(115, 34, '>', 'Mix type: next', this.typeRightClicked, typeIndex),
            label(doc, 'UNISON', 4, 67),
            this.unison.el,
            label(doc, 'VOICES', 25, 87, 12, true),
            stepper(56, 86, '<', 'Unison voices: fewer', this.voicesLeftClicked, voicesIndex),
            this.voices.el,
            stepper(115, 86, '>', 'Unison voices: more', this.voicesRightClicked, voicesIndex),
        )
    }

    getType(): string {
        return this.type.value
    }
    setType(text: string): void {
        this.type.value = text
    }
    getUnisonValue(): number {
        return this.unison.value
    }
    setUnisonValue(value: number): void {
        this.unison.value = value
    }
    getVoices(): string {
        return this.voices.value
    }
    setVoices(text: string): void {
        this.voices.value = text
    }
}
