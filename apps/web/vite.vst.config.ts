import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { defineConfig, type Plugin } from 'vite'

// The skin for the JUCE plugin window: one classic script, skin.js, with the CSS, font and images inlined.
// CMake runs this build (outDir build/vst/ui) and builds skin.js into the plugin next to hosts/plugin/ui/index.html.

function dropCss(): Plugin {
    return {
        name: 'brimstone-bass-drop-css',
        // vst.ts injects the CSS itself. Vite still extracts a copy after the bundle hooks run.
        writeBundle(options) {
            for (const fileName of fs.readdirSync(options.dir!)) {
                if (fileName.endsWith('.css')) {
                    fs.rmSync(path.join(options.dir!, fileName))
                }
            }
        },
    }
}

export default defineConfig({
    plugins: [dropCss()],
    publicDir: false,
    build: {
        outDir: fileURLToPath(new URL('../../build/vst/ui', import.meta.url)),
        emptyOutDir: true,
        // WebView2 (Windows) and WKWebView on macOS 11.
        target: 'es2020',
        lib: {
            entry: fileURLToPath(new URL('./src/vst.ts', import.meta.url)),
            formats: ['iife'],
            name: 'BrimstoneBassSkin',
            fileName: () => 'skin.js',
        },
    },
})
