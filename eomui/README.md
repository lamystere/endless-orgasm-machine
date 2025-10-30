# Endless Orgasm Machine UI

This is a UI for the Endless Orgasm Machine, a DIY-friendly fork of the Endless Orgasm Machine.  It is extremely lightweight, built using Svelte and Vite only, and meant to be stored in spiffs memory on an ESP32.  The prebuilt output is already included with the larger EOM project.  This source is included to encourage tinkering and enhancements from the maker community.

It can be run locally if you prefer.  It should be backwards compatible with the Endless Orgasm Machine though missing some features like denial count.

## Use
- determine the IP address of your EOM device after it boots up (you can monitor the serial output to see its IP)
- Visit the IP using ```https://ip.addy.goes.here``` in a browser to see this UI

## Building 
- from the eomui directory run ```npm install``` then ```npm run build```
- the output ```index.html``` will automatically overwrite the version in the /data directory of the EOM project.
- using platformIO tools ```build filesystem``` then ```upload filesystem``` to update your device
- the EOM code itself doesn't need to be built/uploaded again


## Development
- run ```vite```
- visit ```https://localhost:5173/``` in your browser
- enter the ip address of your EOM and hit connect - it will open a websocket connection and request a stream of readings
- to speed up development the IP address can be entered into the code on line 8 of [/src/App.svelte](/src/App.svelte)

## License
This UI was not derived from any previous projects and is released under GPL courtesy of Lamystere.  If you aren't familiar with GPL it basically means you can do whatever you want with the code but anything you make with it should also be released under GPL.