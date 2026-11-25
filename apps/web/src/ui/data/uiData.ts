// 2 - UI data: what the screen is showing. Plain data, no behaviour: MainUiModel owns it.

/** Screens in order; prev / next walk this list (and wrap). */
export const SCREENS = ['BRIMSTONE', 'OSC1', 'OSC2', 'LFO1', 'LFO2', 'LFO3', 'MIX'] as const
export type ScreenName = (typeof SCREENS)[number]

export const Screen = {
    Brimstone: 0,
    Osc1: 1,
    Osc2: 2,
    Lfo1: 3,
    Lfo2: 4,
    Lfo3: 5,
    Mix: 6,
} as const

export const NO_FX_BUTTON = -1

export interface OscViewData {
    /** Effect-chain position (0..2) whose button is pressed, waiting for a second press to swap; -1 = none. */
    fxButtonPressed: number
}

export interface MainViewData {
    /** The output line at the bottom of the screen (max OUTPUT_LENGTH chars). */
    lowerText: string
    /** Index into SCREENS. */
    openedScreen: number
    /** OSC1, OSC2. */
    osc: [OscViewData, OscViewData]
}

export function initialMainViewData(): MainViewData {
    return {
        lowerText: '',
        openedScreen: Screen.Brimstone,
        osc: [{ fxButtonPressed: NO_FX_BUTTON }, { fxButtonPressed: NO_FX_BUTTON }],
    }
}
