import raw from '../../../samples/samples.json'

// The built-in samples. Their files ship in the plugin folder under samples/.
// The order is the osc sample choice order (params.json osc1Sample / osc2Sample).
export interface SampleInfo {
    id: string
    label: string
    file: string
    rootNote: number // the MIDI note the sample plays as recorded
    loopStart: number // seconds: a note starts here...
    loopEnd: number // ...and loops back from here
    crossfade?: number // seconds of overlap at the loop seam; missing = the core's default (0.1 s)
}

export const SAMPLES = raw.samples as SampleInfo[]
