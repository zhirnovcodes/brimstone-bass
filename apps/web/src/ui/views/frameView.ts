import type { Event } from '../core/event'
import type { KnobEdit } from './edit'

/** The 25 knobs on the device frame (groups "Top level" and "Lower level"). Indexes are params.json indexes. */
export interface FrameView {
    /** Knob position 0..1. */
    setKnob(index: number, value: number): void
    readonly knobChanged: Event<KnobEdit>
    /** Double-click: back to the default. */
    readonly knobReset: Event<number>
}
