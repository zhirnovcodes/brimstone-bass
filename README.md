# Brimstone Bass

Bass generator: MIDI notes in, stereo audio out. Two sample oscillators with an ADSR envelope each; the low end
(below 120 Hz) stays mono and dry, while unison, effects and pan apply only to the highs. Built on the same
tamagotchi-egg skin as [Axolotl Pad](https://github.com/zhirnovcodes/axolotl-pad).

Status: skin only (web preview, no engine yet).

## Try the skin

```
pnpm install
pnpm --filter @brimstone-bass/web dev    # http://localhost:5280/
pnpm --filter @brimstone-bass/web check  # typecheck + tests
```

Query options: `?screen=0..6` opens a screen, `&open=1` opens its drop-down, `&hover=<param id>` shows that output line.
