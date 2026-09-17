import { socketService } from './socket.svelte';
import type { ModuleStatus } from '../interfaces';


class ModuleStatusStore {
    public moduleStatuses = $state<ModuleStatus[]>([]);
    private started = false;

    // Idempotent — subscribes once and stays subscribed for the app's lifetime.
    // Deliberately does NOT return the unsubscribe: this is a shared singleton, so
    // a single component unmounting must not tear the subscription down (otherwise
    // the 'started' guard blocks it from ever re-subscribing).
    start() {
        if (this.started) return;
        this.started = true;

        socketService.subscribe((message) => {
            if (message?.type == "module-scan") {
                this.moduleStatuses.push(message.data as ModuleStatus);
            }
        });
    }
}

// single instance shared across components
export const moduleStatusStore = new ModuleStatusStore();
