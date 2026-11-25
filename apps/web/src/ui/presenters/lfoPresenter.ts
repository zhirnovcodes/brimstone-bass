import { INDEX } from '../../params'
import { Subscriptions } from '../core/event'
import { LFO_TYPE_TEXT, lfoMapOrder, lfoTargetShort } from '../data/paramText'
import type { EngineDataModel } from '../models/engineDataModel'
import type { LfoView } from '../views/lfoView'
import { applyEdit, readParam, readPosition, stepParam, toggleParam, writeParam } from './paramAccess'

/** Connects one LFO screen (lfo = 1, 2 or 3) to the engine data model. Type wraps around; the map list is sorted by name. */
export class LfoPresenter {
    private readonly subs = new Subscriptions()
    private readonly ids: { type: string; map: string; sync: string; retrig: string; offset: string }
    /** Map list row → map choice index. */
    private readonly order = lfoMapOrder()

    constructor(
        lfo: 1 | 2 | 3,
        private readonly engine: EngineDataModel,
        private readonly view: LfoView,
    ) {
        const ids = { type: `lfo${lfo}Type`, map: `lfo${lfo}Map`, sync: `lfo${lfo}Sync`, retrig: `lfo${lfo}Retrig`, offset: `lfo${lfo}Offset` }
        this.ids = ids
        const mine = new Set(Object.values(ids).map((id) => INDEX.get(id)!))

        view.setMapList(this.order.map(lfoTargetShort))
        view.hideMap()
        view.setMapButtonClose()
        this.showAll()

        this.subs.add(
            view.openMapClicked.on(() => {
                view.setMapValue(this.order.indexOf(readParam(engine, ids.map)))
                view.openMap()
                view.setMapButtonOpen()
            }),
        )
        this.subs.add(view.closeMapClicked.on(() => this.closeMap()))
        this.subs.add(
            view.mapValueSelected.on((row) => {
                writeParam(engine, ids.map, this.order[row])
                this.closeMap()
            }),
        )
        this.subs.add(view.leftTypeClicked.on(() => stepParam(engine, ids.type, -1)))
        this.subs.add(view.rightTypeClicked.on(() => stepParam(engine, ids.type, +1)))
        this.subs.add(view.retrigClicked.on(() => toggleParam(engine, ids.retrig)))
        this.subs.add(view.syncClicked.on(() => toggleParam(engine, ids.sync)))
        this.subs.add(view.offsetChanged.on((edit) => applyEdit(engine, ids.offset, edit)))
        this.subs.add(
            engine.changed.on((index) => {
                if (mine.has(index)) this.showAll()
            }),
        )
    }

    dispose(): void {
        this.subs.dispose()
    }

    private closeMap(): void {
        this.view.hideMap()
        this.view.setMapButtonClose()
    }

    private showAll(): void {
        const { engine, view, ids } = this
        const map = readParam(engine, ids.map)
        view.setType(LFO_TYPE_TEXT[readParam(engine, ids.type)] ?? '')
        view.setMap(lfoTargetShort(map))
        view.setMapValue(this.order.indexOf(map))
        view.setOffset(readPosition(engine, ids.offset))
        view.setRetrig(readParam(engine, ids.retrig) >= 0.5)
        view.setSync(readParam(engine, ids.sync) >= 0.5)
    }
}
