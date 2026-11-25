import type { HostContext, PluginEvent, PluginInstance, TransportState } from 'ivandaw-core'
import factory, { type Meter } from './plugin'

// Dev page: plays the plugin without ivan-daw. A stand-in host with a transport, a computer keyboard,
// Web MIDI input when the browser allows it, a level meter, and a log.

const $ = <T extends HTMLElement>(id: string) => document.getElementById(id) as T
const log = (text: string) => {
    const el = $('log')
    el.textContent += `${new Date().toISOString().slice(11, 23)}  ${text}\n`
    el.scrollTop = el.scrollHeight
}

// A W S E D F T G Y H U J K = C .. C, one octave and a note.
// Octaves are named the way the samples are (C3 = MIDI 60, so C1 = MIDI 36: the built-in samples' root note).
// The keyboard starts at C1, where every sample plays as recorded.
const noteName = (note: number) => `${['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'][note % 12]}${Math.floor(note / 12) - 2}`
const KEYS = ['a', 'w', 's', 'e', 'd', 'f', 't', 'g', 'y', 'h', 'u', 'j', 'k']
const BLACK = new Set([1, 3, 6, 8, 10])

let ctx: AudioContext | null = null
let plugin: (PluginInstance & { meter?: (l: (m: Meter) => void) => () => void }) | null = null
let octave = 1
let transport: TransportState = { playing: false, bpm: 120, beat: 0, numerator: 4, denominator: 4, loop: null }
let transportTime = 0
const down = new Map<string, number>() // key → note

function beatNow(): number {
    if (!ctx) {
        return 0
    }
    return transport.playing ? transport.beat + ((ctx.currentTime - transportTime) * transport.bpm) / 60 : transport.beat
}

function send(events: PluginEvent[]) {
    plugin?.schedule(events)
}

function setTransport(next: Partial<TransportState>) {
    if (!ctx) {
        return
    }
    transport = { ...transport, beat: beatNow(), ...next }
    transportTime = ctx.currentTime
    send([{ type: 'transport', time: ctx.currentTime, state: transport }])
    if (next.playing === false) {
        send([{ type: 'midi', time: ctx.currentTime, bytes: [0xb0, 123, 0] }])
    }
    log(`transport ${transport.playing ? 'play' : 'stop'} ${transport.bpm} BPM, beat ${transport.beat.toFixed(2)}`)
}

function noteOn(note: number, velocity = 100) {
    if (!ctx) {
        return
    }
    send([{ type: 'midi', time: ctx.currentTime, bytes: [0x90, note, velocity] }])
    log(`note on ${note} (${noteName(note)}) vel ${velocity}`)
}

function noteOff(note: number) {
    if (!ctx) {
        return
    }
    send([{ type: 'midi', time: ctx.currentTime, bytes: [0x80, note, 0] }])
    log(`note off ${note} (${noteName(note)})`)
}

function buildKeys() {
    const keys = $('keys')
    KEYS.forEach((key, i) => {
        const button = document.createElement('button')
        button.textContent = key.toUpperCase()
        button.className = BLACK.has(i % 12) ? 'black' : ''
        button.dataset.key = key
        const start = (e: Event) => {
            e.preventDefault()
            press(key)
        }
        button.addEventListener('pointerdown', start)
        button.addEventListener('pointerup', () => release(key))
        button.addEventListener('pointerleave', () => release(key))
        keys.append(button)
    })
}

function keyButton(key: string) {
    return document.querySelector<HTMLButtonElement>(`#keys button[data-key="${key}"]`)
}

function press(key: string) {
    const i = KEYS.indexOf(key)
    if (i < 0 || down.has(key)) {
        return
    }
    const note = 12 * (octave + 2) + i
    down.set(key, note)
    keyButton(key)?.classList.add('down')
    noteOn(note)
}

function release(key: string) {
    const note = down.get(key)
    if (note === undefined) {
        return
    }
    down.delete(key)
    keyButton(key)?.classList.remove('down')
    noteOff(note)
}

