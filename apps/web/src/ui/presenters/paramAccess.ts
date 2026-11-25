import { INDEX, param, toNormalized, toPlain } from '../../params'
import { type EngineData, type GroupKey, PARAM_PATHS, readPlain, writePlain } from '../data/engineData'
import type { EngineDataModel } from '../models/engineDataModel'
import type { Edit } from '../views/edit'

// One-parameter reads and writes for presenters, done through the model's Get/Set[group]Data.

function getGroup(model: EngineDataModel, group: GroupKey): EngineData[GroupKey] {
    switch (group) {
        case 'topLevel':
            return model.getTopLevelData()
        case 'lowerLevel':
            return model.getLowerLevelData()
        case 'mapping':
            return model.getMappingData()
        case 'oscillators':
            return model.getOscillatorsData()
        case 'mix':
            return model.getMixData()
    }
}

function setGroup(model: EngineDataModel, group: GroupKey, value: EngineData[GroupKey]): void {
    switch (group) {
        case 'topLevel':
            return model.setTopLevelData(value as EngineData['topLevel'])
        case 'lowerLevel':
            return model.setLowerLevelData(value as EngineData['lowerLevel'])
        case 'mapping':
            return model.setMappingData(value as EngineData['mapping'])
        case 'oscillators':
            return model.setOscillatorsData(value as EngineData['oscillators'])
        case 'mix':
            return model.setMixData(value as EngineData['mix'])
    }
}

/** Plain value (params.json min..max; booleans 0 / 1). */
export function readParam(model: EngineDataModel, id: string): number {
    const [group, ...rest] = PARAM_PATHS[id]
    return readPlain({ [group]: getGroup(model, group) } as unknown as EngineData, [group, ...rest])
}

export function writeParam(model: EngineDataModel, id: string, plain: number): void {
    const [group] = PARAM_PATHS[id]
    const holder = { [group]: getGroup(model, group) } as unknown as EngineData
    writePlain(holder, PARAM_PATHS[id], plain)
    setGroup(model, group, holder[group])
}

/** Control position 0..1 for `id`. */
export function readPosition(model: EngineDataModel, id: string): number {
    return toNormalized(param(id), readParam(model, id))
}

/** A knob / slider edit on `id`: gesture begin, value, gesture end. */
export function applyEdit(model: EngineDataModel, id: string, edit: Edit): void {
    const index = INDEX.get(id)!
    if (edit.gesture === 'begin') model.beginGesture(index)
    writeParam(model, id, toPlain(param(id), edit.value))
    if (edit.gesture === 'end') model.endGesture(index)
}

/** Steps a choice / int parameter by `delta`, wrapping (choices) or clamping (`clamp`). */
export function stepParam(model: EngineDataModel, id: string, delta: number, clamp = false): void {
    const p = param(id)
    const count = p.max - p.min + 1
    const current = readParam(model, id)
    const next = clamp ? Math.min(p.max, Math.max(p.min, current + delta)) : p.min + ((((current - p.min + delta) % count) + count) % count)
    writeParam(model, id, next)
}

export function toggleParam(model: EngineDataModel, id: string): void {
    writeParam(model, id, readParam(model, id) >= 0.5 ? 0 : 1)
}
