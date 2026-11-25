/** A continuous control moved (knob or slider). `value` is the control position 0..1. */
export interface Edit {
    value: number
    /** 'begin' on grab, 'end' on release: the host records one automation gesture in between. */
    gesture?: 'begin' | 'end'
}

/** A frame knob moved: `index` is the params.json index. */
export interface KnobEdit extends Edit {
    index: number
}
