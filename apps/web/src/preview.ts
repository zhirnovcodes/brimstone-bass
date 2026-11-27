import { DEFAULT_FX_ORDER, type FxOrder, PARAMS, toNormalized, toPlain } from './params'
import { browserPresets } from './presets'
import { buildState, readState } from './state'
import { mountSkin, type SkinBridge } from './ui/skin'
import skinCss from './ui/skin.css?inline'

// Skin preview (pnpm dev → http://localhost:5280/preview.html): the UI alone, on an in-memory bridge. No audio.
// Query: ?screen=0..7 opens a screen; &open=1 opens its drop-down; &hover=<param id> shows that output line.

const values = new Map(PARAMS.map((p) => [p.id, toNormalized(p, p.default)]))
const orders: [FxOrder, FxOrder] = [[...DEFAULT_FX_ORDER], [...DEFAULT_FX_ORDER]]
const paramListeners = new Set<(id: string, n: number) => void>()
const orderListeners = new Set<(osc: 0 | 1, order: FxOrder) => void>()
let presetName = ''
const nameListeners = new Set<(name: string) => void>()
const setName = (name: string) => {
    presetName = name
    for (const l of nameListeners) l(name)
}

const bridge: SkinBridge = {
    value: (id) => values.get(id) ?? 0,
    setParam: (id, n) => {
        if (values.get(id) === n) return
        values.set(id, n)
        for (const l of paramListeners) l(id, n)
    },
    onParam: (l) => {
        paramListeners.add(l)
        return () => paramListeners.delete(l)
    },
    fxOrder: (osc) => [...orders[osc]] as FxOrder,
    setFxOrder: (osc, order) => {
        orders[osc] = [...order] as FxOrder
        for (const l of orderListeners) l(osc, orders[osc])
    },
    onFxOrder: (l) => {
        orderListeners.add(l)
        return () => orderListeners.delete(l)
    },
    saveState: () => buildState(PARAMS.map((p) => toPlain(p, values.get(p.id)!)), orders, presetName),
    loadState: (data) => {
        const saved = readState(data)
        PARAMS.forEach((p, i) => bridge.setParam(p.id, toNormalized(p, saved.values[i])))
        saved.fxOrder.forEach((order, osc) => bridge.setFxOrder(osc as 0 | 1, order))
        setName(saved.name)
    },
    // The dev server's presets folder.
    presets: browserPresets({
        doc: document,
        fetchText: async (path) => {
            const response = await fetch(`/presets/${path}`, { cache: 'no-store' })
            return response.ok ? response.text() : null
        },
        saveState: () => bridge.saveState(),
        loadState: (data) => bridge.loadState(data),
        name: () => presetName,
        setName,
        onName: (l) => {
            nameListeners.add(l)
            return () => nameListeners.delete(l)
        },
    }),
}

const style = document.createElement('style')
style.textContent = skinCss
document.head.append(style)
mountSkin(document.getElementById('plugin')!, bridge)

const query = new URLSearchParams(location.search)
const next = document.querySelector<HTMLButtonElement>('[aria-label="Next screen"]')!
for (let i = 0; i < Number(query.get('screen') ?? 0); i++) next.click()
if (query.get('open')) {
    document.querySelector<HTMLElement>('.bsb-page:not([hidden]) .bsb-btn:has(.bsb-arrow-down)')?.click()
}
const hover = query.get('hover')
if (hover) {
    document.querySelector(`[data-param="${hover}"]`)?.dispatchEvent(new PointerEvent('pointerenter'))
}
