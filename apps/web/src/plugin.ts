import type {
    HostContext,
    ParameterChange,
    ParameterInfo,
    PluginEvent,
    PluginFactory,
    PluginInstance,
    PluginState,
    PortHandle,
    PortInfo,
    TransportState,
} from 'ivandaw-core'
import { formatValue } from './display'
import { ID, manifest, VERSION } from './manifest'
import { clampPlain, DEFAULT_FX_ORDER, type FxOrder, INDEX, isFxOrder, PARAMS, toNormalized, toPlain } from './params'
import { browserPresets, cleanPresetName } from './presets'
import { SAMPLES } from './samples'
import { buildState, readState, type StateData } from './state'
import { mountSkin, type Skin, type SkinBridge } from './ui/skin'
import skinCss from './ui/skin.css?inline'

// Brimstone Bass for IvanDAW: the Plugin API v2 entry.
//
// Main thread → worklet messages (hosts/web/audio-thread/processor.ts):
//   init {wasm, seed} · sample {index, left, right, rate, rootNote, attack, loop, release, crossfade} · param {index, value}
//   fxOrder {osc, order} · midi {time, status, d1, d2} · transport {time, playing, bpm, beat, numerator, denominator}
//   allOff {time} · dispose
// Worklet → main thread: ready · sampleLoaded {index} · meter {peak, voices} · error {message}

export interface Meter {
    peak: number
    voices: number
}

/** Not part of Plugin API v2: what the localhost dev page's preset panel uses. */
export interface DevPresets {
    /** The presets folder changed: the PRESETS screen reloads its list. */
    changed(): void
    /** The sound was saved as this preset: it becomes the current preset name. */
    setName(name: string): void
    name(): string
    onName(listener: (name: string) => void): () => void
}

// One style element per document, shared by every instance.
const styles = new Map<Document, { element: HTMLStyleElement; users: number }>()

function holdStyles(doc: Document): () => void {
    let entry = styles.get(doc)
    if (!entry) {
        const element = doc.createElement('style')
        element.dataset.brimstoneBass = ''
        element.textContent = skinCss
        doc.head.append(element)
        entry = { element, users: 0 }
        styles.set(doc, entry)
    }
    entry.users++
    const held = entry
    return () => {
        held.users--
        if (held.users === 0) {
            held.element.remove()
            styles.delete(doc)
        }
    }
}

function portInfo<K extends PortInfo['kind']>(id: string, kind: K): PortInfo & { kind: K } {
    const info = manifest.ports.find((port) => port.id === id && port.kind === kind)
    if (!info) {
        throw new Error(`manifest has no ${kind} port ${id}`)
    }
    return info as PortInfo & { kind: K }
}

async function fetchBytes(url: string): Promise<ArrayBuffer> {
    const response = await fetch(url)
    if (!response.ok) {
        throw new Error(`could not load ${url} (${response.status})`)
    }
    return response.arrayBuffer()
}

