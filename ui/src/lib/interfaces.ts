export interface SystemStatus {
    canStarted: boolean;
    canSpeedKbps: number;
    canSentCounter: number;
    canReceivedCounter: number;
    canFailedCounter: number;
    cpuFreqMHz: number;
    chipModel: string;
    memoryFreeHeap: number;
    memoryMaxBlock: number;
    memoryMinFreeHeap: number;
    psramFree: number;
    psramSize: number;
    wifiChannel: number;
    wiFiConnected: boolean;
    wifiGatewayIP: string;
    wifiIp: string;
    wifiMACAddress: string;
    wifiMDNSName: string;
    wifiQuality: number;
    wifiRssi: number;
    uptimeSeconds: number;
}

export interface Sequence {
    name: string;
    type: number;
    description: string;
    speed: number;
    intensity: number;
    data: string;
}

export interface SequenceGenerator extends Sequence {
    intensityControl: boolean; // these govern what controls are shown in the UI
    speedControl: boolean;
    dataControl: boolean;
}

export interface SavedSequence extends Sequence {
    // breaking this interface out to add more saved sequence specific fields in future
    fileName: string;
}

export interface SequenceStatus {
    name: string;
    description: string;
    type: number;
    loop: boolean;
    paused: boolean;
    playing: boolean;
    speed: number;
    intensity: number;
    data: string;
}

export interface SequenceSaveResult {
    result: string;
    fileName: string;
    name: string;
    description: string;
}

export interface MoveCommand {
    target: number;
    speed: number;
    priority: number;
    pixels: Array<Array<number>>; // array of [X,Y] arrays
}

export interface SystemConfig{
    pixelMap: Array<Array<Array<number>>>;
    maxTarget: number;
    homingTarget: number;
    canId: number;
    posixTimezoneString: string;
    debug: boolean;
}

export interface InstallationSpec{
    pixelCount: number;
    controlModuleCount: number;
    rows: number;
    cols: number;
}

export interface ModuleStatus{
    moduleId: number;
    status: number;
}

export interface TestCommand {
    moduleId: number;
    nodeId: number;
}