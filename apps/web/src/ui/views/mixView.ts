import type { Event } from '../core/event'
import type { Edit } from './edit'

/** 6 - Mix view: the MIX screen. */
export interface MixView {
    getType(): string
    setType(text: string): void
    /** Slider position 0..1. */
    getUnisonValue(): number
    setUnisonValue(value: number): void
    getVoices(): string
    setVoices(text: string): void

    readonly typeLeftClicked: Event<void>
    readonly typeRightClicked: Event<void>
    readonly unisonValueChanged: Event<Edit>
    readonly voicesLeftClicked: Event<void>
    readonly voicesRightClicked: Event<void>
}
