import { attackSeconds, decaySeconds, DELAY_DIVISIONS, DIVISION_LABELS, divisionIndex, LFO_DIVISIONS, lfoSeconds, releaseSeconds, signed } from '../../display'
import { LFO_TARGETS, PARAMS } from '../../params'
import { SAMPLES } from '../../samples'
import { type EngineData, FX_ORDER_INDEX, PARAM_PATHS, readPlain } from './engineData'

// The screen's output line: "<OUTPUT> <VALUE>", all caps, at most OUTPUT_LENGTH characters.
// Text comes from the plugin params table: "Name shorten (8)", "Output (20)", "value format".

export const OUTPUT_LENGTH = 20
export const SHORT_NAME_LENGTH = 8

export type ValueFormat =
    | 'dec 0.1' // 0..1 → "0.2"
    | 'dec -0.1' // -1..1 → "-0.5"
    | 'frac' // beats: "1/16", "1/8T", "4"
    | 'frac / time' // LFO time: "1/16" when synced, else "31.04 S"
    | 'time' // seconds: "0.31 S"
    | 'enum string' // lookup table: "SQUARE"
    | 'bool' // "TRUE" / "FALSE"
    | 'deg 360' // "360"
    | 'int' // "2"
    | 'int+-' // octave: "+2", "-1", "0"
    | 'dt+-' // detune in Hz: "-1000 HZ", "+5 HZ", "0 HZ"
    | 'EFF' // effect button: "FIL. REPLACE"

export interface ParamText {
    /** "Name shorten (8)": how the LFO map list names this parameter. Only LFO targets have one. */
    short?: string
    /** "Output (20)". `null` = the output line shows the value alone ("[value]" in the table). */
    output: string | null
    format: ValueFormat
}

