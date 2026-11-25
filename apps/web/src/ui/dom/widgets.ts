import arrowDownSvg from '../assets/figma/arrow-down.svg?raw'
import arrowLeftSvg from '../assets/figma/arrow-left.svg?raw'
import scrollLineSvg from '../assets/figma/scroll-line.svg?raw'
import { Emitter } from '../core/event'
import type { Edit } from '../views/edit'

// The screen's building blocks, at Figma size (the device scales as a whole). Coordinates are px in the
// parent: the 134 x 134 screen for everything on a page.

export interface Box {
    x: number
    y: number
    w?: number
    h?: number
}

/** Shadow colours from the design: header buttons and LFO, OSC / MIX buttons, slider handles. */
export const SHADOW = { dark: '#4A403C', light: '#4A403C', handle: '#687D85' } as const

export function place<T extends HTMLElement | SVGElement>(e: T, b: Box): T {
    e.style.left = `${b.x}px`
    e.style.top = `${b.y}px`
    if (b.w !== undefined) e.style.width = `${b.w}px`
    if (b.h !== undefined) e.style.height = `${b.h}px`
    return e
}

export function div(doc: Document, cls: string, b?: Box, text?: string): HTMLDivElement {
    const e = doc.createElement('div')
    e.className = cls
    if (b) place(e, b)
    if (text !== undefined) e.textContent = text
    return e
}

/** A text label. `centerX`: the label is centred on that x (as Figma's centre-aligned texts are). */
export function label(doc: Document, text: string, x: number, y: number, size: 10 | 12 | 16 = 12, centerX = false): HTMLDivElement {
    const e = div(doc, `bsb-text bsb-t${size}${centerX ? ' bsb-center' : ''}`, { x, y }, text)
    return e
}

/** An inline icon from a Figma SVG asset. */
export function icon(doc: Document, svg: string, x: number, y: number, cls = ''): HTMLDivElement {
    const e = div(doc, `bsb-icon ${cls}`.trim(), { x, y })
    e.innerHTML = svg
    return e
}

export const arrowLeft = (doc: Document, x: number, y: number, mirrored = false) => icon(doc, arrowLeftSvg, x, y, mirrored ? 'bsb-mirror' : '')
export const arrowDown = (doc: Document, x: number, y: number) => icon(doc, arrowDownSvg, x, y, 'bsb-arrow-down')

/** A peach pixel button with a 1 px shadow; pressed = sits on its shadow, lighter. */
export class PixelButton {
    readonly el: HTMLButtonElement
    readonly clicked = new Emitter<void>()

    constructor(doc: Document, b: Box, content: string | HTMLElement, ariaLabel: string, shadow: string = SHADOW.light, textSize: 10 | 12 | 16 = 12) {
        const e = doc.createElement('button')
        e.type = 'button'
        e.className = `bsb-btn bsb-t${textSize}`
        e.style.setProperty('--bsb-shadow', shadow)
        e.setAttribute('aria-label', ariaLabel)
        place(e, b)
        if (typeof content === 'string') e.textContent = content
        else e.append(content)
        e.addEventListener('click', () => this.clicked.emit())
        this.el = e
    }

    setPressed(on: boolean): void {
        this.el.classList.toggle('bsb-pressed', on)
        this.el.setAttribute('aria-pressed', String(on))
    }

    isPressed(): boolean {
        return this.el.classList.contains('bsb-pressed')
    }

    setText(text: string): void {
        this.el.textContent = text
    }
}

/** A value field: translucent white box with a text in it. */
export class PixelField {
    readonly el: HTMLDivElement
    private readonly text: HTMLDivElement

    constructor(doc: Document, b: Box, textX: number, size: 10 | 12 = 12, centered = false) {
        this.el = div(doc, 'bsb-field', b)
        this.text = div(doc, `bsb-text bsb-t${size}${centered ? ' bsb-field-center' : ''}`, centered ? { x: 0, y: 0, w: b.w } : { x: textX - b.x, y: 0 })
        // Centred vertically in the field.
        this.text.style.lineHeight = `${b.h}px`
        this.el.append(this.text)
    }

    get value(): string {
        return this.text.textContent ?? ''
    }

    set value(text: string) {
        this.text.textContent = text
    }
}

