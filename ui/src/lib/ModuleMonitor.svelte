<script lang="ts">
    import type { ModuleStatus, MoveCommand, Sequence } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { onMount  } from 'svelte';
    import { busyStore } from './services/busy.svelte';
    import { ENDPOINTS } from './config';
    import { socketService } from './services/socket.svelte';
    import { moduleStatusStore } from './services/moduleStatus.svelte';

    let { config } = $props();

    let error = $state<string>("");
    let modules = $state<ApiResponse<number[]>>({error: null, loading: false, data: []});
    let checkingStatus = $state<Boolean>(true);
    let commandResult =  $state<ApiResponse<null>>({ data: null, error: null, loading: false });
    let moveCmd = $state<MoveCommand>({ pixels: [[1,1]], priority: 1, speed: 100, target: 0 });
    let testingModuleId = $state<number | undefined>(undefined);

    function getStatus(moduleId: number){
        let status = moduleStatusStore.moduleStatuses.find((x) => { return x.moduleId == moduleId});
        if(status){
            return status.status;
        }

        return -1; // module not responded
    }

    function getModulePixels(moduleId: number){
        let pixelArray : Array<Array<number>> = [];
        for(let x = 0; x < config.data.pixelMap.length; x++){
            for(let y = 0; y < config.data.pixelMap[x].length; y++){
                if(config.data.pixelMap[x][y][0] == moduleId){
                    pixelArray.push([ x + 1, y + 1]);
                }
            }
        }
        return pixelArray;
    }

    async function testModule(moduleId: number){
        testingModuleId = moduleId;
        commandResult.loading = true;
        commandResult = await apiRequest<null>(ENDPOINTS.POST_MOTION_MOVE, "POST", 
            { ...moveCmd, pixels: getModulePixels(moduleId), target: config.data.maxTarget });
        
        setTimeout(async () => {
            commandResult.loading = true;
            commandResult = await apiRequest<null>(ENDPOINTS.POST_MOTION_MOVE, "POST", 
            { ...moveCmd, pixels: getModulePixels(moduleId), target: 0 });
            testingModuleId = undefined;
        }, 3000);
    }

    async function homeModule(moduleId: number){
        commandResult.loading = true;
        commandResult = await apiRequest<null>(ENDPOINTS.POST_MOTION_HOME, "POST", 
            { ...moveCmd, pixels: getModulePixels(moduleId), target: config.data.maxTarget });
    }

    async function resetModule(moduleId: number){
        commandResult.loading = true;
        commandResult = await apiRequest<null>(ENDPOINTS.POST_MODULE_RESET, "POST", 
            { moduleId: moduleId });
    }

    async function sendHomeAll(){
        commandResult.loading = true;
        let homeSequence : Sequence = { name:"home-all", description: "Home All", type: 0, speed: 0, intensity: 0, data: "" }
        commandResult = await apiRequest<null>(ENDPOINTS.POST_SEQ_LOAD, "POST", homeSequence);
    }

    function send(){
        moduleStatusStore.moduleStatuses = [];
        checkingStatus = true;

        setTimeout(() => { checkingStatus = false }, 10000); // Give up waiting for statuses after 10s

        socketService.send({"action" : "scan-modules"});
    }

    async function load(){
        // Fetch list of modules that should exist
        await busyStore.run(async () => {
            modules = await apiRequest<number[]>(ENDPOINTS.GET_MODULES)
                            .then<ApiResponse<number[]>>((x) => {
                                setTimeout(() => { send();  }, 1000);
                                return x;
                            });
        });
    }

    onMount(() => {
        load();
        moduleStatusStore.start();
	});

</script>



