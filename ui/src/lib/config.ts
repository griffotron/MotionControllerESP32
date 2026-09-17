// Automatically detects if you are in development (npm run dev) 
// or production (running on the ESP32)
const IS_DEV = import.meta.env.DEV;

// Using proxy so not necessary, but could be useful
export const BASE_URL = IS_DEV ? '' : '';


export const ENDPOINTS = {
    POST_SEQ_GENERATE:  `${BASE_URL}/api/sequence/generate`,
    POST_SEQ_LOAD:      `${BASE_URL}/api/sequence/load`,
    POST_SEQ_CONTROL:   `${BASE_URL}/api/sequence/control`,
    POST_SEQ_SAVE:      `${BASE_URL}/api/sequence/save`,
    POST_SEQ_DELETE:    `${BASE_URL}/api/sequence/delete`,
    POST_SEQ_DEMO:      `${BASE_URL}/api/sequence/demo`,
    GET_SEQ_JSON:       `${BASE_URL}/api/sequence/json`,
    GET_SEQ_GENERATORS: `${BASE_URL}/api/sequence/generators`,
    GET_SEQ_SAVED:      `${BASE_URL}/api/sequence/saved`,
    POST_MOTION_MOVE:   `${BASE_URL}/api/motion/move`,
    POST_MOTION_HOME:   `${BASE_URL}/api/motion/home`,
    POST_MOTION_ZERO:   `${BASE_URL}/api/motion/zero`,
    POST_MODULE_RESET:  `${BASE_URL}/api/module/reset`,
    GET_SYSTEM_STATUS:  `${BASE_URL}/api/system/status`,
    GET_SYSTEM_CONFIG:  `${BASE_URL}/api/system/config`,
    POST_SYSTEM_CONFIG: `${BASE_URL}/api/system/config`,
    PUT_SYSTEM_REBOOT:  `${BASE_URL}/api/system/reboot`,
    GET_MODULES:        `${BASE_URL}/api/system/modules`,
    WEB_SOCKET:         `${BASE_URL}/ws`,
};

export const SEQUENCE_NAMES = {
    CLOCK: 'clock'
}