import { defineConfig } from 'vitest/config'

// Skin preview only (step 1: no engine yet). pnpm dev → http://localhost:5280/
export default defineConfig({
    publicDir: false,
    server: { port: 5280, strictPort: true, fs: { allow: ['../..'] } },
    test: {
        include: ['src/**/*.test.ts'],
    },
})
