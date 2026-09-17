<script lang="ts">
    import type { MoveCommand, Sequence } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { onMount } from 'svelte';
    import { busyStore } from './services/busy.svelte';
    import { ENDPOINTS } from './config';

    let { config, installation } = $props();

    let emptyRes =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    let target1 = $state<number>(0);
    let target2 = $state<number>(0);
    let moveCmd = $state<MoveCommand>({ pixels: [[1,1]], priority: 1, speed: 100, target: 0 });
    let allChecked = $state(false);
    let overrideLimits = $state(false);

    // Pixel bounds are the physical matrix size, so they always apply.
    function validPixels(){
        let err = "";
        if(moveCmd.pixels[0][0] > installation.cols || moveCmd.pixels[0][0] < 0)
            err = "Pixel X must be between 0 and " + installation.cols + ". ";
        if(moveCmd.pixels[0][1] > installation.rows || moveCmd.pixels[0][1] < 0)
            err += "Pixel Y must be between 0 and " + installation.rows;
        emptyRes.error = err;
        return !err;
    }

    async function sendMove(newTarget: number){
        emptyRes.error = "";

        // Pixel bounds checked in normal mode only (override allows free-form entry),
        // and skipped when targeting ALL (which ignores pixels).
        if(!overrideLimits && !allChecked && !validPixels())
            return;

        // Target bounds only enforced when NOT overriding the configured limits.
        if(!overrideLimits && (newTarget < 0 || newTarget > config.data.maxTarget)){
            emptyRes.error = "Target must be between 0 and " + config.data.maxTarget;
            return;
        }

        moveCmd.target = newTarget;
        await busyStore.run(async () => {
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_MOTION_MOVE, "POST", allChecked ? { ...moveCmd, pixels: [[0,0]] } : moveCmd);
        });
    }

    async function sendSetZero(){
        await busyStore.run(async () => {
            emptyRes = await apiRequest<null>(ENDPOINTS.POST_MOTION_ZERO, "POST", allChecked ? { ...moveCmd, pixels: [[0,0]] } : moveCmd);
        });
    }

    async function load() {

	}

    onMount(() => {
		load();
	});

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


           <div class="flex flex-wrap gap-4 w-full text-sm">
            <div class="card shadow-sm w-150 md:w-200 p-2">
            <div class="text-base font-semibold mb-5 flex">
                <span>Custom Motion</span>
                <label class="label ml-auto text-xs">
                    <input type="checkbox" bind:checked={overrideLimits} class="toggle checkbox-info" />
                    Override limits
                </label>
            </div>

            {#if overrideLimits}
                <div role="alert" class="alert alert-warning mb-2 mt-2">
                    <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="h-6 w-6 shrink-0">
                        <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v3.75m-9.303 3.376c-.866 1.5.217 3.374 1.948 3.374h14.71c1.73 0 2.813-1.874 1.948-3.374L13.949 3.378c-.866-1.5-3.032-1.5-3.898 0L2.697 16.126ZM12 15.75h.007v.008H12v-.008Z" />
                    </svg>
                    <span>No validation will be done on the Target values. Be careful not to exceed mechanical limits.</span>
                </div>
            {/if}

            <div class="flex">
            <div class="w-50 flex-none">

                <label class="input {!overrideLimits && (moveCmd.pixels[0][0] > installation.cols || moveCmd.pixels[0][0] < 0) ? 'input-error' : 'input-info'} mb-2">
                <span class="label">Pixel X:</span>
                <input disabled={allChecked} type="number" min="0" max={overrideLimits ? undefined : installation.cols} placeholder="0" bind:value={moveCmd.pixels[0][0]} />
                /{installation.cols}
                </label>

                <label class="input {!overrideLimits && (moveCmd.pixels[0][1] > installation.rows || moveCmd.pixels[0][1] < 0) ? 'input-error' : 'input-info'} mb-2">
                <span class="label">Pixel Y:</span>
                <input disabled={allChecked} type="number" min="0" max={overrideLimits ? undefined : installation.rows} placeholder="0" bind:value={moveCmd.pixels[0][1]} />
                /{installation.rows}
                </label>
            </div>
            <div class="w-10 flex-1 p-8">
                <label class="label w-8">
                    <input type="checkbox" bind:checked={allChecked} class="checkbox" />
                    <span class="text-xs font-bold uppercase w-6">ALL</span>

                </label>

            </div>
            </div>

            <div class="form-control w-full mb-2">
            <div class="flex items-center space-x-3 p-3 rounded-lg mb-2">
                <span class="text-xs font-bold uppercase opacity-50 w-16">Speed</span>
                <input type="range" min="1" max="100" bind:value={moveCmd.speed}
                    class="range range-info range-md flex-1" />
                <div class="badge badge-info badge-outline font-mono w-16">
                    {moveCmd.speed}%
                </div>
            </div>

            <div class="flex items-center space-x-3 p-3 rounded-lg mb-2">
                <span class="text-xs font-bold uppercase opacity-50 w-16">Target 1</span>
                {#if overrideLimits}
                    <input type="number" placeholder="0" bind:value={target1} class="input input-info flex-1" />
                {:else}
                    <input type="range" min="0" max={config.data.maxTarget} bind:value={target1}
                        class="range range-info range-md flex-1 px-1" />
                    <div class="badge badge-info badge-outline font-mono w-15">{target1}</div>
                {/if}
                <button class="btn btn-md btn-soft btn-info" onclick={() => sendMove(target1)} title="Send">
                <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                    <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                </svg>
            </button>
            </div>

            <div class="flex items-center space-x-3 p-3 rounded-lg mb-2">
                <span class="text-xs font-bold uppercase opacity-50 w-16">Target 2</span>
                {#if overrideLimits}
                    <input type="number" placeholder="0" bind:value={target2} class="input input-info flex-1" />
                {:else}
                    <input type="range" min="0" max={config.data.maxTarget} bind:value={target2}
                        class="range range-info range-md flex-1" />
                    <div class="badge badge-info badge-outline font-mono w-15">{target2}</div>
                {/if}
                <button class="btn btn-md btn-soft btn-info" onclick={() => sendMove(target2)} title="Send">
                <svg xmlns="http://www.w3.org/2000/svg" fill="currentColor" viewBox="0 0 24 24" class="size-5">
                    <path fill-rule="evenodd" d="M4.5 5.653c0-1.426 1.529-2.33 2.779-1.643l11.54 6.348c1.295.712 1.295 2.573 0 3.285L7.28 19.991c-1.25.687-2.779-.217-2.779-1.643V5.653Z" clip-rule="evenodd" />
                </svg>
            </button>
            </div>

            </div>

            <button class="btn btn-md btn-soft btn-info" onclick={sendSetZero}>
                <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
                <path stroke-linecap="round" stroke-linejoin="round" d="m2.25 12 8.954-8.955c.44-.439 1.152-.439 1.591 0L21.75 12M4.5 9.75v10.125c0 .621.504 1.125 1.125 1.125H9.75v-4.875c0-.621.504-1.125 1.125-1.125h2.25c.621 0 1.125.504 1.125 1.125V21h4.125c.621 0 1.125-.504 1.125-1.125V9.75M8.25 21h8.25" />
                </svg>
                Set current position as home
            </button>


            </div>

            </div>

            <div class="flex flex-wrap gap-4 w-full">

            <div class="card shadow-sm w-150 md:w-200 p-4 mt-5 text-sm">
            <div class="text-base font-semibold mb-5">Installation Details</div>
            <table class="table table-sm">
                    <tbody>
                    <tr>
                        <th>Layout (X x Y)</th>
                        <td>{installation.cols} x {installation.rows}
                        </td>
                    </tr>
                    <tr>
                        <th>Max Target</th>
                        <td>{config.data.maxTarget}</td>
                    </tr>
                    <tr>
                        <th>Modules</th>
                        <td>{installation.controlModuleCount}</td>
                    </tr>
                    <tr>
                        <th>Pixels</th>
                        <td>{installation.pixelCount}</td>
                    </tr>

                    </tbody>
                </table>
            </div>

            </div>

</div>