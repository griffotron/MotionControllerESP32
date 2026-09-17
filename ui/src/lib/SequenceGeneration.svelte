<script lang="ts">
    import type { Sequence, SequenceGenerator, SequenceSaveResult, SequenceStatus } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { onMount } from 'svelte';
    import { ENDPOINTS, SEQUENCE_NAMES } from './config';
    import { sequenceStatusStore } from './services/sequenceStatus.svelte';
    import { busyStore } from './services/busy.svelte';

    let { sequenceGenerators, reloadStoredSequences } = $props();

    let selectedSequence = $state<SequenceGenerator | undefined>(undefined);
    let sequencePayload = $state<Sequence>({ name: '', description: '', type: 0, speed: 0, intensity: 0, data: '' });
    let saveRes =  $state<ApiResponse<SequenceSaveResult>>({ data: null, error: null, loading: false });
    let emptyRes =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    let statusRes = $state<ApiResponse<SequenceStatus>>({ data: null, error: null, loading: false});

    function sequenceSelected(){
        if(!selectedSequence)
            return;

        sequencePayload.name = selectedSequence.name;
        sequencePayload.type = selectedSequence.type;
        sequencePayload.speed = selectedSequence.speed;
        sequencePayload.intensity = selectedSequence.intensity
        sequencePayload.data = selectedSequence.data;
    }

    async function sendSequence(){
        emptyRes.error = "";
        await busyStore.run(async () => {
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_SEQ_GENERATE, "POST", sequencePayload);
        });
    }
    
    async function saveSequence(){
        // Hold the central Overlay for the whole flow: save + reload of the
        // parent's stored-sequences list (so it stays until the new list renders).
        await busyStore.run(async () => {
            saveRes.error = "";
            saveRes = await apiRequest<SequenceSaveResult>(ENDPOINTS.POST_SEQ_SAVE, "POST", { });

            // Refresh the stored sequences list in the parent so it reflects the new save.
            if(saveRes.data?.result == "success"){
                await reloadStoredSequences?.();
            }
        });
    }

    onMount(() => {	});

</script>



<div class="p-2">
            {#if emptyRes.error || statusRes.error}
                <div role="alert" class="alert alert-error">
                <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" />
                </svg>
                <span>{emptyRes.error}{statusRes.error}</span>
                </div>
            {/if}

            <div class="flex flex-wrap gap-4 w-full">
                <div class="card shadow-sm w-150 md:w-100 p-3">
                <div class="text-base font-semibold mb-5">Generate Sequence</div>

                <div class="form-control w-full mb-2">
                    <label class="select w-full">
                    <select class="select select-info" id="select-sequence"
                    bind:value={selectedSequence} onchange={sequenceSelected}>
                    <option disabled selected value={undefined}>Select a sequence generator</option>
                    {#each sequenceGenerators as seq}
                        <option value={seq}>
                            {seq.description}
                        </option>
                    {/each}
                    </select>
                    </label>
                </div>
                   <div class="form-control w-full mb-2">
                    {#if selectedSequence?.speedControl } 
                        <div class="flex items-center space-x-3 p-3 rounded-lg mb-2">
                            <span class="text-xs font-bold uppercase opacity-50 w-16">Speed</span>
                            <input type="range" min="1" max="100" bind:value={sequencePayload.speed}
                                class="range range-info range-md flex-1" />
                            <div class="badge badge-info badge-outline font-mono w-16">
                                {sequencePayload.speed}%
                            </div>
                        </div>
                    {/if}
                    {#if selectedSequence?.intensityControl } 
                        <div class="flex items-center space-x-3 p-3 rounded-lg mb-2">
                            <span class="text-xs font-bold uppercase opacity-50 w-16">Intensity</span>
                            <input type="range" min="1" max="100" bind:value={sequencePayload.intensity}
                                class="range range-info range-md flex-1" />
                            <div class="badge badge-info badge-outline font-mono w-16">
                                {sequencePayload.intensity}%
                            </div>
                        </div>
                    {/if}
                    {#if selectedSequence?.dataControl }
                        <div class="flex items-center space-x-3 p-3 rounded-lg mb-2">
                            <span class="text-xs font-bold uppercase opacity-50 w-16">Text</span>
                            <input type="text" bind:value={sequencePayload.data}
                                class="uppercase input input-info input-md flex-1" />
                        </div>
                    {/if}
                   
                    <button class="btn btn-md btn-soft btn-info w-full mb-2 {!selectedSequence ? 'btn-disabled' : ''}" onclick={sendSequence}>
                        <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M9.75 3.104v5.714a2.25 2.25 0 0 1-.659 1.591L5 14.5M9.75 3.104c-.251.023-.501.05-.75.082m.75-.082a24.301 24.301 0 0 1 4.5 0m0 0v5.714c0 .597.237 1.17.659 1.591L19.8 15.3M14.25 3.104c.251.023.501.05.75.082M19.8 15.3l-1.57.393A9.065 9.065 0 0 1 12 15a9.065 9.065 0 0 0-6.23-.693L5 14.5m14.8.8 1.402 1.402c1.232 1.232.65 3.318-1.067 3.611A48.309 48.309 0 0 1 12 21c-2.773 0-5.491-.235-8.135-.687-1.718-.293-2.3-2.379-1.067-3.61L5 14.5" />
                        </svg>

                            Generate & Play
                    </button>


                    <button class="btn btn-md btn-soft btn-info w-full { !sequenceStatusStore.status?.name || sequenceStatusStore.status?.name == SEQUENCE_NAMES.CLOCK ? "btn-disabled" : "" }" onclick={saveSequence}>
                        <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M12 4.5v15m7.5-7.5h-15" />
                        </svg>
                            Save Current Sequence
                    </button>

                    {#if saveRes.data?.result == "success" }
                    <div role="alert" class="alert alert-success">
                    <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z" />
                    </svg>
                    <span>Sequence saved as <strong>{saveRes.data?.description}</strong></span>
                    </div>
                    {/if}

                    {#if saveRes.data?.result == "error" || saveRes.error }
                        <div role="alert" class="alert alert-error">
                        <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" />
                        </svg>
                        <span>Error saving sequence. {saveRes.error}</span>
                        </div>
                    {/if}

                    </div>
                </div>

            </div>

</div>