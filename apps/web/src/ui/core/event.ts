// The one event primitive the UI layers share. Views expose `Event`s, presenters subscribe.

export interface Event<T> {
    /** Subscribe. Returns an unsubscribe. */
    on(listener: (value: T) => void): () => void
}

export class Emitter<T = void> implements Event<T> {
    private readonly listeners = new Set<(value: T) => void>()

    on(listener: (value: T) => void): () => void {
        this.listeners.add(listener)
        return () => this.listeners.delete(listener)
    }

    emit(value: T): void {
        for (const listener of [...this.listeners]) {
            listener(value)
        }
    }

    clear(): void {
        this.listeners.clear()
    }
}

/** Collects unsubscribes so a presenter can drop all of them in dispose(). */
export class Subscriptions {
    private readonly offs: (() => void)[] = []

    add(off: () => void): void {
        this.offs.push(off)
    }

    dispose(): void {
        for (const off of this.offs.splice(0)) {
            off()
        }
    }
}