export const PARAM_TEXT: Record<string, ParamText> = {
    filterDw1: { short: 'FIL1 DW', output: 'Fil.1 Dry/Wet', format: 'dec 0.1' },
    filterPos1: { short: 'FIL1 FRQ', output: 'Fil.1 freq.', format: 'dec 0.1' },
    reverbDw1: { short: 'REV1 DW', output: 'Rev.1 Dry/Wet', format: 'dec 0.1' },
    reverbFeedback1: { short: 'REV1 FB', output: 'Rev.1 feed.', format: 'dec 0.1' },
    delayDw1: { short: 'DEL1 DW', output: 'Del.1 Dry/wet', format: 'dec 0.1' },
    delayTime1: { short: 'DEL1 TIM', output: 'Del.1 time', format: 'frac' },
    filterDw2: { short: 'FIL2 DW', output: 'Filter 2 Dry/Wet', format: 'dec 0.1' },
    filterPos2: { short: 'FIL2 FRQ', output: 'Filter 2 freq.', format: 'dec 0.1' },
    reverbDw2: { short: 'REV2 DW', output: 'Reverb 2 Dry/Wet', format: 'dec 0.1' },
    reverbFeedback2: { short: 'REV2 FB', output: 'Reverb 2 feed.', format: 'dec 0.1' },
    delayDw2: { short: 'DEL2 DW', output: 'Delay 2 Dry/wet', format: 'dec 0.1' },
    delayTime2: { short: 'DEL2 TIM', output: 'Delay 2 time', format: 'frac' },
    oscMix: { short: 'OSC MIX', output: 'Osc. Mix', format: 'dec -0.1' },
    lfo1Amp: { short: 'LFO1 AMP', output: 'LFO 1 Amp.', format: 'dec 0.1' },
    lfo1Time: { short: 'LFO1 TIM', output: 'LFO 1 Time', format: 'frac / time' },
    lfo2Amp: { short: 'LFO2 AMP', output: 'LFO 2 Amp.', format: 'dec 0.1' },
    lfo2Time: { short: 'LFO2 TIM', output: 'LFO 2 Time', format: 'frac / time' },
    lfo1Type: { output: 'Type', format: 'enum string' },
    lfo1Map: { output: null, format: 'enum string' },
    lfo1Sync: { output: 'Synch to BPM', format: 'bool' },
    lfo1Retrig: { output: 'Retrig', format: 'bool' },
    lfo1Offset: { short: 'LFO1 OFF', output: 'Phase offset', format: 'deg 360' },
    lfo2Type: { output: 'Type', format: 'enum string' },
    lfo2Map: { output: null, format: 'enum string' },
    lfo2Sync: { output: 'Synch to BPM', format: 'bool' },
    lfo2Retrig: { output: 'Retrig', format: 'bool' },
    lfo2Offset: { short: 'LFO2 OFF', output: 'Phase offset', format: 'deg 360' },
    osc1On: { short: 'OSC1 ON', output: 'On/off', format: 'bool' },
    osc1Sample: { output: 'Sample', format: 'enum string' },
    osc1Vol: { short: 'OSC1 VOL', output: 'OSC1 Volume', format: 'dec 0.1' },
    osc1Pan: { short: 'OSC1 PAN', output: 'OSC1 Pan', format: 'dec -0.1' },
    osc2On: { short: 'OSC2 ON', output: 'On/off', format: 'bool' },
    osc2Sample: { output: 'Sample', format: 'enum string' },
    osc2Vol: { short: 'OSC2 VOL', output: 'OSC2 Volume', format: 'dec 0.1' },
    osc2Pan: { short: 'OSC2 PAN', output: 'OSC2 Pan', format: 'dec -0.1' },
    osc1Attack: { short: 'OSC1 ATT', output: 'Attack', format: 'time' },
    osc1Release: { short: 'OSC1 REL', output: 'Release', format: 'time' },
    osc2Attack: { short: 'OSC2 ATT', output: 'Attack', format: 'time' },
    osc2Release: { short: 'OSC2 REL', output: 'Release', format: 'time' },
    unisonValue: { short: 'UNI VAL', output: 'Unison value', format: 'dec 0.1' },
    unisonVoices: { output: 'Voices', format: 'int' },
    mixType: { output: 'Osc mix', format: 'enum string' },
    lfo3Amp: { short: 'LFO3 AMP', output: 'LFO 3 Amp.', format: 'dec 0.1' },
    lfo3Time: { short: 'LFO3 TIM', output: 'LFO 3 Time', format: 'frac / time' },
    lfo3Type: { output: 'Type', format: 'enum string' },
    lfo3Map: { output: null, format: 'enum string' },
    lfo3Sync: { output: 'Synch to BPM', format: 'bool' },
    lfo3Retrig: { output: 'Retrig', format: 'bool' },
    lfo3Offset: { short: 'LFO3 OFF', output: 'Phase offset', format: 'deg 360' },
    osc1Oct: { short: 'OSC1 OCT', output: 'Octave', format: 'int+-' },
    osc2Oct: { short: 'OSC2 OCT', output: 'Octave', format: 'int+-' },
    osc1Detune: { short: 'OSC1 DT', output: 'OSC1 Detune', format: 'dt+-' },
    osc2Detune: { short: 'OSC2 DT', output: 'OSC2 Detune', format: 'dt+-' },
    osc1Sustain: { short: 'OSC1 SUS', output: 'Sustain', format: 'dec 0.1' },
    osc1Decay: { short: 'OSC1 DEC', output: 'Decay', format: 'time' },
    osc2Sustain: { short: 'OSC2 SUS', output: 'Sustain', format: 'dec 0.1' },
    osc2Decay: { short: 'OSC2 DEC', output: 'Decay', format: 'time' },
}

/** Enum lookup tables ("enum string"). */
export const LFO_TYPE_TEXT = ['TRI', 'SQUARE', 'SINE', 'RAND', 'SAW SOFT', 'SAW', 'SAW INV', 'SOFT INV']
export const MIX_TYPE_TEXT = ['LINEAR', 'EQ. POW.', 'BOTH']
/** "EFF": the effect whose button is pressed (effect index 0 delay, 1 reverb, 2 filter). */
export const FX_REPLACE_TEXT = ['Del. Replace', 'Rev. Replace', 'Fil. Replace']
/** Letters on the effect buttons (effect index 0 delay, 1 reverb, 2 filter). */
export const FX_LETTERS = ['D', 'R', 'F']