export interface SliderOptions {
    label: string
    /** Positions (0..1) the handle snaps to within 3 px, each marked with a tiny dot under the track. */
    snaps?: number[]
}

/** A horizontal slider: 3 px track, 3 x 11 handle. Drag, wheel, or arrow keys. */
export class PixelSlider {
    readonly el: HTMLDivElement
    readonly edited = new Emitter<Edit>()
    private readonly handle: HTMLDivElement
    private position = 0

    constructor(
        doc: Document,
        trackX: number,
        trackY: number,
        private readonly width: number,
        private readonly options: SliderOptions,
    ) {
        const e = div(doc, 'bsb-slider', { x: trackX, y: trackY - 4, w: width, h: 11 })
        e.tabIndex = 0
        e.setAttribute('role', 'slider')
        e.setAttribute('aria-label', options.label)
        e.setAttribute('aria-valuemin', '0')
        e.setAttribute('aria-valuemax', '1')
        e.append(div(doc, 'bsb-track', { x: 0, y: 4, w: width, h: 3 }))
        for (const s of options.snaps ?? []) {
            e.append(div(doc, 'bsb-dot', { x: Math.round(this.px(s) - 1.5), y: 7, w: 3, h: 3 }))
        }
        this.handle = div(doc, 'bsb-handle', { x: 0, y: 0, w: 3, h: 11 })
        e.append(this.handle)
        this.el = e

        let dragging = false
        e.addEventListener('pointerdown', (event) => {
            if (event.button !== 0) return
            event.preventDefault()
            e.focus()
            e.setPointerCapture(event.pointerId)
            dragging = true
            this.edited.emit({ value: this.fromPointer(event.clientX), gesture: 'begin' })
        })
        e.addEventListener('pointermove', (event) => {
            if (dragging) this.edited.emit({ value: this.fromPointer(event.clientX) })
        })
        const end = (event: PointerEvent) => {
            if (!dragging) return
            dragging = false
            this.edited.emit({ value: this.fromPointer(event.clientX), gesture: 'end' })
        }
        e.addEventListener('pointerup', end)
        e.addEventListener('pointercancel', end)
        e.addEventListener('wheel', (event) => {
            event.preventDefault()
            this.edited.emit({ value: clamp(this.position + (event.deltaY < 0 ? 0.02 : -0.02)) })
        })
        e.addEventListener('keydown', (event) => {
            const step = { ArrowRight: 0.01, ArrowUp: 0.01, ArrowLeft: -0.01, ArrowDown: -0.01, PageUp: 0.1, PageDown: -0.1 }[event.key]
            const to = event.key === 'Home' ? 0 : event.key === 'End' ? 1 : step === undefined ? null : clamp(this.position + step)
            if (to !== null) {
                event.preventDefault()
                this.edited.emit({ value: to })
            }
        })
    }

    get value(): number {
        return this.position
    }

    set value(v: number) {
        this.position = clamp(v)
        this.handle.style.left = `${Math.round(this.px(this.position) - 1.5)}px`
        this.el.setAttribute('aria-valuenow', this.position.toFixed(3))
    }

    /** Handle centre in px along the track. */
    private px(v: number): number {
        return 1.5 + v * (this.width - 3)
    }

    private fromPointer(clientX: number): number {
        const rect = this.el.getBoundingClientRect()
        const x = ((clientX - rect.left) / rect.width) * this.width
        for (const s of this.options.snaps ?? []) {
            if (Math.abs(x - this.px(s)) <= 3) return s
        }
        return clamp((x - 1.5) / (this.width - 3))
    }
}

export interface ListGeometry {
    box: Required<Box>
    /** Rows shown at once. */
    rows: number
    /** First row's text top and the row step, inside the box. */
    firstTop: number
    step: number
}

/**
 * A drop-down list: orange box with a shadow, rows of 12 px text, a scroll line with a peach thumb.
 * Scrolls with the wheel or by dragging along the scroll line. Clicking outside it raises `dismissed`.
 */
export class PixelList {
    readonly el: HTMLDivElement
    readonly selected = new Emitter<number>()
    readonly dismissed = new Emitter<void>()
    private readonly rowsEl: HTMLDivElement
    private readonly thumb: HTMLDivElement
    private items: string[] = []
    private current = -1
    private scroll = 0
    private open = false

