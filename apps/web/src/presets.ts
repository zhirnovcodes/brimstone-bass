import type { StateData } from './state'
import type { PresetBridge } from './ui/skin'

// Presets: .brimstone files. One is the shared state JSON (state.ts: every parameter and both effects orders, the
// "sequence" of each oscillator's effects) with the preset's name in it.
//
// The plugin's own presets are the files in presets/ at the repo root. They ship in the plugin folder (IvanDAW:
// presets/ next to index.js, with presets/index.json listing them; JUCE: built into the window's skin.js) and are
// edited only on the localhost dev page. SAVE and LOAD on the PRESETS screen write and read files on the user's computer.

export const PRESET_EXT = '.brimstone'
/** As many characters as a PRESETS row shows. */
export const PRESET_NAME_LENGTH = 16

/** A name every OS can use as a file name: letters, digits, space, - and _, at most 16 characters. '' when nothing is left. */
export function cleanPresetName(name: string): string {
    return name
        .replace(/[^A-Za-z0-9 _-]/g, '')
        .replace(/\s+/g, ' ')
        .trim()
        .slice(0, PRESET_NAME_LENGTH)
        .trim()
}

export const presetFileName = (name: string) => `${name}${PRESET_EXT}`

/** "C:\\x\\Bass 1.brimstone" → "Bass 1". */
export function presetNameOf(fileName: string): string {
    const base = fileName.replace(/^.*[\\/]/, '')
    return cleanPresetName(base.toLowerCase().endsWith(PRESET_EXT) ? base.slice(0, -PRESET_EXT.length) : base)
}

/** The file's text: the state, named, pretty-printed (presets/ is checked in, so its diffs stay readable). */
export function presetText(state: StateData, name: string): string {
    return JSON.stringify({ ...state, name }, null, 2) + '\n'
}

/** A .brimstone file's text → the state to load, named `name` (the file's name wins over the one inside). Throws on a file that is not a preset. */
export function parsePreset(text: string, name: string): StateData {
    const data = JSON.parse(text) as unknown
    if (typeof data !== 'object' || data === null || typeof (data as StateData).params !== 'object') {
        throw new Error('not a Brimstone Bass preset')
    }
    return { ...(data as StateData), name }
}

/** A listing (presets/index.json) → clean names, unique ignoring case (as file names on Windows and macOS), sorted the way people read them. */
export function sortPresetNames(names: unknown): string[] {
    const list = Array.isArray(names) ? names.filter((n): n is string => typeof n === 'string').map(cleanPresetName) : []
    const unique = new Map<string, string>()
    for (const name of list) {
        if (name !== '' && !unique.has(name.toLowerCase())) unique.set(name.toLowerCase(), name)
    }
    return [...unique.values()].sort((a, b) => a.localeCompare(b, undefined, { numeric: true, sensitivity: 'base' }))
}

// Files on the user's computer, from a browser page.

interface SaveFilePicker {
    showSaveFilePicker?: (options: {
        suggestedName: string
        types: { description: string; accept: Record<string, string[]> }[]
    }) => Promise<{ name: string; createWritable(): Promise<{ write(text: string): Promise<void>; close(): Promise<void> }> }>
}

/**
 * Saves `text` as <name>.brimstone on the user's computer. Chrome and Edge show a save dialog (the name may change
 * there); elsewhere, or inside a frame that may not open one, the browser downloads it. Resolves to the name it was
 * saved under, null when the user cancelled.
 */
export async function saveFileFromBrowser(doc: Document, text: string, name: string): Promise<string | null> {
    const win = doc.defaultView as (Window & SaveFilePicker) | null
    if (win?.showSaveFilePicker) {
        try {
            const handle = await win.showSaveFilePicker({
                suggestedName: presetFileName(name),
                types: [{ description: 'Brimstone Bass preset', accept: { 'application/json': [PRESET_EXT] } }],
            })
            const writable = await handle.createWritable()
            await writable.write(text)
            await writable.close()
            return presetNameOf(handle.name) || name
        } catch (error) {
            if ((error as Error).name === 'AbortError') return null
            // SecurityError (a cross-origin frame) and the like: download instead.
        }
    }
    const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }))
    const a = doc.createElement('a')
    a.href = url
    a.download = presetFileName(name)
    a.style.display = 'none'
    doc.body.append(a)
    a.click()
    a.remove()
    setTimeout(() => URL.revokeObjectURL(url), 10_000)
    return name
}

/** Asks for a .brimstone file on the user's computer. Resolves to its name and text, null when the user cancelled. */
export function openFileFromBrowser(doc: Document): Promise<{ name: string; text: string } | null> {
    return new Promise((resolve, reject) => {
        const input = doc.createElement('input')
        input.type = 'file'
        input.accept = PRESET_EXT
        input.addEventListener('change', () => {
            const file = input.files?.[0]
            if (!file) {
                resolve(null)
                return
            }
            file.text().then((text) => resolve({ name: presetNameOf(file.name) || 'Preset', text }), reject)
        })
        input.addEventListener('cancel', () => resolve(null))
        input.click()
    })
}

/** What a browser host gives the preset bridge. */
export interface BrowserPresetHost {
    doc: Document
    /** A file of the plugin's presets folder (path relative to it), or null when it is missing. */
    fetchText(path: string): Promise<string | null>
    saveState(): StateData
    /** Loads a sound; its `name` becomes the current preset name. */
    loadState(data: StateData): void
    name(): string
    setName(name: string): void
    onName(listener: (name: string) => void): () => void
}

/** The skin's PresetBridge for a browser page (IvanDAW, the dev page, the skin preview). `changed()`: the folder changed. */
export function browserPresets(host: BrowserPresetHost): PresetBridge & { changed(): void } {
    const listListeners = new Set<() => void>()
    return {
        async list() {
            const text = await host.fetchText('index.json')
            if (text === null) return []
            try {
                return sortPresetNames(JSON.parse(text))
            } catch {
                return []
            }
        },
        async open(name) {
            const text = await host.fetchText(encodeURIComponent(presetFileName(name)))
            if (text === null) throw new Error(`no preset ${name}`)
            host.loadState(parsePreset(text, name))
        },
        onListChanged(listener) {
            listListeners.add(listener)
            return () => listListeners.delete(listener)
        },
        name: () => host.name(),
        onName: (listener) => host.onName(listener),
        async saveFile() {
            const name = host.name() || 'Preset'
            const saved = await saveFileFromBrowser(host.doc, presetText(host.saveState(), name), name)
            if (saved !== null) host.setName(saved)
            return saved
        },
        async loadFile() {
            const file = await openFileFromBrowser(host.doc)
            if (file === null) return null
            host.loadState(parsePreset(file.text, file.name))
            return file.name
        },
        changed() {
            for (const listener of [...listListeners]) listener()
        },
    }
}
