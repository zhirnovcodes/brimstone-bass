import { defineConfig } from 'vitest/config'
import { devAssets } from './vite.assets.ts'
import { processorScript } from './vite.processor.ts'

// pnpm dev → http://localhost:5280/            the plugin from source: keyboard, transport, level meter
//            http://localhost:5280/preview.html the skin alone, no audio
// processor.js, brimstone-bass.wasm and samples/ are served at the root, where host.assetUrl points.
export default defineConfig({
    plugins: [processorScript('processor.js'), devAssets()],
    publicDir: false,
    server: { port: 5280, strictPort: true, fs: { allow: ['../..'] } },
    test: {
        include: ['src/**/*.test.ts'],
    },
})
