<script lang="ts">
    import type { SavedSequence, SequenceStatus } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { onMount } from 'svelte';
    import { ENDPOINTS } from './config';
    import { sequenceStatusStore } from './services/sequenceStatus.svelte';
    import { busyStore } from './services/busy.svelte';

    let { storedSequences, reloadStoredSequences } = $props();

    let emptyRes =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    let deleteRes = $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    let statusRes = $state<ApiResponse<SequenceStatus>>({ data: null, error: null, loading: false});

    // The sequence pending deletion — non-undefined means the confirm dialog is open.
    let sequenceToDelete = $state<SavedSequence | undefined>(undefined);

    // Alphabetical by name, without mutating the prop array.
    let sortedSequences = $derived(
        [...(storedSequences ?? [])].sort((a, b) => a.name.localeCompare(b.name))
    );

    // Live status pushed over the websocket lets us flag the currently loaded sequence.
    $effect(() => {
        if (sequenceStatusStore.status) {
            statusRes.data = sequenceStatusStore.status;
        }
    });

    async function loadSequence(seq: SavedSequence){
        emptyRes.error = "";
        await busyStore.run(async () => {
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_SEQ_LOAD, "POST", { fileName: seq.fileName });
        });
    }

    async function confirmDelete(){
        if(!sequenceToDelete)
            return;

        const seqFileName = sequenceToDelete.fileName;
        deleteRes.error = "";
        sequenceToDelete = undefined;

        await busyStore.run(async () => {
            deleteRes = await apiRequest<null>(ENDPOINTS.POST_SEQ_DELETE, "POST", { fileName: seqFileName });

            // Refresh the list in the parent so the deleted item disappears.
            if(!deleteRes.error){
                await reloadStoredSequences?.();
            }
        });
    }

    onMount(() => {
	});

</script>





<div class="p-2">

        {#if emptyRes.error || deleteRes.error || statusRes.error}
            <div role="alert" class="alert alert-error mb-4">
            <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <span>{emptyRes.error}{deleteRes.error}{statusRes.error}</span>
            </div>
        {/if}


        <div class="flex flex-wrap gap-4 w-full">
        <div class="card shadow-sm w-150 md:w-200 p-0.5">
            <div class="text-base p-2 font-semibold mb-5 flex">
                <span>Stored Sequences</span>
            </div>

           <ul class="list bg-base-100">
                   
                    {#if sortedSequences.length === 0}
                    <li class="list-row"><div class="text-center opacity-60 py-10">No stored sequences.</div></li>
                        
                    {:else}

                        {#each sortedSequences as seq (seq.name)}
                        {@const playing = statusRes.data?.name === seq.name && statusRes.data?.speed === seq.speed && statusRes.data?.intensity === seq.intensity && statusRes.data?.data === seq.data }

                        <li class="list-row px-2 {playing ? 'ring-2 ring-info' : ''}">
                            <div class="{ playing ? "text-info" : ""}">
                                <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor" class="size-4">
                                <path fill-rule="evenodd" d="M5 4a.75.75 0 0 1 .738.616l.252 1.388A1.25 1.25 0 0 0 6.996 7.01l1.388.252a.75.75 0 0 1 0 1.476l-1.388.252A1.25 1.25 0 0 0 5.99 9.996l-.252 1.388a.75.75 0 0 1-1.476 0L4.01 9.996A1.25 1.25 0 0 0 3.004 8.99l-1.388-.252a.75.75 0 0 1 0-1.476l1.388-.252A1.25 1.25 0 0 0 4.01 6.004l.252-1.388A.75.75 0 0 1 5 4ZM12 1a.75.75 0 0 1 .721.544l.195.682c.118.415.443.74.858.858l.682.195a.75.75 0 0 1 0 1.442l-.682.195a1.25 1.25 0 0 0-.858.858l-.195.682a.75.75 0 0 1-1.442 0l-.195-.682a1.25 1.25 0 0 0-.858-.858l-.682-.195a.75.75 0 0 1 0-1.442l.682-.195a1.25 1.25 0 0 0 .858-.858l.195-.682A.75.75 0 0 1 12 1ZM10 11a.75.75 0 0 1 .728.568.968.968 0 0 0 .704.704.75.75 0 0 1 0 1.456.968.968 0 0 0-.704.704.75.75 0 0 1-1.456 0 .968.968 0 0 0-.704-.704.75.75 0 0 1 0-1.456.968.968 0 0 0 .704-.704A.75.75 0 0 1 10 11Z" clip-rule="evenodd" />
                                </svg>

                            </div>
                            <div>
                                <div class="text-sm"><span class=" uppercase font-semibold opacity-60">{seq.description || seq.name}</span>
                                    
                                </div>
                                <div class="text-xs">
                                    {#if seq.speed}<span>Speed: {seq.speed}%</span>{/if}
                                    {#if seq.intensity}<span>Intensity: {seq.intensity}%</span>{/if}
                                    {#if seq.data}<span class="uppercase">Text: {seq.data}</span>{/if}
                                    {#if !seq.speed && !seq.intensity && !seq.data}<span class="italic">No adjustable settings</span>{/if}

                                </div>    
                            </div>
                                <button class="btn btn-md btn-ghost text-error" aria-label="Delete {seq.name}" onclick={() => sequenceToDelete = seq}>
                                    <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-5">
                                        <path stroke-linecap="round" stroke-linejoin="round" d="m14.74 9-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 0 1-2.244 2.077H8.084a2.25 2.25 0 0 1-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 0 0-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 0 1 3.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 0 0-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 0 0-7.5 0" />
                                    </svg>
                                </button>
                                <button title="Play" class="btn btn-md btn-soft btn-info" onclick={() => loadSequence(seq)}>
                                <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                                        <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                                    </svg>
                                
                                </button>
                               
                        </li>
                        {/each}

                    {/if}

            </ul>
            </div>
        </div>

</div>


<!-- Delete confirmation -->
{#if sequenceToDelete}
    <div class="modal modal-open" role="dialog" aria-modal="true">
        <div class="modal-box">
            <h3 class="text-lg font-bold">Delete sequence?</h3>
            <p class="py-4">
                Are you sure you want to delete
                <strong>{sequenceToDelete.description || sequenceToDelete.name}</strong>?
                This cannot be undone.
            </p>
            <div class="modal-action">
                <button class="btn" onclick={() => sequenceToDelete = undefined}>Cancel</button>
                <button class="btn btn-error" onclick={confirmDelete}>Delete</button>
            </div>
        </div>
        <button class="modal-backdrop" aria-label="Cancel" onclick={() => sequenceToDelete = undefined}></button>
    </div>
{/if}