const factory: PluginFactory = {
    manifest,

    async create(host: HostContext, container: HTMLElement | null): Promise<PluginInstance> {
        const ctx = host.audioContext
        await ctx.audioWorklet.addModule(host.assetUrl('processor.js'))
        const node = new AudioWorkletNode(ctx, 'brimstone-bass', {
            numberOfInputs: 0,
            numberOfOutputs: 1,
            outputChannelCount: [2],
        })
        const post = (message: unknown, transfer: Transferable[] = []) => node.port.postMessage(message, transfer)

        const meterListeners = new Set<(meter: Meter) => void>()
        let lastError: string | null = null
        const ready = new Promise<void>((resolve, reject) => {
            node.port.onmessage = (event: MessageEvent) => {
                const message = event.data
                if (message.type === 'ready') {
                    resolve()
                } else if (message.type === 'error') {
                    lastError = String(message.message)
                    reject(new Error(lastError))
                    host.notify('error', `Brimstone Bass: ${lastError}`)
                } else if (message.type === 'meter') {
                    for (const listener of meterListeners) {
                        listener({ peak: message.peak, voices: message.voices })
                    }
                }
            }
        })

        // Awaited below; this only keeps an early worklet error from showing as unhandled meanwhile.
        ready.catch(() => {})
        try {
            const wasm = await fetchBytes(host.assetUrl('brimstone-bass.wasm'))
            post({ type: 'init', wasm, seed: (Math.random() * 2 ** 32) >>> 0 }, [wasm])
            // The built-in samples, decoded at the context's rate (decodeAudioData resamples).
            const decoded = await Promise.all(
                SAMPLES.map(async (sample) => ctx.decodeAudioData(await fetchBytes(host.assetUrl('samples/' + sample.file)))),
            )
            await ready
            decoded.forEach((buffer, index) => {
                const info = SAMPLES[index]
                const left = buffer.getChannelData(0).slice()
                const right = buffer.numberOfChannels > 1 ? buffer.getChannelData(1).slice() : null
                post(
                    {
                        type: 'sample',
                        index,
                        left,
                        right,
                        rate: buffer.sampleRate,
                        rootNote: info.rootNote,
                        attack: info.attack,
                        loop: info.loop,
                        release: info.release,
                        crossfade: info.crossfade ?? 0,
                    },
                    right ? [left.buffer, right.buffer] : [left.buffer],
                )
            })
        } catch (error) {
            post({ type: 'dispose' })
            node.disconnect()
            throw error
        }

        // Plain values, index = params.json index.
        const values = PARAMS.map((p) => p.default)
        const fxOrder: [FxOrder, FxOrder] = [[...DEFAULT_FX_ORDER], [...DEFAULT_FX_ORDER]]
        PARAMS.forEach((_, index) => post({ type: 'param', index, value: values[index] }))
        fxOrder.forEach((order, osc) => post({ type: 'fxOrder', osc, order }))

        const changeListeners = new Set<(change: ParameterChange) => void>()
        const uiParamListeners = new Set<(id: string, normalized: number) => void>()
        const uiOrderListeners = new Set<(osc: 0 | 1, order: FxOrder) => void>()

        const valueOf = (id: string) => {
            const index = INDEX.get(id)
            return index === undefined ? 0 : values[index]
        }

        // Sets a plain value and tells everyone who listens.
        const apply = (index: number, plain: number, source: ParameterChange['source'], gesture?: 'begin' | 'end') => {
            const p = PARAMS[index]
            const value = clampPlain(p, plain)
            const changed = value !== values[index]
            values[index] = value
            if (changed) {
                post({ type: 'param', index, value })
            }
            const normalized = toNormalized(p, value)
            if (changed || gesture) {
                for (const listener of changeListeners) {
                    listener({ id: p.id, value: normalized, gesture, source })
                }
            }
            if (changed) {
                for (const listener of uiParamListeners) {
                    listener(p.id, normalized)
                }
            }
        }

        const applyFxOrder = (osc: 0 | 1, order: FxOrder) => {
            if (!isFxOrder(order)) {
                return
            }
            fxOrder[osc] = [...order]
            post({ type: 'fxOrder', osc, order: fxOrder[osc] })
            for (const listener of uiOrderListeners) {
                listener(osc, fxOrder[osc])
            }
        }

        // The transport the engine follows. A tempo event keeps the beat running at the new BPM.
        let last: { time: number; state: TransportState } | null = null
        const transport = (time: number, state: TransportState) => {
            if (!(state.bpm > 0) || !Number.isFinite(state.beat)) {
                return
            }
            last = { time, state }
            post({
                type: 'transport',
                time,
                playing: state.playing,
                bpm: state.bpm,
                beat: state.beat,
                numerator: state.numerator,
                denominator: state.denominator,
            })
        }
        // An instance added while the project already plays.
        transport(ctx.currentTime, host.transport())

        const play = (event: PluginEvent) => {
            switch (event.type) {
                case 'midi':
                    post({ type: 'midi', time: event.time, status: event.bytes[0], d1: event.bytes[1], d2: event.bytes[2] })
                    break
                case 'automation': {
                    // Effects order is not a parameter, so no automation can reach it. Unknown ids: ignored.
                    const index = INDEX.get(event.id)
                    if (index !== undefined) {
                        apply(index, toPlain(PARAMS[index], event.value), event.source === 'binding' ? 'binding' : 'host')
                    }
                    break
                }
                case 'tempo': {
                    const base = last ?? { time: event.time, state: host.transport() }
                    const beat = base.state.beat + ((event.time - base.time) * base.state.bpm) / 60
                    transport(event.time, { ...base.state, bpm: event.bpm, beat })
                    break
                }
                case 'transport':
                    transport(event.time, event.state)
                    break
            }
        }

        // The preset the sound came from ('' = none). Saved with the state.
        let presetName = ''
        const nameListeners = new Set<(name: string) => void>()
        const setPresetName = (name: string) => {
            const clean = cleanPresetName(name)
            if (clean === presetName) return
            presetName = clean
            for (const listener of nameListeners) listener(clean)
        }

        // Save / load: the same code for the host's getState / setState, the UI's EngineDataModel and presets.
        const saveState = (): StateData => buildState(values, fxOrder, presetName)
        const loadState = (data: unknown) => {
            const saved = readState(data)
            // Held notes release first, then the saved sound loads.
            post({ type: 'allOff', time: 0 })
            saved.values.forEach((v, index) => apply(index, v, 'internal'))
            saved.fxOrder.forEach((order, osc) => applyFxOrder(osc as 0 | 1, order))
            setPresetName(saved.name)
        }

        // The plugin's presets: presets/ in the plugin folder (presets/index.json lists them).
        const presets = browserPresets({
            doc: container?.ownerDocument ?? document,
            fetchText: async (path) => {
                try {
                    const response = await fetch(host.assetUrl('presets/' + path), { cache: 'no-store' })
                    return response.ok ? await response.text() : null
                } catch {
                    return null
                }
            },
            saveState,
            loadState: (data) => {
                loadState(data)
                host.markDirty()
            },
            name: () => presetName,
            setName: (name) => {
                setPresetName(name)
                host.markDirty()
            },
            onName: (listener) => {
                nameListeners.add(listener)
                return () => nameListeners.delete(listener)
            },
        })

        const bridge: SkinBridge = {
            value: (id) => {
                const index = INDEX.get(id)
                return index === undefined ? 0 : toNormalized(PARAMS[index], values[index])
            },
            setParam: (id, normalized, gesture) => {
                const index = INDEX.get(id)
                if (index === undefined) {
                    return
                }
                apply(index, toPlain(PARAMS[index], normalized), 'ui', gesture)
                host.markDirty()
            },
            onParam: (listener) => {
                uiParamListeners.add(listener)
                return () => uiParamListeners.delete(listener)
            },
            fxOrder: (osc) => [...fxOrder[osc]] as FxOrder,
            setFxOrder: (osc, order) => {
                applyFxOrder(osc, order)
                host.markDirty()
            },
            onFxOrder: (listener) => {
                uiOrderListeners.add(listener)
                return () => uiOrderListeners.delete(listener)
            },
            saveState,
            loadState: (data) => {
                loadState(data)
                host.markDirty()
            },
            presets,
        }

        const infos: ParameterInfo[] = PARAMS.map((p) => {
            const { lfo: _lfo, resets: _resets, ...info } = p
            return { ...info, format: (value: number) => formatValue(p, value, valueOf) }
        })

        let skin: Skin | null = null
        let releaseStyles: (() => void) | null = null
        const mount = (target: HTMLElement) => {
            unmount()
            releaseStyles = holdStyles(target.ownerDocument)
            skin = mountSkin(target, bridge)
        }
        const unmount = () => {
            skin?.destroy()
            skin = null
            releaseStyles?.()
            releaseStyles = null
        }
        if (container) {
            mount(container)
        }

        const ports = new Map<string, PortHandle>([
            ['in', { info: portInfo('in', 'events') }],
            ['out', { info: portInfo('out', 'audio'), node, index: 0 }],
        ])

        const instance: PluginInstance & { meter(listener: (meter: Meter) => void): () => void; presets: DevPresets } = {
            ports,
            latency: 0,
            // Longest release (8 s) plus a long reverb; the host keeps the node running this long after the last note.
            tail: 12,
            parameters: () => infos,
            parameter: (id) => bridge.value(id),
            schedule: (events) => {
                for (const event of events) {
                    play(event)
                }
            },
            // An instrument sends no events.
            listen: () => () => {},
            subscribe: (listener) => {
                changeListeners.add(listener)
                return () => {
                    changeListeners.delete(listener)
                }
            },
            getState: async (): Promise<PluginState> => ({ pluginId: ID, pluginVersion: VERSION, data: saveState() }),
            setState: async (state: PluginState) => {
                if (state.pluginId !== ID) {
                    throw new Error(`state is for ${state.pluginId}, not ${ID}`)
                }
                loadState(state.data)
            },
            mount,
            unmount,
            suspend: () => {},
            resume: () => {},
            dispose: () => {
                unmount()
                changeListeners.clear()
                uiParamListeners.clear()
                uiOrderListeners.clear()
                meterListeners.clear()
                nameListeners.clear()
                post({ type: 'dispose' })
                node.port.onmessage = null
                node.disconnect()
            },
            // Not part of Plugin API v2: the dev page's level meter.
            meter: (listener) => {
                meterListeners.add(listener)
                return () => meterListeners.delete(listener)
            },
            // Not part of Plugin API v2: the dev page's preset panel.
            presets: {
                changed: () => presets.changed(),
                setName: (name) => {
                    setPresetName(name)
                    host.markDirty()
                },
                name: () => presetName,
                onName: (listener) => presets.onName(listener),
            },
        }
        return instance
    },
}

export default factory
