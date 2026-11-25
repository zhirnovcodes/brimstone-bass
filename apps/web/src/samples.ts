import raw from '../../../samples/samples.json'

// The built-in samples. Their files ship in the plugin folder under samples/.
// The order is the osc sample choice order (params.json osc1Sample / osc2Sample).
export interface SampleInfo {
    id: string
    label: string
    file: string
    rootNote: number // the MIDI note the sample plays as recorded
    // Seconds of the recording (see core/domain/SamplePlayer.h):
    attack: number // [0, attack): played once at the note's start
    loop: number // [attack, attack + loop): repeats while the note sounds; 0 = no loop, plays through
    release: number // the file's last `release` seconds: the tail, played as the note dies; 0 = no tail
    crossfade?: number // seconds of overlap at the loop seam; missing = the core's default (0.1 s)
}

export const SAMPLES = raw.samples as SampleInfo[]
