// Central "is something loading" flag so a single Overlay (in App.svelte) can
// cover cross-cutting async work — e.g. saving a sequence and then reloading the
// shared stored-sequences list, which spans a child component and the parent.
//
// A counter (rather than a boolean) means overlapping or nested operations each
// hold the overlay open, and it only clears once the last one finishes.
class BusyStore {
    private _count = $state(0);

    get active(): boolean {
        return this._count > 0;
    }

    begin(): void {
        this._count++;
    }

    end(): void {
        if (this._count > 0) this._count--;
    }

    // Holds the busy flag for the full duration of an async operation, even if
    // it throws. Nested run() calls are safe thanks to the counter.
    async run<T>(operation: () => Promise<T>): Promise<T> {
        this.begin();
        try {
            return await operation();
        } finally {
            this.end();
        }
    }
}

// Single instance shared across components.
export const busyStore = new BusyStore();
