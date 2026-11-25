import { PARAMS } from '../../params'
import { Subscriptions } from '../core/event'
import { PARAM_PATHS } from '../data/engineData'
import type { EngineDataModel } from '../models/engineDataModel'
import type { FrameView } from '../views/frameView'
import { applyEdit, readPosition, writeParam } from './paramAccess'

/** The params.json indexes of the frame knobs: groups "Top level" and "Lower level". */
export const FRAME_KNOBS = PARAMS.flatMap((p, i) => (['topLevel', 'lowerLevel'].includes(PARAM_PATHS[p.id][0]) ? [i] : []))

/** Connects the frame knobs to the engine data model. */
export class FramePresenter {
    private readonly subs = new Subscriptions()

    constructor(
        private readonly engine: EngineDataModel,
        private readonly view: FrameView,
    ) {
        for (const index of FRAME_KNOBS) this.show(index)
        this.subs.add(view.knobChanged.on((edit) => applyEdit(engine, PARAMS[edit.index].id, edit)))
        this.subs.add(view.knobReset.on((index) => writeParam(engine, PARAMS[index].id, PARAMS[index].default)))
        this.subs.add(
            engine.changed.on((index) => {
                if (FRAME_KNOBS.includes(index)) this.show(index)
            }),
        )
    }

    dispose(): void {
        this.subs.dispose()
    }

    private show(index: number): void {
        this.view.setKnob(index, readPosition(this.engine, PARAMS[index].id))
    }
}
