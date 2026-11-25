import { DEFAULT_FX_ORDER, type FxOrder, isFxOrder, PARAMS } from './params'

// The saved sound. One format for every host: IvanDAW `getState().data`, and (1.0.0) the JUCE
// `getStateInformation` JSON. Changing it needs a version bump and a reader for the old one.

export const STATE_VERSION = 1

export interface StateData {
    v: number
    params: Record<string, number> // plain values by params.json id
    fxOrder: [FxOrder, FxOrder]
}

/** Plain values in params.json order + both effects orders → state. */
export function buildState(values: readonly number[], fxOrder: readonly [FxOrder, FxOrder]): StateData {
    return {
        v: STATE_VERSION,
        params: Object.fromEntries(PARAMS.map((p, index) => [p.id, values[index]])),
        fxOrder: [[...fxOrder[0]], [...fxOrder[1]]],
    }
}

/** State (anything) → plain values in params.json order + effects orders. Unknown ids: ignored. Missing or bad: default. */
export function readState(data: unknown): { values: number[]; fxOrder: [FxOrder, FxOrder] } {
    const d = (data ?? {}) as Partial<StateData>
    const saved = d.params ?? {}
    const values = PARAMS.map((p) => {
        const v = saved[p.id]
        return typeof v === 'number' && Number.isFinite(v) ? v : p.default
    })
    const orders = Array.isArray(d.fxOrder) ? d.fxOrder : []
    const fxOrder = [0, 1].map((osc) => (isFxOrder(orders[osc]) ? [...orders[osc]] : [...DEFAULT_FX_ORDER])) as [FxOrder, FxOrder]
    return { values, fxOrder }
}
