import { DEFAULT_FX_ORDER, type FxOrder, INDEX, PARAMS, toNormalized, toPlain } from '../../params'
import { Emitter, type Event } from '../core/event'
import {
    cloneEngineData,
    type EngineData,
    FX_ORDER_INDEX,
    type GroupKey,
    type LowerLevelData,
    type MappingData,
    type MixData,
    type OscillatorsData,
    PARAM_PATHS,
    readPlain,
    type TopLevelData,
    writePlain,
} from '../data/engineData'
import { paramOutput } from '../data/paramText'
import type { StateData } from '../../state'
import type { SkinBridge } from '../skin'

/**
 * 1 - Engine data model: the decorator over EngineData. The engine (through SkinBridge) is the source of truth:
 * setters write to the bridge, the bridge echoes every change (ours, automation, MIDI, LFO, setState) back,
 * and only then does the data change and `changed` fire. So views never drift from what plays.
 */
export class EngineDataModel {
    private data: EngineData
    private readonly changedEmitter = new Emitter<number>()
    private readonly offs: (() => void)[] = []

    /** A value changed, from any source. The payload is the UI index (params.json index, or FX_ORDER_INDEX). */
    readonly changed: Event<number> = this.changedEmitter

    constructor(private readonly bridge: SkinBridge) {
        this.data = readAll(bridge)
        this.offs.push(
            bridge.onParam((id, normalized) => {
                const index = INDEX.get(id)
                if (index === undefined) return
                writePlain(this.data, PARAM_PATHS[id], toPlain(PARAMS[index], normalized))
                this.changedEmitter.emit(index)
            }),
            bridge.onFxOrder((osc, order) => {
                this.data.oscillators[osc === 0 ? 'osc1' : 'osc2'].fxOrder = [...order] as FxOrder
                this.changedEmitter.emit(FX_ORDER_INDEX[osc])
            }),
        )
    }

    /** The whole sound in the shared state format: the same JSON the web host and the VST save. */
    save(): StateData {
        return this.bridge.saveState()
    }

    /** Loads a saved sound (shared state format). Held notes release; the changes echo back as `changed`. */
    load(state: unknown): void {
        this.bridge.loadState(state)
    }

    /** A copy of every value as nested engine data. */
    getData(): EngineData {
        return cloneEngineData(this.data)
    }

    /** The output line for UI index `index`: "<OUTPUT> <VALUE>", caps, ≤ 20 chars. */
    getParamOutput(index: number): string {
        return paramOutput(index, this.data)
    }

    getTopLevelData(): TopLevelData {
        return structuredClone(this.data.topLevel)
    }
    setTopLevelData(value: TopLevelData): void {
        this.apply('topLevel', { ...this.data, topLevel: value })
    }

    getLowerLevelData(): LowerLevelData {
        return structuredClone(this.data.lowerLevel)
    }
    setLowerLevelData(value: LowerLevelData): void {
        this.apply('lowerLevel', { ...this.data, lowerLevel: value })
    }

    getMappingData(): MappingData {
        return structuredClone(this.data.mapping)
    }
    setMappingData(value: MappingData): void {
        this.apply('mapping', { ...this.data, mapping: value })
    }

    getOscillatorsData(): OscillatorsData {
        return structuredClone(this.data.oscillators)
    }
    setOscillatorsData(value: OscillatorsData): void {
        this.apply('oscillators', { ...this.data, oscillators: value })
    }

    getMixData(): MixData {
        return structuredClone(this.data.mix)
    }
    setMixData(value: MixData): void {
        this.apply('mix', { ...this.data, mix: value })
    }

    /** Knob drags: the host records one automation gesture between begin and end. */
    beginGesture(index: number): void {
        this.gesture(index, 'begin')
    }
    endGesture(index: number): void {
        this.gesture(index, 'end')
    }

    dispose(): void {
        for (const off of this.offs.splice(0)) off()
        this.changedEmitter.clear()
    }

    private gesture(index: number, gesture: 'begin' | 'end'): void {
        const p = PARAMS[index]
        if (p) {
            this.bridge.setParam(p.id, this.bridge.value(p.id), gesture)
        }
    }

    /** Writes the params of `group` whose value in `next` differs from the current data. */
    private apply(group: GroupKey, next: EngineData): void {
        for (const p of PARAMS) {
            const path = PARAM_PATHS[p.id]
            if (path[0] !== group) continue
            const plain = readPlain(next, path)
            if (plain !== readPlain(this.data, path)) {
                this.bridge.setParam(p.id, toNormalized(p, plain))
            }
        }
        if (group === 'oscillators') {
            ;(['osc1', 'osc2'] as const).forEach((key, osc) => {
                const order = next.oscillators[key].fxOrder
                if (order.join() !== this.data.oscillators[key].fxOrder.join()) {
                    this.bridge.setFxOrder(osc as 0 | 1, [...order] as FxOrder)
                }
            })
        }
    }
}

function readAll(bridge: SkinBridge): EngineData {
    const data: EngineData = {
        topLevel: { osc1: fx(), osc2: fx(), oscMix: 0 },
        lowerLevel: { lfo1: { amp: 0, time: 0 }, lfo2: { amp: 0, time: 0 }, lfo3: { amp: 0, time: 0 } },
        mapping: { lfo1: lfo(), lfo2: lfo(), lfo3: lfo() },
        oscillators: { osc1: osc(), osc2: osc() },
        mix: { unisonValue: 0, unisonVoices: 0, mixType: 0 },
    }
    for (const p of PARAMS) {
        writePlain(data, PARAM_PATHS[p.id], toPlain(p, bridge.value(p.id)))
    }
    data.oscillators.osc1.fxOrder = [...bridge.fxOrder(0)] as FxOrder
    data.oscillators.osc2.fxOrder = [...bridge.fxOrder(1)] as FxOrder
    return data
}

const fx = () => ({ filterDw: 0, filterPos: 0, reverbDw: 0, reverbFeedback: 0, delayDw: 0, delayTime: 0, vol: 0, pan: 0, detune: 0 })
const lfo = () => ({ type: 0, map: 0, sync: false, retrig: false, offset: 0 })
const osc = () => ({ on: false, sample: 0, oct: 0, attack: 0, decay: 0, sustain: 0, release: 0, fxOrder: [...DEFAULT_FX_ORDER] as FxOrder })