async function start() {
    if (ctx) {
        return
    }
    ctx = new AudioContext()
    await ctx.resume()
    const audio = ctx
    const host: HostContext = {
        audioContext: audio,
        apiVersion: 2,
        slotId: 'dev',
        transport: () => ({ ...transport, beat: beatNow() }),
        notify: (level, message) => log(`${level}: ${message}`),
        markDirty: () => {},
        assetUrl: (path) => new URL(path, `${location.origin}/`).href,
    }
    log(`AudioContext ${audio.sampleRate} Hz`)
    const started = performance.now()
    plugin = await factory.create(host, $('plugin'))
    log(`plugin ready in ${Math.round(performance.now() - started)} ms`)
    const out = plugin.ports.get('out')
    if (out && 'node' in out) {
        out.node.connect(audio.destination, out.index)
    }
    plugin.subscribe((change) => {
        if (change.source !== 'ui' || change.gesture) {
            log(`param ${change.id} = ${change.value.toFixed(3)} (${change.source}${change.gesture ? ', ' + change.gesture : ''})`)
        }
    })
    const bar = $('meter').firstElementChild as HTMLElement
    plugin.meter?.((m) => {
        bar.style.width = `${Math.min(100, m.peak * 100)}%`
        $('voices').textContent = `voices ${m.voices}`
    })
    $('start').setAttribute('disabled', '')
    for (const id of ['play', 'roundtrip', 'panic']) {
        $(id).removeAttribute('disabled')
    }
    connectMidi()
}

async function connectMidi() {
    if (!('requestMIDIAccess' in navigator)) {
        return
    }
    try {
        const access = await navigator.requestMIDIAccess()
        const listen = () => {
            access.inputs.forEach((input) => {
                input.onmidimessage = (event) => {
                    const d = event.data
                    if (!d || d.length < 2 || d[0] >= 0xf0 || !ctx) {
                        return
                    }
                    send([{ type: 'midi', time: ctx.currentTime, bytes: [d[0], d[1], d[2] ?? 0] }])
                }
            })
        }
        listen()
        access.onstatechange = listen
        log(`Web MIDI: ${access.inputs.size} input(s)`)
    } catch {
        log('Web MIDI not allowed; use the computer keyboard')
    }
}

buildKeys()
$('start').addEventListener('click', () => void start().catch((error) => log(`error: ${error}`)))
$('play').addEventListener('click', () => {
    const playing = !transport.playing
    $('play').setAttribute('aria-pressed', String(playing))
    $('play').textContent = playing ? 'Stop' : 'Play'
    setTransport(playing ? { playing, beat: 0 } : { playing })
})
$<HTMLInputElement>('bpm').addEventListener('change', (e) => {
    const bpm = Number((e.target as HTMLInputElement).value)
    if (bpm >= 20 && bpm <= 300) {
        setTransport({ bpm })
    }
})
$('panic').addEventListener('click', () => {
    if (ctx) {
        send([{ type: 'midi', time: ctx.currentTime, bytes: [0xb0, 123, 0] }])
        log('CC 123')
    }
})
$('roundtrip').addEventListener('click', async () => {
    if (!plugin) {
        return
    }
    const state = await plugin.getState()
    await plugin.setState(JSON.parse(JSON.stringify(state)))
    log(`state round-trip: ${JSON.stringify(state.data).length} bytes`)
})

window.addEventListener('keydown', (e) => {
    if (e.repeat || e.target instanceof HTMLInputElement || e.target instanceof HTMLSelectElement) {
        return
    }
    if (e.key === 'z' || e.key === 'x') {
        octave = Math.max(-2, Math.min(7, octave + (e.key === 'z' ? -1 : 1)))
        $('octave').textContent = String(octave)
        return
    }
    press(e.key.toLowerCase())
})
window.addEventListener('keyup', (e) => release(e.key.toLowerCase()))
