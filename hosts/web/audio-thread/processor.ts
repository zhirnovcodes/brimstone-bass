// Audio thread: runs brimstone-bass.wasm inside an AudioWorklet.
// Served as one script, so this file imports nothing. Messages are described in apps/web/src/plugin.ts.

declare const sampleRate: number
// The first frame of the block process() is rendering.
declare const currentFrame: number

interface Core {
    memory: WebAssembly.Memory
    _initialize?: () => void
    bb_alloc: (bytes: number) => number
    bb_free: (ptr: number) => void
    bb_create: (rate: number, seed: number) => number
    bb_destroy: (engine: number) => void
    bb_set_param: (engine: number, index: number, value: number) => void
    bb_set_fx_order: (engine: number, osc: number, a: number, b: number, c: number) => number
    bb_set_sample: (
        engine: number,
        index: number,
        left: number,
        right: number,
        frames: number,
        rate: number,
        rootNote: number,
        attack: number,
        loop: number,
        release: number,
        crossfade: number,
    ) => number
    bb_transport: (engine: number, playing: number, bpm: number, beat: number, num: number, den: number, frame: number) => void
    bb_midi: (engine: number, frame: number, status: number, d1: number, d2: number) => void
    bb_all_off: (engine: number, frame: number) => void
    bb_process: (engine: number, frames: number, left: number, right: number) => void
    bb_active_voices: (engine: number) => number
}

type HostMessage =
    | { type: 'init'; wasm: ArrayBuffer; seed: number }
    | {
          type: 'sample'
          index: number
          left: Float32Array
          right: Float32Array | null
          rate: number
          rootNote: number
          attack: number
          loop: number
          release: number
          crossfade: number
      }
    | { type: 'param'; index: number; value: number }
    | { type: 'fxOrder'; osc: number; order: [number, number, number] }
    // time is in the context's seconds. A time already past plays at once.
    | { type: 'midi'; time: number; status: number; d1: number; d2: number }
    | { type: 'transport'; time: number; playing: boolean; bpm: number; beat: number; numerator: number; denominator: number }
    | { type: 'allOff'; time: number }
    | { type: 'dispose' }

// frame is on the context's clock: Math.round(time * sampleRate).
type Timed =
    | { frame: number; kind: 'midi'; status: number; d1: number; d2: number }
    | { frame: number; kind: 'transport'; playing: boolean; bpm: number; beat: number; numerator: number; denominator: number }
    | { frame: number; kind: 'allOff' }

interface WorkletPort {
    onmessage: ((event: { data: HostMessage }) => void) | null
    postMessage(message: unknown): void
}

declare class AudioWorkletProcessor {
    readonly port: WorkletPort
    constructor()
    process(inputs: Float32Array[][], outputs: Float32Array[][], parameters: Record<string, Float32Array>): boolean
}

declare function registerProcessor(name: string, processorCtor: new () => AudioWorkletProcessor): void

class BrimstoneBassProcessor extends AudioWorkletProcessor {
    private core: Core | null = null
    private engine = 0
    private leftPtr = 0
    private rightPtr = 0
    private capacity = 0
    private outLeft: Float32Array | null = null
    private outRight: Float32Array | null = null
    private waiting: HostMessage[] = []
    // Notes and transport changes waiting for their frame, kept across blocks.
    private timed: Timed[] = []
    // Built-in samples by index. Loaded once; the engine reads this memory for as long as it lives.
    private samples = new Set<number>()
    private disposed = false
    private meterBlocks = 0
    private peak = 0

    constructor() {
        super()
        this.port.onmessage = (event) => {
            void this.onMessage(event.data)
        }
    }

    process(_inputs: Float32Array[][], outputs: Float32Array[][]): boolean {
        if (this.disposed) {
            return false
        }
        const channels = outputs[0]
        if (!channels || channels.length === 0) {
            return true
        }
        const frames = channels[0].length
        if (!this.core || this.engine === 0) {
            for (const channel of channels) {
                channel.fill(0)
            }
            return true
        }
        const core = this.core
        this.ensureRenderBuffers(frames)

        // The events due in this block go to the engine at their frame. Late ones play at its start.
        const end = currentFrame + frames
        if (this.timed.length > 0) {
            this.timed.sort((a, b) => a.frame - b.frame)
            let count = 0
            while (count < this.timed.length && this.timed[count].frame < end) {
                count++
            }
            for (const event of this.timed.splice(0, count)) {
                const at = Math.max(0, event.frame - currentFrame)
                if (event.kind === 'midi') {
                    core.bb_midi(this.engine, at, event.status, event.d1, event.d2)
                } else if (event.kind === 'transport') {
                    core.bb_transport(this.engine, event.playing ? 1 : 0, event.bpm, event.beat, event.numerator, event.denominator, at)
                } else {
                    core.bb_all_off(this.engine, at)
                }
            }
        }
        core.bb_process(this.engine, frames, this.leftPtr, this.rightPtr)

        const { left, right } = this.outputViews(frames)
        channels[0].set(left)
        if (channels.length > 1) {
            channels[1].set(right)
        }
        for (let i = 0; i < frames; i++) {
            this.peak = Math.max(this.peak, Math.abs(left[i]), Math.abs(right[i]))
        }
        // About 30 meter updates a second.
        if (++this.meterBlocks >= Math.max(1, Math.round(sampleRate / frames / 30))) {
            this.meterBlocks = 0
            this.port.postMessage({ type: 'meter', peak: this.peak, voices: core.bb_active_voices(this.engine) })
            this.peak = 0
        }
        return true
    }

