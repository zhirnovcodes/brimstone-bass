import { type FxOrder, PARAMS } from '../../params'

// 1 - Engine data: the value of every parameter, in plain units (the params.json min..max range),
// nested by params.json "group". Plain data, no behaviour: EngineDataModel owns reading and writing it.

/** Per-oscillator frame knobs (top level): the effect knobs above the screen, vol / pan / detune beside it. */
export interface OscFxData {
    filterDw: number // 0..1
    filterPos: number // 0..1
    reverbDw: number // 0..1
    reverbFeedback: number // 0..1
    delayDw: number // 0..1
    delayTime: number // 0..1 (the engine maps it to 1/16t .. 4)
    vol: number // 0..1.2
    pan: number // -1..1
    detune: number // -24..24 cents
}

/** Group "Top level": the knobs on the upper frame. */
export interface TopLevelData {
    osc1: OscFxData
    osc2: OscFxData
    oscMix: number // 0..1
}

export interface LfoKnobsData {
    amp: number // 0..1
    time: number // 0..1 (seconds or beats, depending on MappingData.lfoN.sync)
}

/** Group "Lower level": the LFO knobs on the lower frame. */
export interface LowerLevelData {
    lfo1: LfoKnobsData
    lfo2: LfoKnobsData
    lfo3: LfoKnobsData
}

export interface LfoMappingData {
    type: number // 0..3: Tri, Square, Sine, Rand
    map: number // index into LFO_TARGETS
    sync: boolean
    retrig: boolean
    offset: number // 0..1 (0..360°)
}

/** Group "Mapping": the LFO screens. */
export interface MappingData {
    lfo1: LfoMappingData
    lfo2: LfoMappingData
    lfo3: LfoMappingData
}

export interface OscData {
    on: boolean
    sample: number // index into SAMPLES
    oct: number // -2..2
    attack: number // 0..1
    decay: number // 0..1 (0.01..8 s)
    sustain: number // 0..1 (level)
    release: number // 0..1
    /** Effects order: state, not a parameter (never automated). Position → effect index (0 delay, 1 reverb, 2 filter). */
    fxOrder: FxOrder
}

/** Group "Oscillators": the OSC screens. */
export interface OscillatorsData {
    osc1: OscData
    osc2: OscData
}

/** Group "Mix": the MIX screen. */
export interface MixData {
    unisonValue: number // 0..1
    unisonVoices: number // 0..4
    mixType: number // 0..2: Linear, Equal-power, Both full
}

export interface EngineData {
    topLevel: TopLevelData
    lowerLevel: LowerLevelData
    mapping: MappingData
    oscillators: OscillatorsData
    mix: MixData
}

export type GroupKey = keyof EngineData

