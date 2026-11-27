import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import type { Plugin } from 'vite'

// The files the plugin folder carries next to index.js: the wasm, the built-in samples and the presets.
// The dev server serves them at the same relative paths the DAW build writes them to.

export const wasmPath = fileURLToPath(new URL('../../build/web/brimstone-bass.wasm', import.meta.url))
export const samplesDir = fileURLToPath(new URL('../../samples', import.meta.url))
/** The plugin's presets: <name>.brimstone files. The dev page's preset panel edits this folder. */
export const presetsDir = fileURLToPath(new URL('../../presets', import.meta.url))
const PRESET_EXT = '.brimstone'
export const wasmCommand =
    'emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/web --target brimstone-bass'

export function sampleFiles(): string[] {
    const doc = JSON.parse(fs.readFileSync(path.join(samplesDir, 'samples.json'), 'utf8')) as { samples: { file: string }[] }
    return doc.samples.map((sample) => sample.file)
}

/** The presets folder's files, sorted. */
export function presetFiles(): string[] {
    if (!fs.existsSync(presetsDir)) return []
    return fs
        .readdirSync(presetsDir)
        .filter((file) => file.toLowerCase().endsWith(PRESET_EXT))
        .sort((a, b) => a.localeCompare(b, undefined, { numeric: true, sensitivity: 'base' }))
}

/** presets/index.json: the presets' names (file names without .brimstone). */
export const presetIndex = () => JSON.stringify(presetFiles().map((file) => file.slice(0, -PRESET_EXT.length))) + '\n'

/** A preset file name the dev server accepts: the same characters cleanPresetName (src/presets.ts) keeps. */
const isPresetFile = (file: string) => /^[A-Za-z0-9 _-]{1,16}\.brimstone$/.test(file) && file.trim() === file

const types: Record<string, string> = { '.wasm': 'application/wasm', '.wav': 'audio/wav', '.json': 'application/json' }

function readBody(req: import('node:http').IncomingMessage): Promise<string> {
    return new Promise((resolve, reject) => {
        let body = ''
        req.setEncoding('utf8')
        req.on('data', (chunk: string) => {
            body += chunk
            if (body.length > 1_000_000) req.destroy(new Error('too large'))
        })
        req.on('end', () => resolve(body))
        req.on('error', reject)
    })
}

export function devAssets(): Plugin {
    return {
        name: 'brimstone-bass-dev-assets',
        configureServer(server) {
            server.middlewares.use((req, res, next) => {
                const url = decodeURIComponent(req.url?.split('?')[0] ?? '')
                // The presets folder: read by the plugin, written by the dev page's preset panel (localhost only).
                if (url.startsWith('/presets/')) {
                    void presets(req, res, url.slice('/presets/'.length))
                    return
                }
                let file: string | null = null
                if (url === '/brimstone-bass.wasm') {
                    file = wasmPath
                } else if (url.startsWith('/samples/') && sampleFiles().includes(url.slice('/samples/'.length))) {
                    file = path.join(samplesDir, url.slice('/samples/'.length))
                }
                if (!file) {
                    next()
                    return
                }
                if (!fs.existsSync(file)) {
                    res.statusCode = 404
                    res.end(file === wasmPath ? `build/web/brimstone-bass.wasm is missing. From the repo root:\n  ${wasmCommand}` : 'missing')
                    return
                }
                res.setHeader('Content-Type', types[path.extname(file)] ?? 'application/octet-stream')
                res.end(fs.readFileSync(file))
            })
        },
    }
}

async function presets(req: import('node:http').IncomingMessage, res: import('node:http').ServerResponse, file: string) {
    const send = (status: number, body: string, type = 'text/plain') => {
        res.statusCode = status
        res.setHeader('Content-Type', type)
        res.setHeader('Cache-Control', 'no-store')
        res.end(body)
    }
    if (file === 'index.json' && req.method === 'GET') {
        send(200, presetIndex(), 'application/json')
        return
    }
    if (!isPresetFile(file)) {
        send(400, 'a preset file is <name>.brimstone: letters, digits, space, - and _, up to 16 characters')
        return
    }
    const target = path.join(presetsDir, file)
    // A name that differs only in case is the same file on Windows and macOS: replace it, whatever its case.
    const existing = presetFiles().find((f) => f.toLowerCase() === file.toLowerCase())
    try {
        if (req.method === 'GET') {
            if (!existing) send(404, 'missing')
            else send(200, fs.readFileSync(path.join(presetsDir, existing), 'utf8'), 'application/json')
        } else if (req.method === 'PUT') {
            const body = await readBody(req)
            let data: { params?: unknown } | null = null
            try {
                data = JSON.parse(body) as { params?: unknown }
            } catch {
                // not JSON: refused below
            }
            if (typeof data !== 'object' || data === null || typeof data.params !== 'object') {
                send(400, 'not a preset')
                return
            }
            fs.mkdirSync(presetsDir, { recursive: true })
            if (existing && existing !== file) fs.rmSync(path.join(presetsDir, existing))
            fs.writeFileSync(target, body)
            send(200, 'saved')
        } else if (req.method === 'DELETE') {
            if (existing) fs.rmSync(path.join(presetsDir, existing))
            send(existing ? 200 : 404, existing ? 'deleted' : 'missing')
        } else {
            send(405, 'GET, PUT or DELETE')
        }
    } catch (error) {
        send(500, String(error))
    }
}
