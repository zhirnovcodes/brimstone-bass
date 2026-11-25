# Brimstone Bass

Bass generator: MIDI notes in, stereo audio out. Two sample oscillators with an ADSR envelope each; the low end
(below 120 Hz) stays mono and dry, while unison, effects and pan apply only to the highs. Built on the same
tamagotchi-egg skin as [Axolotl Pad](https://github.com/zhirnovcodes/axolotl-pad).

Status: engine and IvanDAW web plugin working with one placeholder sample (a synthetic C2 bass); the real
samples are coming. VST3 / AU later. Rules and layout: [CLAUDE.md](CLAUDE.md).

## Try it locally

```
emcmake cmake -S . -B build/web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/web --target brimstone-bass
pnpm install
pnpm --filter @brimstone-bass/web dev    # http://localhost:5280/
```

Click **Start audio**, play with A W S E D F T G Y H U J K (Z / X change the octave) or a MIDI keyboard.
The skin alone, without audio: http://localhost:5280/preview.html (`?screen=0..6`, `&open=1`, `&hover=<param id>`).

## Samples

Each entry in [samples/samples.json](samples/samples.json) gives the file, the MIDI note it was recorded at, and
three times in seconds of the recording:

| Field | Meaning |
| --- | --- |
| `attack` | `[0, attack)` plays once when the note starts |
| `loop` | `[attack, attack + loop)` repeats while the note sounds (0 = no loop) |
| `release` | the file's last `release` seconds: the tail, played at the end of the note's release (0 = no tail) |
| `crossfade` | optional: overlap at the loop seam (default 0.1 s, at most half the loop) |
