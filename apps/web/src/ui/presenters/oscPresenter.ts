import { type FxOrder, INDEX } from '../../params'
import { SAMPLES } from '../../samples'
import { Subscriptions } from '../core/event'
import { FX_ORDER_INDEX } from '../data/engineData'
import { FX_LETTERS } from '../data/paramText'
import { signed } from '../../display'
import { NO_FX_BUTTON } from '../data/uiData'
import type { EngineDataModel } from '../models/engineDataModel'
import type { MainUiModel } from '../models/mainUiModel'
import type { OscView } from '../views/oscView'
import { applyEdit, readParam, readPosition, stepParam, toggleParam, writeParam } from './paramAccess'

/**
 * Connects one OSC screen (osc = 1 or 2) to the engine data model and the UI model.
 * Effect buttons: click one → it stays pressed (the output line then says e.g. "FIL. REPLACE");
 * click another → the two swap places in the chain; click the same one again → cancel.
 */
export class OscPresenter {
    private readonly subs = new Subscriptions()
    private readonly slot: 0 | 1
    private readonly key: 'osc1' | 'osc2'
    private readonly ids: { on: string; sample: string; oct: string; attack: string; decay: string; sustain: string; release: string }

    constructor(
        osc: 1 | 2,
        private readonly engine: EngineDataModel,
        private readonly ui: MainUiModel,
        private readonly view: OscView,
    ) {
        this.slot = osc === 1 ? 0 : 1
        this.key = osc === 1 ? 'osc1' : 'osc2'
        const ids = {
            on: `osc${osc}On`,
            sample: `osc${osc}Sample`,
            oct: `osc${osc}Oct`,
            attack: `osc${osc}Attack`,
            decay: `osc${osc}Decay`,
            sustain: `osc${osc}Sustain`,
            release: `osc${osc}Release`,
        }
        this.ids = ids
        const mine = new Set<number>([...Object.values(ids).map((id) => INDEX.get(id)!), FX_ORDER_INDEX[this.slot]])

        view.setSampleList(SAMPLES.map((s) => s.label.toUpperCase()))
        view.hideSampleList()
        view.setSampleButtonClose()
        this.showAll()
        view.setFxButtonPressed(ui.getFxButtonPressed(this.slot))

        this.subs.add(view.onClicked.on(() => toggleParam(engine, ids.on)))
        this.subs.add(
            view.openSampleClicked.on(() => {
                view.setSampleValue(readParam(engine, ids.sample))
                view.openSampleList()
                view.setSampleButtonOpen()
            }),
        )
        this.subs.add(view.closeSampleClicked.on(() => this.closeSampleList()))
        this.subs.add(
            view.sampleSelected.on((index) => {
                writeParam(engine, ids.sample, index)
                this.closeSampleList()
            }),
        )
        this.subs.add(view.octaveDownClicked.on(() => stepParam(engine, ids.oct, -1, true)))
        this.subs.add(view.octaveUpClicked.on(() => stepParam(engine, ids.oct, +1, true)))
        this.subs.add(view.attackChanged.on((edit) => applyEdit(engine, ids.attack, edit)))
        this.subs.add(view.decayChanged.on((edit) => applyEdit(engine, ids.decay, edit)))
        this.subs.add(view.sustainChanged.on((edit) => applyEdit(engine, ids.sustain, edit)))
        this.subs.add(view.releaseChanged.on((edit) => applyEdit(engine, ids.release, edit)))
        this.subs.add(view.fxButtonClicked.on((position) => this.fxClicked(position)))
        this.subs.add(
            engine.changed.on((index) => {
                if (mine.has(index)) this.showAll()
            }),
        )
        this.subs.add(
            ui.changed.on((change) => {
                if (change === 'fxButton') view.setFxButtonPressed(ui.getFxButtonPressed(this.slot))
            }),
        )
    }

    dispose(): void {
        this.subs.dispose()
    }

    private order(): FxOrder {
        return this.engine.getOscillatorsData()[this.key].fxOrder
    }

    private closeSampleList(): void {
        this.view.hideSampleList()
        this.view.setSampleButtonClose()
    }

    private fxClicked(position: number): void {
        const pressed = this.ui.getFxButtonPressed(this.slot)
        if (pressed === NO_FX_BUTTON) {
            this.ui.setFxButtonPressed(this.slot, position)
            return
        }
        this.ui.setFxButtonPressed(this.slot, NO_FX_BUTTON)
        if (pressed !== position) {
            const data = this.engine.getOscillatorsData()
            const order = data[this.key].fxOrder
            ;[order[pressed], order[position]] = [order[position], order[pressed]]
            this.engine.setOscillatorsData(data)
        }
    }

    private showAll(): void {
        const { engine, view, ids } = this
        view.setOn(readParam(engine, ids.on) >= 0.5)
        const sample = readParam(engine, ids.sample)
        view.setSample(SAMPLES[sample]?.label.toUpperCase() ?? '')
        view.setSampleValue(sample)
        view.setOctave(signed(readParam(engine, ids.oct)))
        view.setAttack(readPosition(engine, ids.attack))
        view.setDecay(readPosition(engine, ids.decay))
        view.setSustain(readPosition(engine, ids.sustain))
        view.setRelease(readPosition(engine, ids.release))
        view.setFxOrder(this.order().map((e) => FX_LETTERS[e]))
    }
}
