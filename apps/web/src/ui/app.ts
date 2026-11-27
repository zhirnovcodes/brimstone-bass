import { EngineDataModel } from './models/engineDataModel'
import { MainUiModel } from './models/mainUiModel'
import { BrimstonePresenter } from './presenters/brimstonePresenter'
import { FramePresenter } from './presenters/framePresenter'
import { LfoPresenter } from './presenters/lfoPresenter'
import { MainPresenter } from './presenters/mainPresenter'
import { MixPresenter } from './presenters/mixPresenter'
import { OscPresenter } from './presenters/oscPresenter'
import { PresetsPresenter } from './presenters/presetsPresenter'
import type { SkinBridge } from './skin'
import type { BrimstoneView } from './views/brimstoneView'
import type { EventView } from './views/eventView'
import type { FrameView } from './views/frameView'
import type { LfoView } from './views/lfoView'
import type { MainView } from './views/mainView'
import type { MixView } from './views/mixView'
import type { OscView } from './views/oscView'
import type { PresetsView } from './views/presetsView'

export interface Views {
    events: EventView
    main: MainView
    frame: FrameView
    brimstone: BrimstoneView
    osc: [OscView, OscView]
    lfo: [LfoView, LfoView, LfoView]
    mix: MixView
    presets: PresetsView
}

export interface UiApp {
    engine: EngineDataModel
    ui: MainUiModel
    dispose(): void
}

/** Composition root: builds the models and wires every view to its presenter. The views come from the skin. */
export function createUi(bridge: SkinBridge, views: Views): UiApp {
    const engine = new EngineDataModel(bridge)
    const ui = new MainUiModel()
    const presenters = [
        new MainPresenter(engine, ui, views.main, views.events),
        new FramePresenter(engine, views.frame),
        new BrimstonePresenter(views.brimstone),
        new OscPresenter(1, engine, ui, views.osc[0]),
        new OscPresenter(2, engine, ui, views.osc[1]),
        new LfoPresenter(1, engine, views.lfo[0]),
        new LfoPresenter(2, engine, views.lfo[1]),
        new LfoPresenter(3, engine, views.lfo[2]),
        new MixPresenter(engine, views.mix),
        new PresetsPresenter(bridge.presets, ui, views.presets),
    ]
    return {
        engine,
        ui,
        dispose() {
            for (const p of presenters) p.dispose()
            engine.dispose()
        },
    }
}
