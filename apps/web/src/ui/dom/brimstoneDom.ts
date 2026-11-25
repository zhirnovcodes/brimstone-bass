import type { BrimstoneView } from '../views/brimstoneView'
import { div } from './widgets'

/** One frame of assets/imp.png (16 frames side by side, 1024 x 64). */
export const IMP_BOX = { x: 35, y: 35, w: 64, h: 64 } as const // centred between the header (29) and the footer (105)

/** The BRIMSTONE screen: the imp, looping (the animation is CSS: .bsb-imp in skin.css). */
export class BrimstoneDomView implements BrimstoneView {
    readonly imp: HTMLDivElement

    constructor(doc: Document, page: HTMLDivElement) {
        this.imp = div(doc, 'bsb-imp', IMP_BOX)
        this.imp.setAttribute('role', 'img')
        this.imp.setAttribute('aria-label', 'Brimstone imp')
        page.append(this.imp)
    }
}
