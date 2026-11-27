import { Emitter } from '../core/event'
import type { PresetsView } from '../views/presetsView'
import { div, PixelButton, SHADOW } from './widgets'

/** PRESETS page. Positions: Figma "Frame 9" (the screen at 48, 21 there). */
// Just the list between the caption and the buttons: 4 rows, beside the scroll line (32 .. 83).
export const PRESET_ROWS = { x: 4, firstTop: 33, step: 13, rows: 4, w: 118 } as const
const SCROLL = { x: 127, y: 32, h: 51 } as const // the scroll line (2 px, #F2DFE7)
const THUMB = { x: 126, top: 34, h: 11 } as const

export class PresetsDomView implements PresetsView {
    readonly itemClicked = new Emitter<number>()
    readonly itemOpened = new Emitter<number>()
    readonly openClicked = new Emitter<void>()
    readonly saveClicked = new Emitter<void>()
    readonly loadClicked = new Emitter<void>()

    private readonly rows: HTMLDivElement
    private readonly thumb: HTMLDivElement
    private count = 0
    private scroll = 0
    private selected = -1

    constructor(
        private readonly doc: Document,
        page: HTMLElement,
    ) {
        this.rows = div(doc, 'bsb-preset-rows', { x: 0, y: 0, w: 124, h: 105 })
        this.rows.setAttribute('role', 'listbox')
        this.rows.setAttribute('aria-label', 'Presets')
        const line = div(doc, 'bsb-preset-scroll', { x: SCROLL.x, y: SCROLL.y, w: 2, h: SCROLL.h })
        this.thumb = div(doc, 'bsb-handle bsb-thumb', { x: THUMB.x, y: THUMB.top, w: 4, h: THUMB.h })
        this.thumb.style.setProperty('box-shadow', `1px 1px 0 ${SHADOW.dark}`)

        const button = (x: number, text: string, aria: string, out: Emitter<void>) => {
            const b = new PixelButton(doc, { x, y: 87, w: 34, h: 14 }, text, aria, SHADOW.dark, 10)
            b.clicked.on(() => out.emit())
            return b.el
        }

        this.rows.addEventListener('wheel', (event) => {
            event.preventDefault()
            this.scrollTo(this.scroll + (event.deltaY > 0 ? 1 : -1))
        })
        let dragging = false
        const fromPointer = (clientY: number) => {
            const rect = line.getBoundingClientRect()
            const t = rect.height > 0 ? (clientY - rect.top) / rect.height : 0
            this.scrollTo(Math.round(Math.min(1, Math.max(0, t)) * this.maxScroll()))
        }
        this.thumb.addEventListener('pointerdown', (event) => {
            event.preventDefault()
            this.thumb.setPointerCapture(event.pointerId)
            dragging = true
        })
        this.thumb.addEventListener('pointermove', (event) => dragging && fromPointer(event.clientY))
        this.thumb.addEventListener('pointerup', () => (dragging = false))
        line.addEventListener('pointerdown', (event) => fromPointer(event.clientY))

        page.append(
            this.rows,
            line,
            this.thumb,
            button(4, 'OPEN', 'Open the selected preset', this.openClicked),
            button(60, 'SAVE', 'Save the sound to a file on this computer', this.saveClicked),
            button(97, 'LOAD', 'Load a preset file from this computer', this.loadClicked),
        )
    }

    setItems(items: string[]): void {
        this.count = items.length
        this.rows.replaceChildren(
            ...items.map((text, i) => {
                const row = div(this.doc, 'bsb-text bsb-t10 bsb-preset-row', { x: PRESET_ROWS.x, y: 0, w: PRESET_ROWS.w, h: 12 }, text)
                row.setAttribute('role', 'option')
                row.addEventListener('click', () => this.itemClicked.emit(i))
                row.addEventListener('dblclick', () => this.itemOpened.emit(i))
                return row
            }),
        )
        this.scrollTo(this.scroll)
        this.setSelected(this.selected < this.count ? this.selected : -1)
    }

    setSelected(index: number): void {
        this.selected = index
        this.rows.querySelectorAll('.bsb-preset-row').forEach((row, i) => row.setAttribute('aria-selected', String(i === index)))
        if (index >= 0 && (index < this.scroll || index >= this.scroll + PRESET_ROWS.rows)) {
            this.scrollTo(index - 1)
        }
    }

    private maxScroll(): number {
        return Math.max(0, this.count - PRESET_ROWS.rows)
    }

    private scrollTo(scroll: number): void {
        this.scroll = Math.min(this.maxScroll(), Math.max(0, scroll))
        this.rows.querySelectorAll<HTMLElement>('.bsb-preset-row').forEach((row, i) => {
            const r = i - this.scroll
            row.hidden = r < 0 || r >= PRESET_ROWS.rows
            row.style.top = `${PRESET_ROWS.firstTop + r * PRESET_ROWS.step}px`
        })
        const travel = SCROLL.y + SCROLL.h - THUMB.h - 2 - THUMB.top
        const t = this.maxScroll() === 0 ? 0 : this.scroll / this.maxScroll()
        this.thumb.style.top = `${Math.round(THUMB.top + t * travel)}px`
    }
}