/** Where each parameter lives in EngineData: [group, ...keys]. Every params.json id appears once (engineData.test.ts). */
export const PARAM_PATHS: Record<string, readonly [GroupKey, ...string[]]> = {
    filterDw1: ['topLevel', 'osc1', 'filterDw'],
    filterPos1: ['topLevel', 'osc1', 'filterPos'],
    reverbDw1: ['topLevel', 'osc1', 'reverbDw'],
    reverbFeedback1: ['topLevel', 'osc1', 'reverbFeedback'],
    delayDw1: ['topLevel', 'osc1', 'delayDw'],
    delayTime1: ['topLevel', 'osc1', 'delayTime'],
    filterDw2: ['topLevel', 'osc2', 'filterDw'],
    filterPos2: ['topLevel', 'osc2', 'filterPos'],
    reverbDw2: ['topLevel', 'osc2', 'reverbDw'],
    reverbFeedback2: ['topLevel', 'osc2', 'reverbFeedback'],
    delayDw2: ['topLevel', 'osc2', 'delayDw'],
    delayTime2: ['topLevel', 'osc2', 'delayTime'],
    oscMix: ['topLevel', 'oscMix'],
    lfo1Amp: ['lowerLevel', 'lfo1', 'amp'],
    lfo1Time: ['lowerLevel', 'lfo1', 'time'],
    lfo2Amp: ['lowerLevel', 'lfo2', 'amp'],
    lfo2Time: ['lowerLevel', 'lfo2', 'time'],
    lfo1Type: ['mapping', 'lfo1', 'type'],
    lfo1Map: ['mapping', 'lfo1', 'map'],
    lfo1Sync: ['mapping', 'lfo1', 'sync'],
    lfo1Retrig: ['mapping', 'lfo1', 'retrig'],
    lfo1Offset: ['mapping', 'lfo1', 'offset'],
    lfo2Type: ['mapping', 'lfo2', 'type'],
    lfo2Map: ['mapping', 'lfo2', 'map'],
    lfo2Sync: ['mapping', 'lfo2', 'sync'],
    lfo2Retrig: ['mapping', 'lfo2', 'retrig'],
    lfo2Offset: ['mapping', 'lfo2', 'offset'],
    osc1On: ['oscillators', 'osc1', 'on'],
    osc1Sample: ['oscillators', 'osc1', 'sample'],
    osc1Vol: ['topLevel', 'osc1', 'vol'],
    osc1Pan: ['topLevel', 'osc1', 'pan'],
    osc2On: ['oscillators', 'osc2', 'on'],
    osc2Sample: ['oscillators', 'osc2', 'sample'],
    osc2Vol: ['topLevel', 'osc2', 'vol'],
    osc2Pan: ['topLevel', 'osc2', 'pan'],
    osc1Attack: ['oscillators', 'osc1', 'attack'],
    osc1Release: ['oscillators', 'osc1', 'release'],
    osc2Attack: ['oscillators', 'osc2', 'attack'],
    osc2Release: ['oscillators', 'osc2', 'release'],
    unisonValue: ['mix', 'unisonValue'],
    unisonVoices: ['mix', 'unisonVoices'],
    mixType: ['mix', 'mixType'],
    lfo3Amp: ['lowerLevel', 'lfo3', 'amp'],
    lfo3Time: ['lowerLevel', 'lfo3', 'time'],
    lfo3Type: ['mapping', 'lfo3', 'type'],
    lfo3Map: ['mapping', 'lfo3', 'map'],
    lfo3Sync: ['mapping', 'lfo3', 'sync'],
    lfo3Retrig: ['mapping', 'lfo3', 'retrig'],
    lfo3Offset: ['mapping', 'lfo3', 'offset'],
    osc1Oct: ['oscillators', 'osc1', 'oct'],
    osc2Oct: ['oscillators', 'osc2', 'oct'],
    osc1Detune: ['topLevel', 'osc1', 'detune'],
    osc2Detune: ['topLevel', 'osc2', 'detune'],
    osc1Sustain: ['oscillators', 'osc1', 'sustain'],
    osc1Decay: ['oscillators', 'osc1', 'decay'],
    osc2Sustain: ['oscillators', 'osc2', 'sustain'],
    osc2Decay: ['oscillators', 'osc2', 'decay'],
}

/**
 * Indexes the UI uses for "a thing with an output line": every params.json index, then the two
 * effects orders (table rows 43 and 44). They are not engine parameters.
 */
export const FX_ORDER_INDEX = [PARAMS.length, PARAMS.length + 1] as const

type Leaf = number | boolean
type Node = { [key: string]: Node | Leaf | FxOrder }

/** Plain number (as the engine sees it) at `path`. Booleans read as 0 / 1. */
export function readPlain(data: EngineData, path: readonly string[]): number {
    let node: unknown = data
    for (const key of path) {
        node = (node as Node)[key]
    }
    return typeof node === 'boolean' ? (node ? 1 : 0) : (node as number)
}

/** Writes plain number `plain` at `path`, keeping the field's type (boolean fields get a boolean). */
export function writePlain(data: EngineData, path: readonly string[], plain: number): void {
    let node = data as unknown as Node
    for (const key of path.slice(0, -1)) {
        node = node[key] as Node
    }
    const last = path[path.length - 1]
    node[last] = typeof node[last] === 'boolean' ? plain >= 0.5 : plain
}

export function cloneEngineData(data: EngineData): EngineData {
    return structuredClone(data)
}