    private async onMessage(message: HostMessage) {
        if (message.type === 'dispose') {
            this.disposed = true
            if (this.core && this.engine !== 0) {
                this.core.bb_destroy(this.engine)
                this.engine = 0
            }
            return
        }
        if (message.type === 'init') {
            try {
                await this.init(message.wasm, message.seed)
                const pending = this.waiting
                this.waiting = []
                for (const queued of pending) {
                    this.handle(queued)
                }
            } catch (error) {
                this.port.postMessage({ type: 'error', message: error instanceof Error ? error.message : String(error) })
            }
            return
        }
        if (!this.core) {
            this.waiting.push(message)
            return
        }
        this.handle(message)
    }

    private async init(wasm: ArrayBuffer, seed: number) {
        const { instance } = await WebAssembly.instantiate(wasm, {
            env: { emscripten_notify_memory_growth() {} },
        })
        const core = instance.exports as unknown as Core
        core._initialize?.()
        const engine = core.bb_create(sampleRate, seed >>> 0)
        if (engine === 0) {
            throw new Error('bb_create failed')
        }
        this.core = core
        this.engine = engine
        this.port.postMessage({ type: 'ready' })
    }

    private handle(message: HostMessage) {
        const core = this.core!
        switch (message.type) {
            case 'param':
                core.bb_set_param(this.engine, message.index, message.value)
                break
            case 'fxOrder': {
                const [a, b, c] = message.order
                core.bb_set_fx_order(this.engine, message.osc, a, b, c)
                break
            }
            case 'sample':
                this.loadSample(message)
                break
            case 'midi': {
                const frame = Math.round(message.time * sampleRate)
                if ((message.status & 0xf0) === 0xb0 && (message.d1 === 120 || message.d1 === 123)) {
                    // Silence all: drop what was queued at or after it.
                    this.timed = this.timed.filter((e) => e.frame < frame)
                }
                this.timed.push({ frame, kind: 'midi', status: message.status, d1: message.d1, d2: message.d2 })
                break
            }
            case 'transport':
                this.timed.push({
                    frame: Math.round(message.time * sampleRate),
                    kind: 'transport',
                    playing: message.playing,
                    bpm: message.bpm,
                    beat: message.beat,
                    numerator: message.numerator,
                    denominator: message.denominator,
                })
                break
            case 'allOff':
                // CC 123 semantics: everything queued at or after it is dropped, then all voices release.
                this.timed = this.timed.filter((e) => e.frame < Math.round(message.time * sampleRate))
                this.timed.push({ frame: Math.round(message.time * sampleRate), kind: 'allOff' })
                break
            default:
                break
        }
    }

    private loadSample(message: Extract<HostMessage, { type: 'sample' }>) {
        const core = this.core!
        if (this.samples.has(message.index)) {
            return
        }
        const frames = message.left.length
        const left = core.bb_alloc(frames * 4)
        const right = message.right ? core.bb_alloc(frames * 4) : 0
        if (left === 0 || (message.right && right === 0)) {
            this.port.postMessage({ type: 'error', message: 'out of memory loading a sample' })
            return
        }
        new Float32Array(core.memory.buffer, left, frames).set(message.left)
        if (message.right) {
            new Float32Array(core.memory.buffer, right, frames).set(message.right)
        }
        const ok = core.bb_set_sample(
            this.engine,
            message.index,
            left,
            right,
            frames,
            message.rate,
            message.rootNote,
            message.attack,
            message.loop,
            message.release,
            message.crossfade,
        )
        if (ok === 0) {
            core.bb_free(left)
            if (right !== 0) {
                core.bb_free(right)
            }
            this.port.postMessage({ type: 'error', message: `sample ${message.index} rejected` })
            return
        }
        this.samples.add(message.index)
        this.port.postMessage({ type: 'sampleLoaded', index: message.index })
    }

    // A new view every block allocates on the audio thread; keep the views until the wasm memory moves.
    private outputViews(frames: number): { left: Float32Array; right: Float32Array } {
        const buffer = this.core!.memory.buffer
        if (
            !this.outLeft ||
            !this.outRight ||
            this.outLeft.buffer !== buffer ||
            this.outLeft.byteOffset !== this.leftPtr ||
            this.outLeft.length !== frames
        ) {
            this.outLeft = new Float32Array(buffer, this.leftPtr, frames)
            this.outRight = new Float32Array(buffer, this.rightPtr, frames)
        }
        return { left: this.outLeft, right: this.outRight! }
    }

    private ensureRenderBuffers(frames: number) {
        if (this.capacity >= frames) {
            return
        }
        const core = this.core!
        if (this.leftPtr !== 0) {
            core.bb_free(this.leftPtr)
        }
        if (this.rightPtr !== 0) {
            core.bb_free(this.rightPtr)
        }
        this.leftPtr = core.bb_alloc(frames * 4)
        this.rightPtr = core.bb_alloc(frames * 4)
        this.capacity = frames
    }
}

registerProcessor('brimstone-bass', BrimstoneBassProcessor)
