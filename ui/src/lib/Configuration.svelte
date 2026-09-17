<script lang="ts">
    import type { SystemConfig } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { onMount } from 'svelte';
    import { busyStore } from './services/busy.svelte';
    import { ENDPOINTS } from './config';

    let { config, reload } = $props();

    let emptyRes =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    // svelte-ignore state_referenced_locally
    let modifiedConfig = $state<SystemConfig>(config.data);
    // svelte-ignore state_referenced_locally
    let pixelMapString = $state<string>(JSON.stringify(config.data.pixelMap));

    let textareaRef = $state<HTMLTextAreaElement | null>(null);

    async function save(){
        await busyStore.run(async () => {
            let saveConfig : SystemConfig = { ...modifiedConfig, pixelMap: JSON.parse(pixelMapString) };
            console.log(saveConfig);
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_SYSTEM_CONFIG, "POST", saveConfig);
            await reload();
        });
    }

    $effect(() => {
        if (textareaRef) {
            textareaRef.style.height = 'auto';
            textareaRef.style.height = textareaRef.scrollHeight + 'px';
        }
    });

    onMount(() => {	});

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
                <span>Configuration</span>
            </div>

            <div class="p-2">
           
               
            <label class="input input-info w-full mb-2">
            <span class="label w-70">Max Target Steps:</span>
            <input type="number" placeholder="0" bind:value={modifiedConfig.maxTarget} />
            </label>

            <label class="input input-info w-full mb-2">
            <span class="label w-70">Homing Target Steps:</span>
            <input type="text" placeholder="0" bind:value={modifiedConfig.homingTarget} />
            </label>

            <label class="input input-info w-full mb-2">
            <span class="label w-70">CAN Id:</span>
            <input type="text" placeholder="0" bind:value={modifiedConfig.canId} />
            </label>
            
            <label class="input input-info w-full mb-2">
            <span class="label w-70">POSIX Timezone:</span>
            <input type="text" placeholder="GMT0BST,M3.5.0/1,M10.5.0" bind:value={modifiedConfig.posixTimezoneString} />
            </label>

            <label class="label mb-2">
                <input type="checkbox" bind:checked={modifiedConfig.debug} class="checkbox checkbox-info" />
                Debug
            </label>

            <p></p>
            <p>Pixel Map</p>
            <textarea bind:this={textareaRef} class="textarea textarea-info textarea-bordered h-24 w-full font-mono mb-2" placeholder="Pixel Map" bind:value={pixelMapString}></textarea>
                



            <button class="btn btn-md btn-soft btn-info w-full" onclick={save}>
            <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" fill="currentColor" class="size-5">
            <path d="M2 3a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v1a1 1 0 0 1-1 1H3a1 1 0 0 1-1-1V3Z" />
            <path fill-rule="evenodd" d="M13 6H3v6a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2V6ZM8.75 7.75a.75.75 0 0 0-1.5 0v2.69L6.03 9.22a.75.75 0 0 0-1.06 1.06l2.5 2.5a.75.75 0 0 0 1.06 0l2.5-2.5a.75.75 0 1 0-1.06-1.06l-1.22 1.22V7.75Z" clip-rule="evenodd" />
            </svg>

                Save
            </button>

            </div>

            </div>
            </div>

</div>