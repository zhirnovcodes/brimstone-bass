import type { Event } from '../core/event'

/**
 * 1 - Event view: fired by every control that has an output line (frame knobs and screen controls).
 * Payload: UI index (params.json index, or FX_ORDER_INDEX for an effects order).
 */
export interface EventView {
    readonly paramChanged: Event<number>
    readonly paramHovered: Event<number>
    readonly paramLeft: Event<number>
}