<div class="p-2">

        {#if error || modules.error || commandResult.error }
            <div role="alert" class="alert alert-error">
            <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <span>{error}{modules.error}{commandResult.error}</span>
            </div>
        {/if}
    
        <div class="flex flex-wrap gap-4 w-full">
        <div class="card shadow-sm w-150 md:w-200 p-0.5">
         <div class="text-base p-2 font-semibold mb-5 flex">
                <span>Modules</span>

                <div class="ml-auto">
                <button class="btn btn-sm" title="Refresh" onclick={send}>
                    <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-4">
                    <path stroke-linecap="round" stroke-linejoin="round" d="M19.5 12c0-1.232-.046-2.453-.138-3.662a4.006 4.006 0 0 0-3.7-3.7 48.678 48.678 0 0 0-7.324 0 4.006 4.006 0 0 0-3.7 3.7c-.017.22-.032.441-.046.662M19.5 12l3-3m-3 3-3-3m-12 3c0 1.232.046 2.453.138 3.662a4.006 4.006 0 0 0 3.7 3.7 48.656 48.656 0 0 0 7.324 0 4.006 4.006 0 0 0 3.7-3.7c.017-.22.032-.441.046-.662M4.5 12l3 3m-3-3-3 3" />
                    </svg>
                    Refresh
                </button>

                <button class="btn btn-sm" onclick={sendHomeAll}>
                    <svg class="size-[1.2em]" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" d="m2.25 12 8.954-8.955c.44-.439 1.152-.439 1.591 0L21.75 12M4.5 9.75v10.125c0 .621.504 1.125 1.125 1.125H9.75v-4.875c0-.621.504-1.125 1.125-1.125h2.25c.621 0 1.125.504 1.125 1.125V21h4.125c.621 0 1.125-.504 1.125-1.125V9.75M8.25 21h8.25" />
                    </svg>
                    Home All
                </button>
                </div>

            </div>



        <ul class="list bg-base-100">

        {#each modules.data as module}

        <li class="list-row flex flex-col gap-3 bg-base-100 p-4 sm:flex-row sm:items-center sm:justify-between">

            <div class="flex w-full items-center gap-3 sm:w-auto">
                <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-5 shrink-0 opacity-70">
                <path stroke-linecap="round" stroke-linejoin="round" d="M8.25 3v1.5M4.5 8.25H3m18 0h-1.5M4.5 12H3m18 0h-1.5m-15 3.75H3m18 0h-1.5M8.25 19.5V21M12 3v1.5m0 15V21m3.75-18v1.5m0 15V21m-9-1.5h10.5a2.25 2.25 0 0 0 2.25-2.25V6.75a2.25 2.25 0 0 0-2.25-2.25H6.75A2.25 2.25 0 0 0 4.5 6.75v10.5a2.25 2.25 0 0 0 2.25 2.25Zm.75-12h9v9h-9v-9Z" />
                </svg>
                <div class="flex flex-1 items-center justify-between gap-3 sm:block sm:flex-none">
                    <div class="text-sm">Module <span class="font-extrabold">{module}</span></div>
                    <div class="flex items-center gap-2 text-xs font-semibold uppercase">
                        <div class="inline-grid *:[grid-area:1/1]">
                            <div class="status {getStatus(module) == 1 ? 'status-success' : 'status-error'} animate-ping"></div>
                            <div class="status {getStatus(module) == 1 ? 'status-success' : 'status-error'}"></div>
                        </div>
                        <span class="opacity-60">{getStatus(module) == 1 ? "Connected" : checkingStatus ? "Waiting..." : "Unreachable"}</span>
                    </div>
                </div>
            </div>

            <div class="flex gap-2 sm:shrink-0">
                <button class="btn btn-sm flex-1 sm:flex-none" onclick={() => testModule(module)} disabled={commandResult.loading || testingModuleId == module}>
                <svg class="size-[1.2em]" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor">
                <path stroke-linecap="round" stroke-linejoin="round" d="M5.25 5.653c0-.856.917-1.398 1.667-.986l11.54 6.347a1.125 1.125 0 0 1 0 1.972l-11.54 6.347a1.125 1.125 0 0 1-1.667-.986V5.653Z" />
                </svg>
                {testingModuleId == module ? "Testing..." : "Test"}
                </button>

                <button class="btn btn-sm flex-1 sm:flex-none" onclick={() => homeModule(module)} disabled={commandResult.loading || testingModuleId == module}>
                <svg class="size-[1.2em]" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor">
                    <path stroke-linecap="round" stroke-linejoin="round" d="m2.25 12 8.954-8.955c.44-.439 1.152-.439 1.591 0L21.75 12M4.5 9.75v10.125c0 .621.504 1.125 1.125 1.125H9.75v-4.875c0-.621.504-1.125 1.125-1.125h2.25c.621 0 1.125.504 1.125 1.125V21h4.125c.621 0 1.125-.504 1.125-1.125V9.75M8.25 21h8.25" />
                </svg>
                Home
                </button>

                <button class="btn btn-sm flex-1 sm:flex-none" onclick={() => resetModule(module)} disabled={commandResult.loading || testingModuleId == module}>
                <svg class="size-[1.2em]" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor">
                    <path fill-rule="evenodd" d="M4.755 10.059a7.5 7.5 0 0 1 12.548-3.364l1.903 1.903h-3.183a.75.75 0 1 0 0 1.5h4.992a.75.75 0 0 0 .75-.75V4.356a.75.75 0 0 0-1.5 0v3.18l-1.9-1.9A9 9 0 0 0 3.306 9.67a.75.75 0 1 0 1.45.388Zm15.408 3.352a.75.75 0 0 0-.919.53 7.5 7.5 0 0 1-12.548 3.364l-1.902-1.903h3.183a.75.75 0 0 0 0-1.5H2.984a.75.75 0 0 0-.75.75v4.992a.75.75 0 0 0 1.5 0v-3.18l1.9 1.9a9 9 0 0 0 15.059-4.035.75.75 0 0 0-.53-.918Z" clip-rule="evenodd" />
                </svg>

                Reset
                </button>

            </div>

        </li>

        {/each}

        </ul>
        </div>


        </div>

</div>