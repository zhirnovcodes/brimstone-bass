import type { PluginManifest } from 'ivandaw-core'

// Plain data, so the DAW build can also write it out as manifest.json.
export const ID = 'com.ivandaw.brimstone-bass'
export const VERSION = '0.0.1' // same as VERSION in CMakeLists.txt

export const manifest: PluginManifest = {
    id: ID,
    name: 'Brimstone Bass',
    vendor: 'zhirnovcodes',
    version: VERSION,
    apiVersion: 2,
    kind: 'instrument',
    ports: [
        { id: 'in', label: 'Events', kind: 'events', direction: 'in', accepts: ['midi', 'automation', 'tempo', 'transport'] },
        { id: 'out', label: 'Output', kind: 'audio', direction: 'out', channels: 2, main: true },
    ],
    ui: { width: 260, height: 306, resizable: false }, // DEVICE in ui/skin.ts: Figma size
    // Notes and transport land on their frame; automation applies on arrival, so not 'sample-accurate' yet.
    features: ['state'],
}
