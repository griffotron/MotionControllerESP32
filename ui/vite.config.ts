import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
import tailwindcss from '@tailwindcss/vite'
import { viteSingleFile } from "vite-plugin-singlefile"
import { compression } from "vite-plugin-compression2"
import path from 'path';

// https://vite.dev/config/
export default defineConfig({
  plugins: [
      svelte(),
      tailwindcss(),
      viteSingleFile(), // Bundle content into single file...
      compression({ deleteOriginalAssets: true, skipIfLargerOrEqual: false }) //gzip it
    ],
  resolve:{
    alias:{
      '$lib': path.resolve(__dirname, './src/lib')
    }
  },
  build: {
    // 1. Force all assets into the root folder (no /assets/ subfolder)
    assetsDir: './', 
    // 2. Clear the folder before building
    emptyOutDir: true,
    // 3. Optional: Minify more aggressively for ESP32
    target: 'esnext',
    // Embed bigger assets directly into the output HTML
    assetsInlineLimit: 4096000
  },
  server: {
    proxy: {
      '/api': 'http://motioncontroller.local/', // Your ESP32's actual IP
      '/ws': 'ws://motioncontroller.local/' // Your ESP32's actual IP
    }
  }
})
