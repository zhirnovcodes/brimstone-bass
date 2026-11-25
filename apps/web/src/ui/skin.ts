import type { FxOrder } from '../params'
import type { StateData } from '../state'
import { createUi } from './app'
import deviceSvg from './assets/device.svg?raw'
import { BrimstoneDomView } from './dom/brimstoneDom'
import { DomEvents } from './dom/domEvents'
import { FrameDomView } from './dom/frameDom'
import { LfoDomView } from './dom/lfoDom'
import { MainDomView } from './dom/mainDom'
import { MixDomView } from './dom/mixDom'
import { OscDomView } from './dom/oscDom'
import { Screen } from './data/uiData'

// The skin talks only to this bridge. The IvanDAW adapter implements it with direct calls; the JUCE editor
// (1.0.0) implements it over the WebView. The skin never knows which host runs it.
export interface SkinBridge {
    /** normalized 0..1 */
    value(id: string): number
    setParam(id: string, normalized: number, gesture?: 'begin' | 'end'): void
    /** Any parameter change, from any source. Returns an unsubscribe. */
    onParam(listener: (id: string, normalized: number) => void): () => void
    fxOrder(osc: 0 | 1): FxOrder
    setFxOrder(osc: 0 | 1, order: FxOrder): void
    onFxOrder(listener: (osc: 0 | 1, order: FxOrder) => void): () => void
    /** The whole sound in the shared state format (same JSON as the host's saved state). */
    saveState(): StateData
    /** Loads a saved sound: held notes release, every value and both effects orders change (and echo back). */
    loadState(data: unknown): void
}

/** The device at Figma size (260 x 306). `scale` would scale it as a whole; 1 = as designed. */
export const DEVICE = { width: 260, height: 306, scale: 1 } as const // Figma size, 100 %

// Keys a focused control uses. Everything else (space = play) reaches the DAW.
const CONTROL_KEYS = new Set(['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown', 'Home', 'End', 'PageUp', 'PageDown', 'Enter'])

export interface Skin {
    root: HTMLElement
    destroy(): void
}

export function mountSkin(container: HTMLElement, bridge: SkinBridge): Skin {
    const doc = container.ownerDocument
    const root = doc.createElement('div')
    root.className = 'bsb-root'
    root.addEventListener('keydown', (event) => {
        if (CONTROL_KEYS.has(event.key)) event.stopPropagation()
    })

    const device = doc.createElement('div')
    device.className = 'bsb-device'
    device.innerHTML = deviceSvg
    const svg = device.querySelector('svg')!
    svg.classList.add('bsb-device-svg')

    const events = new DomEvents()
    const frame = new FrameDomView(svg, events)
    const main = new MainDomView(doc)
    const osc = [new OscDomView(doc, main.pages[Screen.Osc1], events, 1), new OscDomView(doc, main.pages[Screen.Osc2], events, 2)] as const
    const lfo = [
        new LfoDomView(doc, main.pages[Screen.Lfo1], events, 1),
        new LfoDomView(doc, main.pages[Screen.Lfo2], events, 2),
        new LfoDomView(doc, main.pages[Screen.Lfo3], events, 3),
    ] as const
    const mix = new MixDomView(doc, main.pages[Screen.Mix], events)
    const brimstone = new BrimstoneDomView(doc, main.pages[Screen.Brimstone])
    device.append(main.el)
    root.append(device)
    container.append(root)

    const app = createUi(bridge, { events, main, frame, brimstone, osc: [...osc], lfo: [...lfo], mix })

    return {
        root,
        destroy() {
            app.dispose()
            for (const v of [...osc, ...lfo]) v.dispose()
            root.remove()
        },
    }
}
