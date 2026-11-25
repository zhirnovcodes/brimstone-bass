import { Emitter, type Event } from '../core/event'
import { initialMainViewData, type MainViewData, NO_FX_BUTTON, SCREENS } from '../data/uiData'
import { OUTPUT_LENGTH } from '../data/paramText'

export type MainUiChange = 'screen' | 'output' | 'fxButton'

/** 2 - Main UI model: the decorator over MainViewData. */
export class MainUiModel {
    private readonly data: MainViewData = initialMainViewData()
    private readonly changedEmitter = new Emitter<MainUiChange>()

    /** Something in the UI data changed. */
    readonly changed: Event<MainUiChange> = this.changedEmitter

    nextScreen(): void {
        this.goTo((this.data.openedScreen + 1) % SCREENS.length)
    }

    prevScreen(): void {
        this.goTo((this.data.openedScreen + SCREENS.length - 1) % SCREENS.length)
    }

    /** Index into SCREENS. */
    getCurrentScreen(): number {
        return this.data.openedScreen
    }

    setOutputText(text: string): void {
        const clipped = text.slice(0, OUTPUT_LENGTH)
        if (clipped !== this.data.lowerText) {
            this.data.lowerText = clipped
            this.changedEmitter.emit('output')
        }
    }

    getOutputText(): string {
        return this.data.lowerText
    }

    /** OSC view data: chain position whose effect button is pressed, or NO_FX_BUTTON. */
    getFxButtonPressed(osc: 0 | 1): number {
        return this.data.osc[osc].fxButtonPressed
    }

    setFxButtonPressed(osc: 0 | 1, position: number): void {
        if (this.data.osc[osc].fxButtonPressed !== position) {
            this.data.osc[osc].fxButtonPressed = position
            this.changedEmitter.emit('fxButton')
        }
    }

    private goTo(screen: number): void {
        if (screen === this.data.openedScreen) return
        this.data.openedScreen = screen
        // A half-done swap does not survive leaving the screen.
        this.data.osc.forEach((o) => (o.fxButtonPressed = NO_FX_BUTTON))
        this.changedEmitter.emit('screen')
    }
}
