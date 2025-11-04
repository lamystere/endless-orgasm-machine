import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'
//import basicSsl from '@vitejs/plugin-basic-ssl'
import { viteSingleFile } from "vite-plugin-singlefile"

export default defineConfig({
  plugins: [svelte(), 
    //basicSsl(), 
    viteSingleFile()]
})

