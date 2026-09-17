<script lang="ts">
    import type { SystemStatus } from './interfaces';
    import { apiRequest, type ApiResponse } from './api';
    import { onMount } from 'svelte';
    import { busyStore } from './services/busy.svelte';
    import { ENDPOINTS } from './config';

    let status = $state<ApiResponse<SystemStatus>>({error: null, loading: false, data: {
        canStarted: false,
        canSpeedKbps: 0,
        canSentCounter: 0,
        canReceivedCounter: 0,
        canFailedCounter: 0,
        cpuFreqMHz: 0,
        chipModel: '',
        memoryFreeHeap: 0,
        memoryMaxBlock: 0,
        memoryMinFreeHeap: 0,
        psramFree: 0,
        psramSize: 0,
        wifiChannel: 0,
        wiFiConnected: false,
        wifiGatewayIP: '',
        wifiIp: '',
        wifiMACAddress: '',
        wifiMDNSName: '',
        wifiQuality: 0,
        wifiRssi: 0,
        uptimeSeconds: 0
        }});
    
    let uptime = $derived.by(() => {
        if(!status || !status.data)
            return;

        const h = Math.floor(status.data.uptimeSeconds / 3600).toString().padStart(2, '0');
        const m = Math.floor((status.data.uptimeSeconds % 3600) / 60).toString().padStart(2, '0');
        const s = (status.data.uptimeSeconds % 60).toString().padStart(2, '0');
        return `${h}:${m}:${s}`;
    });

    
    async function load() {
        await busyStore.run(async () => {
            status = await apiRequest<SystemStatus>(ENDPOINTS.GET_SYSTEM_STATUS);
        });
	}

    onMount(() => {
		load();

        const interval = setInterval(() => {
            if(status.data){
                status.data.uptimeSeconds++;
            }
            }, 1000);

        return () => clearInterval(interval);
	});

</script>