/**
 * The LFO map list on screen, sorted by name: row → map choice index. Only the list is sorted; the stored
 * value stays the params.json choice index, so saved sounds and the hosts keep their order.
 */
export function lfoMapOrder(): number[] {
    return LFO_TARGETS.map((_, i) => i).sort((a, b) => lfoTargetShort(a).localeCompare(lfoTargetShort(b), 'en', { numeric: true }))
}

/** The LFO map list entry for map choice `map` (≤ 8 chars). */
export function lfoTargetShort(map: number): string {
    const target = LFO_TARGETS[map]
    return target ? (PARAM_TEXT[target.id]?.short ?? target.id).slice(0, SHORT_NAME_LENGTH) : ''
}

const dec = (v: number) => {
    const text = v.toFixed(1)
    return text === '-0.0' ? '0.0' : text
}

/** The value part of the output line (caps not applied yet). */
export function formatParamValue(id: string, data: EngineData): string {
    const text = PARAM_TEXT[id]
    const v = readPlain(data, PARAM_PATHS[id])
    switch (text.format) {
        case 'dec 0.1':
            return dec(v)
        case 'dec -0.1':
            // Osc mix is stored 0..1 (osc 1 .. osc 2) but reads as -1..1 around the centre.
            return dec(id === 'oscMix' ? v * 2 - 1 : v)
        case 'frac':
            return DIVISION_LABELS[divisionIndex(v, DELAY_DIVISIONS)]
        case 'frac / time': {
            const n = /^lfo(\d)Time$/.exec(id)![1]
            const synced = readPlain(data, PARAM_PATHS[`lfo${n}Sync`]) >= 0.5
            return synced ? DIVISION_LABELS[divisionIndex(v, LFO_DIVISIONS)] : `${lfoSeconds(v).toFixed(2)} s`
        }
        case 'time':
            return `${(/Attack$/.test(id) ? attackSeconds(v) : /Decay$/.test(id) ? decaySeconds(v) : releaseSeconds(v)).toFixed(2)} s`
        case 'bool':
            return v >= 0.5 ? 'true' : 'false'
        case 'deg 360':
            return String(Math.round(v * 360))
        case 'int':
            return String(Math.round(v))
        case 'int+-':
            return signed(v)
        case 'dt+-':
            return `${signed(v)} Hz`
        case 'enum string':
            if (/^lfo\dType$/.test(id)) return LFO_TYPE_TEXT[v] ?? ''
            if (/^lfo\dMap$/.test(id)) return lfoTargetShort(v)
            if (id === 'mixType') return MIX_TYPE_TEXT[v] ?? ''
            if (/^osc\dSample$/.test(id)) return SAMPLES[v]?.label ?? ''
            return String(v)
        case 'EFF':
            return ''
    }
}

/** "<OUTPUT> <VALUE>" in caps, cut to OUTPUT_LENGTH. */
export function outputLine(output: string | null, value: string): string {
    const line = output === null ? value : `${output.trim()} ${value}`
    return line.toUpperCase().slice(0, OUTPUT_LENGTH)
}

/** The output line for UI index `index` (params.json index, or FX_ORDER_INDEX for an effects order). */
export function paramOutput(index: number, data: EngineData): string {
    const fxOsc = FX_ORDER_INDEX.indexOf(index as (typeof FX_ORDER_INDEX)[number])
    if (fxOsc >= 0) {
        // No button pressed: the chain itself, e.g. "F > D > R".
        const order = data.oscillators[fxOsc === 0 ? 'osc1' : 'osc2'].fxOrder
        return outputLine(null, order.map((e) => FX_LETTERS[e]).join(' > '))
    }
    const p = PARAMS[index]
    if (!p) {
        return ''
    }
    return outputLine(PARAM_TEXT[p.id].output, formatParamValue(p.id, data))
}

/** The output line while effect `effect` (0 delay, 1 reverb, 2 filter) waits to be swapped. */
export function fxReplaceOutput(effect: number): string {
    return outputLine(null, FX_REPLACE_TEXT[effect] ?? '')
}
