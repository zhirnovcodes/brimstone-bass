import { PARAMS } from '../../params'
import { Emitter } from '../core/event'
import { FX_ORDER_INDEX } from '../data/engineData'
import type { EventView } from '../views/eventView'

/**
 * The event view, DOM side: every control with an output line registers here.
 * Hover / leave come from the pointer (a drag keeps the pointer captured, so leave waits for the release);
 * a control calls `changed` after its own event, so the line shows the new value.
 */
export class DomEvents implements EventView {
    readonly paramChanged = new Emitter<number>()
    readonly paramHovered = new Emitter<number>()
    readonly paramLeft = new Emitter<number>()

    watch(element: HTMLElement | SVGElement, index: number): void {
        // Every parameter control is findable by its id (effects orders: fxOrder1 / fxOrder2).
        element.dataset.param ??= PARAMS[index]?.id ?? `fxOrder${FX_ORDER_INDEX.findIndex((i) => i === index) + 1}`
        element.addEventListener('pointerenter', () => this.paramHovered.emit(index))
        element.addEventListener('pointerleave', () => this.paramLeft.emit(index))
        element.addEventListener('focus', () => this.paramHovered.emit(index))
        element.addEventListener('blur', () => this.paramLeft.emit(index))
    }

    changed(index: number): void {
        this.paramChanged.emit(index)
    }
}
