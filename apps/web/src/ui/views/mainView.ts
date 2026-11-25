import type { Event } from '../core/event'

/** 2 - Main view: the screen frame: caption bar with < >, the opened screen, the output line. */
export interface MainView {
    setScreenIndex(index: number): void
    setOutputText(text: string): void
    /** Name of the page, e.g. "OSC1". */
    setCaptionText(text: string): void

    readonly prevClicked: Event<void>
    readonly nextClicked: Event<void>
}
