import { INDEX } from '../../params'
import { Subscriptions } from '../core/event'
import { MIX_TYPE_TEXT } from '../data/paramText'
import type { EngineDataModel } from '../models/engineDataModel'
import type { MixView } from '../views/mixView'
import { applyEdit, readParam, readPosition, stepParam } from './paramAccess'

/** Connects the MIX screen to the engine data model. Type wraps around; voices stop at 0 and 4. */
export class MixPresenter {
    private readonly subs = new Subscriptions()

    constructor(
        private readonly engine: EngineDataModel,
        private readonly view: MixView,
    ) {
        const mine = new Set(['mixType', 'unisonValue', 'unisonVoices'].map((id) => INDEX.get(id)!))
        this.showAll()
        this.subs.add(view.typeLeftClicked.on(() => stepParam(engine, 'mixType', -1)))
        this.subs.add(view.typeRightClicked.on(() => stepParam(engine, 'mixType', +1)))
        this.subs.add(view.voicesLeftClicked.on(() => stepParam(engine, 'unisonVoices', -1, true)))
        this.subs.add(view.voicesRightClicked.on(() => stepParam(engine, 'unisonVoices', +1, true)))
        this.subs.add(view.unisonValueChanged.on((edit) => applyEdit(engine, 'unisonValue', edit)))
        this.subs.add(
            engine.changed.on((index) => {
                if (mine.has(index)) this.showAll()
            }),
        )
    }

    dispose(): void {
        this.subs.dispose()
    }

    private showAll(): void {
        const { engine, view } = this
        view.setType(MIX_TYPE_TEXT[readParam(engine, 'mixType')] ?? '')
        view.setUnisonValue(readPosition(engine, 'unisonValue'))
        view.setVoices(String(readParam(engine, 'unisonVoices')))
    }
}
