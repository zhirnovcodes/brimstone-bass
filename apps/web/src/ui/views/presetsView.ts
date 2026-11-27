import type { Event } from '../core/event'

/** 7 - Presets view: the PRESETS screen. The current preset's name, the plugin's presets, OPEN / SAVE / LOAD. */
export interface PresetsView {
    /** The current preset's name, ready to show. */
    setName(text: string): void
    /** The plugin's presets, ready to show. */
    setItems(items: string[]): void
    /** Index into the items; -1 = none. The row scrolls into view. */
    setSelected(index: number): void

    /** A row was clicked: select it. */
    readonly itemClicked: Event<number>
    /** A row was double-clicked: open it. */
    readonly itemOpened: Event<number>
    /** OPEN: the selected preset. */
    readonly openClicked: Event<void>
    /** SAVE: a .brimstone file on the user's computer. */
    readonly saveClicked: Event<void>
    /** LOAD: a .brimstone file from the user's computer. */
    readonly loadClicked: Event<void>
}
