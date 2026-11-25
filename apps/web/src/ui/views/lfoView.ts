import type { Event } from '../core/event'
import type { Edit } from './edit'

/** 4 - LFO view: one LFO screen (LFO1, LFO2 or LFO3). Texts come in ready to show. */
export interface LfoView {
    /** The map drop-down: open / close it, mark the selected entry. */
    openMap(): void
    hideMap(): void
    setMapValue(index: number): void
    /** Entries of the map drop-down (≤ 8 chars each), in the order shown. setMapValue / mapValueSelected use rows of it. */
    setMapList(items: string[]): void

    /** The arrow on the map button: list closed (open it) / list open (close it). */
    setMapButtonOpen(): void
    setMapButtonClose(): void

    getMap(): string
    setMap(text: string): void
    getType(): string
    setType(text: string): void
    /** Slider position 0..1. */
    getOffset(): number
    setOffset(value: number): void
    getRetrig(): boolean
    setRetrig(on: boolean): void
    getSync(): boolean
    setSync(on: boolean): void

    readonly openMapClicked: Event<void>
    readonly closeMapClicked: Event<void>
    readonly leftTypeClicked: Event<void>
    readonly rightTypeClicked: Event<void>
    /** Index into the map list. */
    readonly mapValueSelected: Event<number>
    readonly offsetChanged: Event<Edit>
    readonly retrigClicked: Event<void>
    readonly syncClicked: Event<void>
}
