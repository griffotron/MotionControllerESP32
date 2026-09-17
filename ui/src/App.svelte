<script lang="ts">
  import type { Sequence, SequenceGenerator, SequenceStatus } from './lib/interfaces';
  import Status from './lib/Status.svelte';
  import SequenceSelection from './lib/SequenceSelection.svelte';
  import SequenceGeneration from './lib/SequenceGeneration.svelte';
  import SequenceControl from './lib/SequenceControl.svelte';
  import Home from '$lib/Home.svelte';
  import Configuration from '$lib/Configuration.svelte';
  import ModuleMonitor from '$lib/ModuleMonitor.svelte';
  import Advanced from '$lib/Advanced.svelte';
  import Overlay from '$lib/Overlay.svelte';
  import { busyStore } from '$lib/services/busy.svelte';
  import { ENDPOINTS } from '$lib/config';
  import { type SystemConfig, type InstallationSpec } from './lib/interfaces';
  import { apiRequest, type ApiResponse } from './lib/api';
  import { onMount } from 'svelte';
  import { socketService } from '$lib/services/socket.svelte';

  onMount(() => {
      socketService.connect(ENDPOINTS.WEB_SOCKET);
      preflight();
      loadConfig();
      loadSequenceGenerators();
      loadStoredSequences();
  });


  let activeTab = $state('home');

  let config = $state<ApiResponse<SystemConfig>>({ data: {
    debug: true, homingTarget: 0, maxTarget: 0, canId: 0, posixTimezoneString: '', pixelMap: [[[]]]
  },error: null, loading: false});

  let installationSpec = $state<InstallationSpec>({ rows: 0, cols: 0, pixelCount: 0, controlModuleCount: 0 });
  let storedSequencesResponse = $state<ApiResponse<Sequence[]>>({ data: null,error: null, loading: false});
  let storedSequences = $state<Sequence[] | null>([]);
  let sequenceGeneratorsResponse = $state<ApiResponse<SequenceGenerator[]>>({ data: null,error: null, loading: false});
  let sequenceGenerators = $state<SequenceGenerator[] | null>([]);
  

  function navigate(tab: string){
    activeTab = tab;
    // When navigating, we'll close the open menu (only for mobile as desktop is always open)
    const toggle = document.getElementById('drawer-menu') as HTMLInputElement | null;
    if (toggle) toggle.checked = false;
  }

  function preflight(){
    config.error = "";
    sequenceGeneratorsResponse.error = "";
    storedSequencesResponse.error = "";
  }

  async function loadConfig(){
    await busyStore.run(async () => {
      config = await apiRequest<SystemConfig>(ENDPOINTS.GET_SYSTEM_CONFIG);

      if(!config.data)
        return;

      installationSpec.cols = config.data.pixelMap.length;
      installationSpec.rows = config.data.pixelMap[0].length;
      installationSpec.pixelCount = 0;

      const uniqueSet = new Set<number>();
      config.data.pixelMap.forEach((subArray: any[]) => {
          subArray.forEach(innerArray => {
              installationSpec.pixelCount++;
              uniqueSet.add(innerArray[0]);
          });
      });
      installationSpec.controlModuleCount = Array.from(uniqueSet).length;
    });
  }

  async function loadSequenceGenerators(){
    await busyStore.run(async () => {
      sequenceGeneratorsResponse = await apiRequest<SequenceGenerator[]>(ENDPOINTS.GET_SEQ_GENERATORS);
      sequenceGenerators = sequenceGeneratorsResponse.data
    });
  }

  async function loadStoredSequences(){
    await busyStore.run(async () => {
      storedSequencesResponse = await apiRequest<SequenceGenerator[]>(ENDPOINTS.GET_SEQ_SAVED);
      storedSequences = storedSequencesResponse.data
    });
  }