    constructor(
        private readonly doc: Document,
        private readonly geometry: ListGeometry,
        ariaLabel: string,
        /** Elements that may be clicked without dismissing (the toggle button). */
        private readonly keepOpenFor: () => HTMLElement[],
    ) {
        const { box } = geometry
        const e = div(doc, 'bsb-list', box)
        e.setAttribute('role', 'listbox')
        e.setAttribute('aria-label', ariaLabel)
        this.rowsEl = div(doc, 'bsb-list-rows', { x: 0, y: 0, w: box.w - 7, h: box.h })
        const line = icon(doc, scrollLineSvg, box.w - 5, 3, 'bsb-scroll-line')
        this.thumb = div(doc, 'bsb-handle bsb-thumb', { x: box.w - 6, y: 5, w: 4, h: 11 })
        e.append(this.rowsEl, line, this.thumb)
        e.hidden = true
        this.el = e

        e.addEventListener('wheel', (event) => {
            event.preventDefault()
            this.scrollTo(this.scroll + (event.deltaY > 0 ? 1 : -1))
        })
        let dragging = false
        const scrollFromPointer = (clientY: number) => {
            const rect = e.getBoundingClientRect()
            const t = ((clientY - rect.top) / rect.height - 0.1) / 0.8
            this.scrollTo(Math.round(clamp(t) * this.maxScroll()))
        }
        this.thumb.addEventListener('pointerdown', (event) => {
            event.preventDefault()
            event.stopPropagation()
            this.thumb.setPointerCapture(event.pointerId)
            dragging = true
        })
        this.thumb.addEventListener('pointermove', (event) => dragging && scrollFromPointer(event.clientY))
        this.thumb.addEventListener('pointerup', () => (dragging = false))
        line.addEventListener('pointerdown', (event) => scrollFromPointer(event.clientY))

        this.onOutside = (event: Event) => {
            const target = event.target as Node
            if (!this.open || e.contains(target) || this.keepOpenFor().some((k) => k.contains(target))) return
            this.dismissed.emit()
        }
    }

    private readonly onOutside: (event: Event) => void

    setItems(items: string[]): void {
        this.items = items
        this.rowsEl.replaceChildren(
            ...items.map((text, i) => {
                const row = div(this.doc, 'bsb-text bsb-t12 bsb-list-row', { x: 3, y: 0, w: this.geometry.box.w - 14 }, text)
                row.setAttribute('role', 'option')
                row.addEventListener('click', () => this.selected.emit(i))
                return row
            }),
        )
        this.layout()
    }

    setSelected(index: number): void {
        this.current = index
        this.rowsEl.querySelectorAll('.bsb-list-row').forEach((row, i) => row.setAttribute('aria-selected', String(i === index)))
    }

    show(): void {
        if (this.open) return
        this.open = true
        this.el.hidden = false
        // Scroll the selected row into view, a row from the top when possible.
        this.scrollTo(this.current - 1)
        this.doc.addEventListener('pointerdown', this.onOutside, true)
    }

    hide(): void {
        if (!this.open) return
        this.open = false
        this.el.hidden = true
        this.doc.removeEventListener('pointerdown', this.onOutside, true)
    }

    dispose(): void {
        this.hide()
    }

    private maxScroll(): number {
        return Math.max(0, this.items.length - this.geometry.rows)
    }

    private scrollTo(scroll: number): void {
        this.scroll = Math.min(this.maxScroll(), Math.max(0, scroll))
        this.layout()
    }

    private layout(): void {
        const { firstTop, step, rows, box } = this.geometry
        this.rowsEl.querySelectorAll<HTMLElement>('.bsb-list-row').forEach((row, i) => {
            const r = i - this.scroll
            row.hidden = r < 0 || r >= rows
            row.style.top = `${firstTop + r * step}px`
        })
        const travel = box.h - 3 - 11 - 7 // thumb top runs from 5 down to the end of the scroll line
        const t = this.maxScroll() === 0 ? 0 : this.scroll / this.maxScroll()
        this.thumb.style.top = `${Math.round(5 + t * travel)}px`
    }
}

export function clamp(v: number): number {
    return Math.min(1, Math.max(0, Number.isFinite(v) ? v : 0))
}
