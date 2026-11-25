import { Emitter } from '../core/event'
import { SCREENS } from '../data/uiData'
import type { MainView } from '../views/mainView'
import { arrowLeft, div, label, PixelButton, SHADOW } from './widgets'

/** Where the screen sits on the device (device.svg #screen). */
export const SCREEN_BOX = { x: 63, y: 113, w: 134, h: 134 } as const

/**
 * The screen: amber caption bar with < title >, one page per SCREENS entry, amber output bar.
 * Figma: header 134 x 29 (#FFB43A, top corners 13), footer the same upside down, output text 10 px at (5, 3).
 */
export class MainDomView implements MainView {
    readonly el: HTMLDivElement
    readonly prevClicked = new Emitter<void>()
    readonly nextClicked = new Emitter<void>()
    /** One empty page per screen, in SCREENS order. Page views fill them. */
    readonly pages: HTMLDivElement[]
    private readonly caption: HTMLDivElement
    private readonly output: HTMLDivElement

    constructor(doc: Document) {
        this.el = div(doc, 'bsb-screen', SCREEN_BOX)
        this.pages = SCREENS.map((name) => {
            const page = div(doc, 'bsb-page', { x: 0, y: 0, w: 134, h: 134 })
            page.dataset.screen = name
            return page
        })

        const header = div(doc, 'bsb-bar bsb-header', { x: 0, y: 0, w: 134, h: 29 })
        this.caption = label(doc, '', 67, 7, 16, true)
        const prev = new PixelButton(doc, { x: 7, y: 7, w: 14, h: 16 }, arrowLeft(doc, 3.39, 3.28), 'Previous screen', SHADOW.dark)
        const next = new PixelButton(doc, { x: 113, y: 7, w: 14, h: 16 }, arrowLeft(doc, 5.05, 3.28, true), 'Next screen', SHADOW.dark)
        prev.clicked.on(() => this.prevClicked.emit())
        next.clicked.on(() => this.nextClicked.emit())
        header.append(this.caption, prev.el, next.el)

        const footer = div(doc, 'bsb-bar bsb-footer', { x: 0, y: 105, w: 134, h: 29 })
        this.output = label(doc, '', 5, 3, 10)
        this.output.classList.add('bsb-output')
        this.output.setAttribute('aria-live', 'polite')
        footer.append(this.output)

        this.el.append(...this.pages, header, footer)
    }

    setScreenIndex(index: number): void {
        this.pages.forEach((page, i) => (page.hidden = i !== index))
    }

    setOutputText(text: string): void {
        this.output.textContent = text
    }

    setCaptionText(text: string): void {
        this.caption.textContent = text
    }
}
