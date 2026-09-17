import { socketService } from './socket.svelte';
import type { SequenceStatus } from '../interfaces';

// Shared, reactive sequence status fed by the websocket. The firmware pushes a
// { type: "sequence-status", data: {...} } message whenever play/pause/loop
// changes, a new sequence is loaded, or a sequence finishes — so any component
// reading 'sequenceStatusStore.status' always reflects the live engine state.
class SequenceStatusStore {
    public status = $state<SequenceStatus | null>(null);
    private started = false;

    // Idempotent — call once after the socket is connected (see App.svelte).
    start() {
        if (this.started) return;
        this.started = true;

        socketService.subscribe((message) => {
            if (message?.type === 'sequence-status') {
                this.status = message.data as SequenceStatus;
            }
        });
    }
}

// single instance shared across components
export const sequenceStatusStore = new SequenceStatusStore();
