import { DEFAULT_FX_ORDER, type FxOrder, isFxOrder, PARAMS, toNormalized, toPlain } from './params'
import { cleanPresetName, parsePreset, presetNameOf, presetText, sortPresetNames } from './presets'
import { buildState, readState, type StateData } from './state'
import { mountSkin, type SkinBridge } from './ui/skin'
import skinCss from './ui/skin.css?inline'

// The skin inside the JUCE plugin window (VST3 / AU / Standalone), the same one IvanDAW shows. The processor owns
// the sound; this page only draws and sends edits, over window.__JUCE__ (hosts/plugin/PluginEditor.cpp).
//
// page -> plugin (native functions): ready() -> {params: {id: normalized}, fxOrder, name} · setParam(id, normalized, gesture)
//                                   setFxOrder(osc, order) · loadState(data)
//                                   savePresetFile(name, text) -> saved name | '' (a save dialog on the user's computer)
//                                   loadPresetFile() -> {name, text} | null (an open dialog)
// plugin -> page (events):          params {id: normalized} (automation, MIDI learn, state loads) · fxOrder {osc, order}
//                                   presetName name (a state load, a preset saved under a new name)
//
// The plugin's own presets (presets/*.brimstone) are built into this script.

interface Backend {
    addEventListener(eventId: string, listener: (payload: never) => void): unknown
    emitEvent(eventId: string, payload: unknown): void
}

declare global {
    interface Window {
        __JUCE__?: { backend: Backend }
    }
}

interface Snapshot {
    params: Record<string, number>
    fxOrder: unknown[]
    name?: unknown
}

const BUILT_IN: Map<string, string> = new Map(
    Object.entries(import.meta.glob<string>('../../../presets/*.brimstone', { query: '?raw', import: 'default', eager: true })).map(
        ([path, text]) => [presetNameOf(path), text],
    ),
)

const backend = window.__JUCE__?.backend
const pending = new Map<number, (result: unknown) => void>()
let nextCall = 0

function call(name: string, ...params: unknown[]): Promise<unknown> {
    if (!backend) {
        return Promise.reject(new Error('no plugin host'))
    }
    const resultId = nextCall++
    const result = new Promise<unknown>((resolve) => pending.set(resultId, resolve))
    backend.emitEvent('__juce__invoke', { name, params, resultId })
    return result
}

const finite = (n: unknown): n is number => typeof n === 'number' && Number.isFinite(n)
const clamp01 = (n: number) => Math.min(1, Math.max(0, n))

const values = new Map(PARAMS.map((p) => [p.id, toNormalized(p, p.default)]))
const orders: [FxOrder, FxOrder] = [[...DEFAULT_FX_ORDER], [...DEFAULT_FX_ORDER]]
const paramListeners = new Set<(id: string, normalized: number) => void>()
const orderListeners = new Set<(osc: 0 | 1, order: FxOrder) => void>()
const nameListeners = new Set<(name: string) => void>()
let presetName = ''

function showName(name: string) {
    const clean = cleanPresetName(name)
    if (clean === presetName) return
    presetName = clean
    for (const listener of nameListeners) listener(clean)
}

// Sets the page's copy and tells the skin. Returns false when nothing changed.
function show(id: string, normalized: number): boolean {
    if (!values.has(id) || values.get(id) === normalized) {
        return false
    }
    values.set(id, normalized)
    for (const listener of paramListeners) listener(id, normalized)
    return true
}

function showOrder(osc: 0 | 1, order: FxOrder) {
    orders[osc] = [...order] as FxOrder
    for (const listener of orderListeners) listener(osc, orders[osc])
}

const bridge: SkinBridge = {
    value: (id) => values.get(id) ?? 0,
    setParam(id, normalized, gesture) {
        if (!values.has(id) || !finite(normalized)) return
        const n = clamp01(normalized)
        const changed = show(id, n)
        if (changed || gesture) {
            void call('setParam', id, n, gesture ?? '')
        }
    },
    onParam(listener) {
        paramListeners.add(listener)
        return () => paramListeners.delete(listener)
    },
    fxOrder: (osc) => [...orders[osc]] as FxOrder,
    setFxOrder(osc, order) {
        if (!isFxOrder(order)) return
        showOrder(osc, order)
        void call('setFxOrder', osc, [...order])
    },
    onFxOrder(listener) {
        orderListeners.add(listener)
        return () => orderListeners.delete(listener)
    },
    saveState: () => buildState(PARAMS.map((p) => toPlain(p, values.get(p.id)!)), orders, presetName),
    loadState(data) {
        // Shown at once; the plugin reads the same JSON, releases held notes and echoes its values back.
        const saved = readState(data)
        PARAMS.forEach((p, index) => show(p.id, toNormalized(p, saved.values[index])))
        saved.fxOrder.forEach((order, osc) => showOrder(osc as 0 | 1, order))
        showName(saved.name)
        void call('loadState', data)
    },
    presets: {
        list: async () => sortPresetNames([...BUILT_IN.keys()]),
        async open(name) {
            const text = BUILT_IN.get(name)
            if (text === undefined) throw new Error(`no preset ${name}`)
            bridge.loadState(parsePreset(text, name))
        },
        // Built in: the list never changes while the plugin runs.
        onListChanged: () => () => {},
        name: () => presetName,
        onName(listener) {
            nameListeners.add(listener)
            return () => nameListeners.delete(listener)
        },
        async saveFile() {
            const name = presetName || 'Preset'
            const saved = await call('savePresetFile', name, presetText(bridge.saveState(), name))
            if (typeof saved !== 'string' || saved === '') return null
            showName(saved)
            return presetName
        },
        async loadFile() {
            const file = (await call('loadPresetFile')) as { name?: unknown; text?: unknown } | null
            if (!file || typeof file.text !== 'string') return null
            const name = presetNameOf(String(file.name ?? '')) || 'Preset'
            const data: StateData = parsePreset(file.text, name)
            bridge.loadState(data)
            return name
        },
    },
}

function applySnapshot(snapshot: Snapshot) {
    for (const [id, n] of Object.entries(snapshot.params ?? {})) {
        if (finite(n)) show(id, clamp01(n))
    }
    ;(snapshot.fxOrder ?? []).forEach((order, osc) => {
        if ((osc === 0 || osc === 1) && isFxOrder(order)) showOrder(osc, order)
    })
    if (typeof snapshot.name === 'string') showName(snapshot.name)
}

async function start() {
    const target = document.getElementById('plugin')!
    if (!backend) {
        target.textContent = 'Open this page in the Brimstone Bass plugin.'
        return
    }
    backend.addEventListener('__juce__complete', ({ promiseId, result }: { promiseId: number; result: unknown }) => {
        pending.get(promiseId)?.(result)
        pending.delete(promiseId)
    })
    backend.addEventListener('params', (changes: Record<string, number>) => {
        for (const [id, n] of Object.entries(changes)) {
            if (finite(n)) show(id, clamp01(n))
        }
    })
    backend.addEventListener('fxOrder', ({ osc, order }: { osc: number; order: unknown }) => {
        if ((osc === 0 || osc === 1) && isFxOrder(order)) showOrder(osc, order)
    })
    backend.addEventListener('presetName', (name: unknown) => {
        if (typeof name === 'string') showName(name)
    })

    applySnapshot((await call('ready')) as Snapshot)

    const style = document.createElement('style')
    style.textContent = skinCss
    document.head.append(style)
    mountSkin(target, bridge)
}

void start()
