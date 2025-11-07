import { mount } from 'svelte'
import './app.css'
import App from './App.svelte'

if ('serviceWorker' in navigator) { navigator.serviceWorker.register('sw.js'); }

const app = mount(App, {
  target: document.getElementById('app')!,
})

export default app
