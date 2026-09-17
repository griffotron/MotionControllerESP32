<script lang="ts">
    import type { Sequence } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { busyStore } from './services/busy.svelte';
    import { ENDPOINTS } from './config';

    let emptyRes =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    let demoMinutes = $state(5);

    async function startDemo(){
        await busyStore.run(async () => {
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_SEQ_DEMO, "POST", { minutes: demoMinutes });
        });
    }

    async function sendSequence(sequenceName: string){
        let sequence : Sequence = { name:sequenceName, type: 0, speed: 0, intensity: 0, description: '', data: '' }
        await busyStore.run(async () => {
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_SEQ_GENERATE, "POST", sequence);
        });
    }
</script>

<div class="p-2">

        {#if emptyRes.error }
            <div role="alert" class="alert alert-error">
            <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <span>{emptyRes.error}</span>
            </div>
        {/if}


        <div class="flex flex-wrap gap-4 w-full">
        <div class="card shadow-sm w-150 md:w-200 p-0.5">
         <div class="text-base p-2 font-semibold mb-5 flex">
                <span>Mode Selection</span>
            </div>
           <ul class="list bg-base-100">
                
                <li class="list-row px-2">
                    <div>
                            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor" class="size-4 me-2">
                                <path fill-rule="evenodd" d="M9.58 1.077a.75.75 0 0 1 .405.82L9.165 6h4.085a.75.75 0 0 1 .567 1.241l-6.5 7.5a.75.75 0 0 1-1.302-.638L6.835 10H2.75a.75.75 0 0 1-.567-1.241l6.5-7.5a.75.75 0 0 1 .897-.182Z" clip-rule="evenodd" />
                                </svg>
                    </div>
                    <div>
                    <div class="text-sm uppercase font-semibold opacity-60">Random</div>
                    <div class="text-xs">Switches between a 24 hour clock and a randomly selected saved sequence.</div>
                    <div class="text-xs">
                            <label class="flex items-center gap-2">
                                <span>Mins between sequences: </span>
                                <input name="mins" type="number" min="1" bind:value={demoMinutes} class="input input-info input-sm w-10 text-center px-1" />
                            </label>
                        </div> 
                    </div>
                        <button title="Run demo" class="btn btn-md btn-soft btn-info" onclick={startDemo}>
                            <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                                <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                            </svg>
                        </button>
                </li>
                
                <li class="list-row px-2">
                    <div>
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor" class="size-4 me-2">
                                <path fill-rule="evenodd" d="M1 8a7 7 0 1 1 14 0A7 7 0 0 1 1 8Zm7.75-4.25a.75.75 0 0 0-1.5 0V8c0 .414.336.75.75.75h3.25a.75.75 0 0 0 0-1.5h-2.5v-3.5Z" clip-rule="evenodd" />
                                </svg>
                    </div>
                    <div>
                        <div class="text-sm uppercase font-semibold opacity-60">Clock</div>
                        <div class="text-xs">This mode displays a 24 hour clock based on the configured time zone.</div>    
                    </div>
                        <button title="Clock Mode" class="btn btn-md btn-soft btn-info" onclick={() => sendSequence('clock')}>
                            <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                                <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                            </svg>
                        </button>
                </li>
                
                <li class="list-row px-2">
                    <div>
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor" class="size-4 me-2">
                                    <path d="M4.5 2a.5.5 0 0 0-.5.5v11a.5.5 0 0 0 .5.5h1a.5.5 0 0 0 .5-.5v-11a.5.5 0 0 0-.5-.5h-1ZM10.5 2a.5.5 0 0 0-.5.5v11a.5.5 0 0 0 .5.5h1a.5.5 0 0 0 .5-.5v-11a.5.5 0 0 0-.5-.5h-1Z" />
                                </svg>
                    </div>
                    <div>
                        <div class="text-sm uppercase font-semibold opacity-60">Idle</div>
                        <div class="text-xs">Clears any running sequence and sets all pixels to zero position.</div>
                    </div>
                            <button title="Idle Mode" class="btn btn-md btn-soft btn-info" onclick={() => sendSequence('idle')}>
                                        <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                                            <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                                        </svg>
                                    </button>
                </li>
                
                <li class="list-row px-2">
                    <div>
                        <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor" class="size-4 me-2">
                        <path d="M8.543 2.232a.75.75 0 0 0-1.085 0l-5.25 5.5A.75.75 0 0 0 2.75 9H4v4a1 1 0 0 0 1 1h1a1 1 0 0 0 1-1v-1a1 1 0 1 1 2 0v1a1 1 0 0 0 1 1h1a1 1 0 0 0 1-1V9h1.25a.75.75 0 0 0 .543-1.268l-5.25-5.5Z" />
                        </svg>

                    </div>
                    <div>
                        <div class="text-sm uppercase font-semibold opacity-60">Homing Sequence</div>
                        <div class="text-xs">Resets the home position of all pixels. Useful if display was powered off with some pixels left open.</div>
                    </div>
                        <button title="Run demo" class="btn btn-md btn-soft btn-info" onclick={() => sendSequence('home-all')}>
                            <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                                <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                            </svg>
                        </button>
                </li>
            </ul>
            </div>
            </div>

</div>