<div class="p-2">
	<div class="flex items-center justify-between mb-4">
        <button class="btn btn-sm " onclick="{() => { load() }}" title="refresh">
                <svg class="size-[1.2em]" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="currentColor">
                    <path fill-rule="evenodd" d="M4.755 10.059a7.5 7.5 0 0 1 12.548-3.364l1.903 1.903h-3.183a.75.75 0 1 0 0 1.5h4.992a.75.75 0 0 0 .75-.75V4.356a.75.75 0 0 0-1.5 0v3.18l-1.9-1.9A9 9 0 0 0 3.306 9.67a.75.75 0 1 0 1.45.388Zm15.408 3.352a.75.75 0 0 0-.919.53 7.5 7.5 0 0 1-12.548 3.364l-1.902-1.903h3.183a.75.75 0 0 0 0-1.5H2.984a.75.75 0 0 0-.75.75v4.992a.75.75 0 0 0 1.5 0v-3.18l1.9 1.9a9 9 0 0 0 15.059-4.035.75.75 0 0 0-.53-.918Z" clip-rule="evenodd" />
                </svg>
                Refresh
        </button>
    </div>

	{#if status.error}
        <div role="alert" class="alert alert-error mb-4">
        <svg xmlns="http://www.w3.org/2000/svg" class="h-6 w-6 shrink-0 stroke-current" fill="none" viewBox="0 0 24 24">
            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z" />
        </svg>
        <span>{status.error}</span>
        </div>
	{/if}
    {#if status.data }
		<div class="grid gap-4 md:grid-cols-2 items-start">

			<!-- WiFi -->
			<div class="card p-4 shadow-sm overflow-x-auto">
                <div class="text-base p-2 font-semibold mb-2 flex uppercase">
                    WiFi
                </div>
                <table class="table table-sm">
                    <tbody>
                    <tr>
                        <th>WiFi Status</th>
                        <td>
                            <div class="inline-grid *:[grid-area:1/1] align-middle">
                                <div class="status {status.data.wiFiConnected ? 'status-success' : 'status-error'} animate-ping"></div>
                                <div class="status {status.data.wiFiConnected ? 'status-success' : 'status-error'}"></div>
                            </div>
                            {status.data.wiFiConnected ? 'Connected' : 'Disconnected'}
                        </td>
                    </tr>
                    <tr>
                        <th>WiFi Quality</th>
                        <td>{status.data.wifiQuality} ({status.data.wifiRssi}dB)</td>
                    </tr>
                    <tr>
                        <th>IP</th>
                        <td>{status.data.wifiIp}</td>
                    </tr>
                    <tr>
                        <th>MDNS Name</th>
                        <td>{status.data.wifiMDNSName}</td>
                    </tr>
                    <tr>
                        <th>WiFi Channel</th>
                        <td>{status.data.wifiChannel}</td>
                    </tr>
                    <tr>
                        <th>WiFi Gateway IP</th>
                        <td>{status.data.wifiGatewayIP}</td>
                    </tr>
                    <tr>
                        <th>WiFi MAC Address</th>
                        <td>{status.data.wifiMACAddress}</td>
                    </tr>
                    </tbody>
                </table>
            </div>

			<!-- CAN Bus -->
			<div class="card p-4 shadow-sm overflow-x-auto">
                <div class="text-base p-2 font-semibold mb-2 flex uppercase">
                    CAN Bus
                </div>
                <table class="table table-sm">
                    <tbody>
                    <tr>
                        <th>CAN Status</th>
                        <td>
                            <div class="inline-grid *:[grid-area:1/1] align-middle">
                                <div class="status {status.data.canStarted ? 'status-success' : 'status-error'} animate-ping"></div>
                                <div class="status {status.data.canStarted ? 'status-success' : 'status-error'}"></div>
                            </div>
                            {status.data.canStarted ? 'Started' : 'Stopped'}
                        </td>
                    </tr>
                    <tr>
                        <th>CAN Speed</th>
                        <td>{status.data.canSpeedKbps} kbit/s</td>
                    </tr>
                    <tr>
                        <th>Sent messages</th>
                        <td>{status.data.canSentCounter.toLocaleString()}</td>
                    </tr>
                    <tr>
                        <th>Received messages</th>
                        <td>{status.data.canReceivedCounter.toLocaleString()}</td>
                    </tr>
                    <tr>
                        <th>Failed messages</th>
                        <td class={status.data.canFailedCounter > 0 ? 'text-error font-semibold' : ''}>{status.data.canFailedCounter.toLocaleString()}</td>
                    </tr>
                    </tbody>
                </table>
            </div>

			<!-- Memory -->
			<div class="card p-4 shadow-sm overflow-x-auto">
                <div class="text-base p-2 font-semibold mb-2 flex uppercase">
                    Memory
                </div>
                <table class="table table-sm">
                    <tbody>
                    <tr>
                        <th>Free Heap</th>
                        <td>{status.data.memoryFreeHeap.toLocaleString()}</td>
                    </tr>
                    <tr>
                        <th>Max Block</th>
                        <td>{status.data.memoryMaxBlock.toLocaleString()}</td>
                    </tr>
                    <tr>
                        <th>Min Free Heap</th>
                        <td>{status.data.memoryMinFreeHeap.toLocaleString()}</td>
                    </tr>
                    <tr>
                        <th>PSRAM Size</th>
                        <td>{status.data.psramSize.toLocaleString()}</td>
                    </tr>
                    <tr>
                        <th>PSRAM Free</th>
                        <td>{status.data.psramFree.toLocaleString()}</td>
                    </tr>
                    </tbody>
                </table>
            </div>

			<!-- System -->
			<div class="card p-4 shadow-sm overflow-x-auto">
                <div class="text-base p-2 font-semibold mb-2 flex uppercase">
                    System
                </div>
                <table class="table table-sm">
                    <tbody>
                    <tr>
                        <th>ESP32 Type</th>
                        <td>{status.data.chipModel}</td>
                    </tr>
                    <tr>
                        <th>Processor Frequency</th>
                        <td>{status.data.cpuFreqMHz} MHz</td>
                    </tr>
                    <tr>
                        <th>Uptime</th>
                        <td>{uptime}</td>
                    </tr>
                    </tbody>
                </table>
            </div>

		</div>
        {/if}
</div>