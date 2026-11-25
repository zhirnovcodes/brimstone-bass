# Brimstone Bass: rules for agents

Brimstone Bass is an audio instrument (bass generator) built from one C++ core: an IvanDAW web plugin (wasm,
Plugin API v2) and, later, a JUCE plugin (VST3, AU, Standalone). It is Axolotl Pad (C:\Work\AxolotlPad,
zhirnovcodes/axolotl-pad) with a different oscillator: read Axolotl's code first when something here is unclear.

Guide: ivan-daw `docs/plugin-dev/` (README, 01-architecture, types/audio-instrument.md, 03-web-host, 04-vst-host, 05-ci-cd).
Effects: zhirnovcodes/ivan-daw-engine-core `v0.1.0` (FetchContent). The ADSR ports ivan-daw-core's `Envelope`.

## The oscillator (what differs from Axolotl)

```
note -> detune + octave -> SamplePlayer (attack piece, loop on repeat, tail in the release's last R s)
     -> ADSR x velocity -> sum of voices -> Crossover 120 Hz
        low (mono)  ---------------------------------------------+-> x vol -> gate
        high -> Unison -> Delay/Reverb/low-pass Filter (user order) -> pan -+
```

## Names

| Thing | Value |
| --- | --- |
| Repo / wasm / zip | `brimstone-bass` / `brimstone-bass.wasm` / `brimstone-bass-daw.zip` |
| IvanDAW id | `com.ivandaw.brimstone-bass` |
| C ABI prefix / namespace / CSS prefix | `bb_` / `bb::` / `bsb-` |

## Folder map

| Folder | Holds |
| --- | --- |
| `core/domain/` | SamplePlayer, AdsrEnv, Crossover, ParamMap, Lfo, OscMix (C++20 std only) |
| `core/engine/` | ChainStage + Chain, OscBus, Engine (core + ivandaw engine-core) |
| `hosts/web/boundary/` | C ABI → `brimstone-bass.wasm` |
| `hosts/web/audio-thread/` | AudioWorklet `processor.ts` (one file, no imports) |
| `apps/web/src/` | `plugin.ts` (PluginFactory), params, display, samples, dev page, skin preview |
| `apps/web/src/ui/` | the skin; talks only to `SkinBridge` |
| `samples/` | built-in samples + `samples.json` (root note; attack, loop, release in seconds) |
| `params.json` | the only parameter list; CMake writes `ParamsJson.h` from it |
| `tests/` | `bb_tests` (core) |

## Commands

```
# Two build folders: build/vst (native: core tests) and build/web (wasm, IvanDAW plugin in build/web/dist-daw).
cmake -S . -B build/vst -G "Visual Studio 17 2022"
cmake --build build/vst --config Release --target bb_tests -- -m:1 -p:CL_MPCount=2   # 8 GB RAM: keep -m:1
build\vst\Release\bb_tests.exe
emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release   # emsdk: C:\Users\1695294\emsdk
cmake --build build/web --target brimstone-bass
pnpm install
pnpm --filter @brimstone-bass/web build:daw
pnpm --filter @brimstone-bass/web dev   # http://localhost:5280/ (plugin, keyboard A–K) · /preview.html (skin only)
```

## Rules

- All behaviour lives in `core/`. Hosts translate; they never decide musical behaviour.
- `params.json` is append-only. Ids and indexes never change.
- Effects order is state, not a parameter: no automation, MIDI learn or LFO may change it.
- The same input gives the same output for any block size. Core tests check sizes 1, 64, 128, 512, 4096.
- Every note-on gets a note-off path (stop, CC 120/123, `setState`, dispose).
- Every sound start, stop or change gets a fade.
- No allocation, lock or log in `Engine::process`.
- Below 120 Hz the bass is mono and dry: nothing after the crossover may touch the low band.
- You cannot hear. Every check says exactly what Ivan should listen for.
- Branch, PR to `master`. ivan-daw is changed only in its own session.
