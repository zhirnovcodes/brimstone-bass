import type { ParameterInfo } from 'ivandaw-core'
import raw from '../../../params.json'

// params.json is the one parameter list. Array index = C ABI index = Engine::setParam index.
// `lfo` and `resets` are Brimstone's own fields: an LFO may target it / changing it resets that oscillator's effects.
export type ParamDef = Omit<ParameterInfo, 'format'> & { lfo?: boolean; resets?: 1 | 2 }

export const PARAMS = raw.params as unknown as ParamDef[]
export const INDEX = new Map(PARAMS.map((p, i) => [p.id, i]))

/** The parameters an LFO can be mapped to, in params.json order: the LFO map choices. */
export const LFO_TARGETS = PARAMS.filter((p) => p.lfo === true)

export function param(id: string): ParamDef {
    const index = INDEX.get(id)
    if (index === undefined) {
        throw new Error(`unknown parameter ${id}`)
    }
    return PARAMS[index]
}

/** normalized 0..1 → plain min..max. Same function as the engine and the JUCE side. */
export function toPlain(p: ParamDef, n: number): number {
    const c = Math.min(1, Math.max(0, Number.isFinite(n) ? n : 0))
    let v = p.scale === 'log' ? p.min * (p.max / p.min) ** c : p.min + c * (p.max - p.min)
    if (p.kind === 'int' || p.kind === 'choice' || p.kind === 'boolean') {
        v = Math.round(v)
    }
    return v
}

/** plain → normalized 0..1. A one-value range (one sample, say) is 0. */
export function toNormalized(p: ParamDef, v: number): number {
    if (p.max === p.min) {
        return 0
    }
    const n = p.scale === 'log' ? Math.log(v / p.min) / Math.log(p.max / p.min) : (v - p.min) / (p.max - p.min)
    return Math.min(1, Math.max(0, n))
}

export function clampPlain(p: ParamDef, v: number): number {
    const c = Math.min(p.max, Math.max(p.min, Number.isFinite(v) ? v : p.default))
    return p.kind === 'float' ? c : Math.round(c)
}

/** Effect indexes in an effects-order array. */
export const EFFECTS = ['Delay', 'Reverb', 'Filter'] as const
export type FxOrder = [number, number, number]
export const DEFAULT_FX_ORDER: FxOrder = [2, 0, 1] // filter → delay → reverb

export function isFxOrder(value: unknown): value is FxOrder {
    if (!Array.isArray(value) || value.length !== 3) {
        return false
    }
    const seen = new Set<number>()
    for (const x of value) {
        if (!Number.isInteger(x) || x < 0 || x > 2 || seen.has(x)) {
            return false
        }
        seen.add(x)
    }
    return true
}