</script>
<main>

    <SequenceControl />

  <div class="drawer lg:drawer-open">
  <input id="drawer-menu" type="checkbox"  class="drawer-toggle" />
    <div class="drawer-content">
      <!-- Navbar -->
      <nav class="navbar w-full bg-base-300">
        <label for="drawer-menu" aria-label="open sidebar" class="btn btn-square btn-ghost">
          <!-- Sidebar toggle icon -->
          <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" stroke-linejoin="round" stroke-linecap="round" stroke-width="2" fill="none" stroke="currentColor" class="my-1.5 inline-block size-4"><path d="M4 4m0 2a2 2 0 0 1 2 -2h12a2 2 0 0 1 2 2v12a2 2 0 0 1 -2 2h-12a2 2 0 0 1 -2 -2z"></path><path d="M9 4v16"></path><path d="M14 10l2 2l-2 2"></path></svg>
        </label>
        <div class="px-4"><h2>Motion Controller</h2></div>


      </nav>
      <!-- Page content here -->
      <div class="p-4 pb-20">
        <Overlay active={busyStore.active}>
          {#if activeTab === 'home'}
              <Home />
          {:else if activeTab === 'module-monitor'}
              <ModuleMonitor config={config} />
          {:else if activeTab === 'sequence-selector'}
              <SequenceSelection storedSequences={storedSequences} reloadStoredSequences={loadStoredSequences} />
          {:else if activeTab === 'sequence-generator'}
              <SequenceGeneration sequenceGenerators={sequenceGenerators} reloadStoredSequences={loadStoredSequences} />
          {:else if activeTab === 'status'}
              <Status />
          {:else if activeTab === 'advanced'}
              <Advanced config={config} installation={installationSpec} />
          {:else if activeTab === 'config'}
              <Configuration config={config} reload={loadConfig} />
          {/if}
        </Overlay>
      </div>
    </div>

  <div class="drawer-side max-lg:is-drawer-close:overflow-visible pb-18">
    <label for="drawer-menu" aria-label="close sidebar" class="drawer-overlay"></label>
    <div class="flex min-h-full flex-col items-start bg-base-200 w-64 max-lg:is-drawer-close:w-14">
      <!-- Sidebar content here -->
      <ul class="menu w-full grow">
        <!-- List item -->
        <li>
          <button class="{activeTab === 'home' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Homepage" onclick={() => navigate('home')}>
            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
              <path stroke-linecap="round" stroke-linejoin="round" d="m2.25 12 8.954-8.955c.44-.439 1.152-.439 1.591 0L21.75 12M4.5 9.75v10.125c0 .621.504 1.125 1.125 1.125H9.75v-4.875c0-.621.504-1.125 1.125-1.125h2.25c.621 0 1.125.504 1.125 1.125V21h4.125c.621 0 1.125-.504 1.125-1.125V9.75M8.25 21h8.25" />
            </svg>
            <span class="max-lg:is-drawer-close:hidden">Home</span>
          </button>
        </li>
                <!-- List item -->
        <li>
          <button class="{activeTab === 'sequence-selector' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Sequence Selector" onclick={() => navigate('sequence-selector')}>
            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
              <path stroke-linecap="round" stroke-linejoin="round" d="M9.813 15.904 9 18.75l-.813-2.846a4.5 4.5 0 0 0-3.09-3.09L2.25 12l2.846-.813a4.5 4.5 0 0 0 3.09-3.09L9 5.25l.813 2.846a4.5 4.5 0 0 0 3.09 3.09L15.75 12l-2.846.813a4.5 4.5 0 0 0-3.09 3.09ZM18.259 8.715 18 9.75l-.259-1.035a3.375 3.375 0 0 0-2.455-2.456L14.25 6l1.036-.259a3.375 3.375 0 0 0 2.455-2.456L18 2.25l.259 1.035a3.375 3.375 0 0 0 2.456 2.456L21.75 6l-1.035.259a3.375 3.375 0 0 0-2.456 2.456ZM16.894 20.567 16.5 21.75l-.394-1.183a2.25 2.25 0 0 0-1.423-1.423L13.5 18.75l1.183-.394a2.25 2.25 0 0 0 1.423-1.423l.394-1.183.394 1.183a2.25 2.25 0 0 0 1.423 1.423l1.183.394-1.183.394a2.25 2.25 0 0 0-1.423 1.423Z" />
            </svg>
            <span class="max-lg:is-drawer-close:hidden">Sequence Selector</span>
          </button>
        </li>
        <!-- List item -->
        <li>
          <button class="{activeTab === 'sequence-generator' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Sequence Generator" onclick={() => navigate('sequence-generator')}>
            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
              <path stroke-linecap="round" stroke-linejoin="round" d="M9.75 3.104v5.714a2.25 2.25 0 0 1-.659 1.591L5 14.5M9.75 3.104c-.251.023-.501.05-.75.082m.75-.082a24.301 24.301 0 0 1 4.5 0m0 0v5.714c0 .597.237 1.17.659 1.591L19.8 15.3M14.25 3.104c.251.023.501.05.75.082M19.8 15.3l-1.57.393A9.065 9.065 0 0 1 12 15a9.065 9.065 0 0 0-6.23-.693L5 14.5m14.8.8 1.402 1.402c1.232 1.232.65 3.318-1.067 3.611A48.309 48.309 0 0 1 12 21c-2.773 0-5.491-.235-8.135-.687-1.718-.293-2.3-2.379-1.067-3.61L5 14.5" />
            </svg>
            <span class="max-lg:is-drawer-close:hidden">Sequence Generator</span>
          </button>
        </li>
        <!-- List item -->
        <li>
          <button class="{activeTab === 'module-monitor' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Module Monitor" onclick={() => navigate('module-monitor')}>
              <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
                <path stroke-linecap="round" stroke-linejoin="round" d="M8.25 3v1.5M4.5 8.25H3m18 0h-1.5M4.5 12H3m18 0h-1.5m-15 3.75H3m18 0h-1.5M8.25 19.5V21M12 3v1.5m0 15V21m3.75-18v1.5m0 15V21m-9-1.5h10.5a2.25 2.25 0 0 0 2.25-2.25V6.75a2.25 2.25 0 0 0-2.25-2.25H6.75A2.25 2.25 0 0 0 4.5 6.75v10.5a2.25 2.25 0 0 0 2.25 2.25Zm.75-12h9v9h-9v-9Z" />
              </svg>
            <span class="max-lg:is-drawer-close:hidden">Module Monitor</span>
          </button>
        </li>
        <!-- List item -->
        <li>
          <button class="{activeTab === 'status' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Status" onclick={() => navigate('status')}>
             <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
              <path stroke-linecap="round" stroke-linejoin="round" d="M8.288 15.038a5.25 5.25 0 0 1 7.424 0M5.106 11.856c3.807-3.808 9.98-3.808 13.788 0M1.924 8.674c5.565-5.565 14.587-5.565 20.152 0M12.53 18.22l-.53.53-.53-.53a.75.75 0 0 1 1.06 0Z" />
            </svg>
            <span class="max-lg:is-drawer-close:hidden">Status</span>
          </button>
        </li>
        <!-- List item -->
        <li>
          <button class="{activeTab === 'advanced' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Advanced" onclick={() => navigate('advanced')}>
             <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
              <path stroke-linecap="round" stroke-linejoin="round" d="M12 9v3.75m-9.303 3.376c-.866 1.5.217 3.374 1.948 3.374h14.71c1.73 0 2.813-1.874 1.948-3.374L13.949 3.378c-.866-1.5-3.032-1.5-3.898 0L2.697 16.126ZM12 15.75h.007v.008H12v-.008Z" />
              </svg>
            <span class="max-lg:is-drawer-close:hidden">Advanced</span>
          </button>
        </li>
        <li>
          <button class="{activeTab === 'config' ? 'active text-accent' : ''} max-lg:is-drawer-close:tooltip max-lg:is-drawer-close:tooltip-right" data-tip="Configuration" onclick={() => navigate('config')}>
            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor" class="size-6">
              <path stroke-linecap="round" stroke-linejoin="round" d="M9.594 3.94c.09-.542.56-.94 1.11-.94h2.593c.55 0 1.02.398 1.11.94l.213 1.281c.063.374.313.686.645.87.074.04.147.083.22.127.325.196.72.257 1.075.124l1.217-.456a1.125 1.125 0 0 1 1.37.49l1.296 2.247a1.125 1.125 0 0 1-.26 1.431l-1.003.827c-.293.241-.438.613-.43.992a7.723 7.723 0 0 1 0 .255c-.008.378.137.75.43.991l1.004.827c.424.35.534.955.26 1.43l-1.298 2.247a1.125 1.125 0 0 1-1.369.491l-1.217-.456c-.355-.133-.75-.072-1.076.124a6.47 6.47 0 0 1-.22.128c-.331.183-.581.495-.644.869l-.213 1.281c-.09.543-.56.94-1.11.94h-2.594c-.55 0-1.019-.398-1.11-.94l-.213-1.281c-.062-.374-.312-.686-.644-.87a6.52 6.52 0 0 1-.22-.127c-.325-.196-.72-.257-1.076-.124l-1.217.456a1.125 1.125 0 0 1-1.369-.49l-1.297-2.247a1.125 1.125 0 0 1 .26-1.431l1.004-.827c.292-.24.437-.613.43-.991a6.932 6.932 0 0 1 0-.255c.007-.38-.138-.751-.43-.992l-1.004-.827a1.125 1.125 0 0 1-.26-1.43l1.297-2.247a1.125 1.125 0 0 1 1.37-.491l1.216.456c.356.133.751.072 1.076-.124.072-.044.146-.086.22-.128.332-.183.582-.495.644-.869l.214-1.28Z" />
              <path stroke-linecap="round" stroke-linejoin="round" d="M15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0Z" />
            </svg>
            <span class="max-lg:is-drawer-close:hidden">Configuration</span>
          </button>
        </li>

        <div class="flex-1"></div>
        
        <li class="px-4 py-2 max-lg:is-drawer-close:hidden">
              
          <label class="flex cursor-pointer gap-2">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="20"
            height="20"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round">
            <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path>
          </svg>
          <input type="checkbox" value="light" class="toggle theme-controller" checked />
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="20"
            height="20"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            stroke-width="2"
            stroke-linecap="round"
            stroke-linejoin="round">
            <circle cx="12" cy="12" r="5" />
            <path
              d="M12 1v2M12 21v2M4.2 4.2l1.4 1.4M18.4 18.4l1.4 1.4M1 12h2M21 12h2M4.2 19.8l1.4-1.4M18.4 5.6l1.4-1.4" />
          </svg>
        </label>
        </li>
      </ul>
    </div>
  </div>
</div>

</main>


