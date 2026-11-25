import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { defineConfig, type Plugin } from 'vite'
import { manifest } from './src/manifest.ts'
import { sampleFiles, samplesDir, wasmCommand, wasmPath } from './vite.assets.ts'
import { processorScript } from './vite.processor.ts'

// The IvanDAW build (build/web/dist-daw/): one folder that works from any URL.
// index.js          the PluginFactory, the skin and its CSS bundled in
// processor.js      the AudioWorklet script
// brimstone-bass.wasm
// manifest.json     the manifest as data
// samples/          the built-in samples

function dawFiles(): Plugin {
    return {
        name: 'brimstone-bass-daw-files',
        buildStart() {
            if (!fs.existsSync(wasmPath)) {
                this.error(`build/web/brimstone-bass.wasm is missing. Run the wasm build first, from the repo root:\n  ${wasmCommand}`)
            }
        },
        // index.js injects the CSS itself (plugin.ts). Vite still extracts a copy after the bundle hooks run.
        writeBundle(options) {
            for (const fileName of fs.readdirSync(options.dir!)) {
                if (fileName.endsWith('.css')) {
                    fs.rmSync(path.join(options.dir!, fileName))
                }
            }
        },
        generateBundle() {
            this.emitFile({ type: 'asset', fileName: 'brimstone-bass.wasm', source: fs.readFileSync(wasmPath) })
            this.emitFile({ type: 'asset', fileName: 'manifest.json', source: JSON.stringify(manifest, null, 2) + '\n' })
            for (const file of sampleFiles()) {
                const source = path.join(samplesDir, file)
                if (!fs.existsSync(source)) {
                    this.error(`samples.json lists a file that is missing: samples/${file}`)
                }
                this.emitFile({ type: 'asset', fileName: 'samples/' + file, source: fs.readFileSync(source) })
            }
        },
    }
}

export default defineConfig({
    plugins: [processorScript('processor.js'), dawFiles()],
    publicDir: false,
    build: {
        // Next to the wasm build, so the web has one build folder: build/web.
        outDir: fileURLToPath(new URL('../../build/web/dist-daw', import.meta.url)),
        emptyOutDir: true,
        target: 'es2022',
        lib: {
            entry: fileURLToPath(new URL('./src/plugin.ts', import.meta.url)),
            formats: ['es'],
            fileName: () => 'index.js',
        },
    },
})
