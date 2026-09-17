<script lang="ts">
  import type { SavedSequence, SequenceGenerator, SequenceStatus } from './interfaces';
  import { sequenceStatusStore } from '$lib/services/sequenceStatus.svelte';
  import { apiRequest, type ApiResponse } from './api';
  import { onMount } from 'svelte';
  import { ENDPOINTS, SEQUENCE_NAMES } from './config';

  let sequenceEngineStatus = $state<SequenceStatus>({ name: '', description: '', type: 0, loop: false, paused: false, playing: false, speed: 0, intensity: 0, data: '' });
  let cmdRes =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });

  $effect(() => {
    if (sequenceStatusStore.status) {
        sequenceEngineStatus = sequenceStatusStore.status;
    }
  });

    onMount(() => {
      sequenceStatusStore.start();
  })

  async function sendCommand(cmd: number){
      cmdRes.loading = true;
      cmdRes.error = "";
      cmdRes = await apiRequest<null>(ENDPOINTS.POST_SEQ_CONTROL, "POST", { "command": cmd });
  }

</script>

<div class="fixed bottom-0 left-0 right-0 z-[60] pb-safe bg-info text-info-content border-t border-info-content/15 shadow-[0_-2px_12px_rgba(0,0,0,0.15)]">
  <div class="mx-auto flex w-full max-w-3xl items-center gap-3 px-4 py-2 sm:gap-4 sm:px-6">

    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor" class="size-7 shrink-0 opacity-80">
      <path fill-rule="evenodd" d="M9 4.5a.75.75 0 0 1 .721.544l.813 2.846a3.75 3.75 0 0 0 2.576 2.576l2.846.813a.75.75 0 0 1 0 1.442l-2.846.813a3.75 3.75 0 0 0-2.576 2.576l-.813 2.846a.75.75 0 0 1-1.442 0l-.813-2.846a3.75 3.75 0 0 0-2.576-2.576l-2.846-.813a.75.75 0 0 1 0-1.442l2.846-.813A3.75 3.75 0 0 0 7.466 7.89l.813-2.846A.75.75 0 0 1 9 4.5ZM18 1.5a.75.75 0 0 1 .728.568l.258 1.036c.236.94.97 1.674 1.91 1.91l1.036.258a.75.75 0 0 1 0 1.456l-1.036.258c-.94.236-1.674.97-1.91 1.91l-.258 1.036a.75.75 0 0 1-1.456 0l-.258-1.036a2.625 2.625 0 0 0-1.91-1.91l-1.036-.258a.75.75 0 0 1 0-1.456l1.036-.258a2.625 2.625 0 0 0 1.91-1.91l.258-1.036A.75.75 0 0 1 18 1.5ZM16.5 15a.75.75 0 0 1 .712.513l.394 1.183c.15.447.5.799.948.948l1.183.395a.75.75 0 0 1 0 1.422l-1.183.395c-.447.15-.799.5-.948.948l-.395 1.183a.75.75 0 0 1-1.422 0l-.395-1.183a1.5 1.5 0 0 0-.948-.948l-1.183-.395a.75.75 0 0 1 0-1.422l1.183-.395c.447-.15.799-.5.948-.948l.395-1.183A.75.75 0 0 1 16.5 15Z" clip-rule="evenodd" />
    </svg>

    <div class="min-w-0 flex-1 leading-tight">
      {#if !sequenceEngineStatus?.description}
        <span class="text-sm opacity-70">No sequence loaded</span>
      {:else}
        <div class="truncate text-lg font-bold uppercase">{sequenceEngineStatus?.description}</div>
        <div class="text-sm opacity-80">
          Speed: {sequenceEngineStatus?.speed} · Intensity: {sequenceEngineStatus?.intensity}
        </div>
      {/if}
    </div>

    <!-- Controls -->
    <div class="flex shrink-0 items-center gap-2">

      <!-- Loop toggle -->
      <button
        class="btn btn-circle btn-ghost {sequenceEngineStatus?.loop ? 'text-white drop-shadow-[0_0_2px_rgba(255,255,255,0.9)]' : 'opacity-70'} {!sequenceEngineStatus?.name || sequenceEngineStatus?.name == SEQUENCE_NAMES.CLOCK ? 'btn-disabled' : ''}"
        aria-label="Toggle loop"
        aria-pressed={sequenceEngineStatus?.loop}
        onclick={() => sequenceEngineStatus?.loop ? sendCommand(6) : sendCommand(5)}>
        <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
          <path stroke-linecap="round" stroke-linejoin="round" d="M19.5 12c0-1.232-.046-2.453-.138-3.662a4.006 4.006 0 0 0-3.7-3.7 48.678 48.678 0 0 0-7.324 0 4.006 4.006 0 0 0-3.7 3.7c-.017.22-.032.441-.046.662M19.5 12l3-3m-3 3-3-3m-12 3c0 1.232.046 2.453.138 3.662a4.006 4.006 0 0 0 3.7 3.7 48.656 48.656 0 0 0 7.324 0 4.006 4.006 0 0 0 3.7-3.7c.017-.22.032-.441.046-.662M4.5 12l3 3m-3-3-3 3" />
        </svg>
      </button>

      <!-- Play / Pause -->
      <button
        class="btn btn-circle btn-lg {!sequenceEngineStatus?.name || sequenceEngineStatus?.name == SEQUENCE_NAMES.CLOCK ? 'btn-disabled' : ''}"
        aria-label={sequenceEngineStatus?.paused ? 'Play' : 'Pause'}
        onclick={() => sendCommand(sequenceEngineStatus?.paused ? 3 : 2)}>
        {#if sequenceEngineStatus?.paused || !sequenceEngineStatus?.name}
          <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-7">
            <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
          </svg>
        {:else}
          <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-7">
            <path fill-rule="evenodd" d="M6.75 5.25a.75.75 0 0 1 .75-.75H9a.75.75 0 0 1 .75.75v13.5a.75.75 0 0 1-.75.75H7.5a.75.75 0 0 1-.75-.75V5.25Zm7.5 0A.75.75 0 0 1 15 4.5h1.5a.75.75 0 0 1 .75.75v13.5a.75.75 0 0 1-.75.75H15a.75.75 0 0 1-.75-.75V5.25Z" clip-rule="evenodd" />
          </svg>
        {/if}
      </button>

    </div>
  </div>
</div>