# Endless Orgasm Machine - An Automated Orgasm Denial Device for ESP32

Using an inflatable butt plug to detect pressure changes indicative of pelvic floor contractions, this
software, and associated hardware, is used to detect when the user is approaching orgasm and control
stimulation accordingly. The net result: automated edging and orgasm denial.  It can also be used to allow orgasms then
determine when the refractory period has subsided enough to begin the next round of pleasuring. 

## Setup 
- Install [VSCode](https://code.visualstudio.com/) with the [PlatformIO](https://platformio.org/) extension
- Open this repo and let all the PlatformIO dependencies download
- Configure your wifi ssid and password at the top of [data/config.json](data/config.json)
- Under ```PlatformIO -> Project Tasks -> esp32dev -> Platform``` choose ```Build Filesystem Image``` then ```Upload Filesystem Image```
- Under ```PlatformIO -> Project Tasks -> esp32dev -> General``` choose ```Build``` then eventually ```Upload and Monitor```
- Watch for your device's IP address to be displayed in the monitor window

## Web UI

[The UI](eomui/README.md) is hosted on the ESP32 itself and can be found by visiting the IP address of your device using https.  You can watch the serial output to determine the IP or use your own means.  If you enable a screen the IP address will be displayed on it.  

Example: If the IP is ```192.168.1.2``` you should visit ```https://192.168.1.2/ui``` (you will need to accept the security warning for self-signed certificates).  It will automatically open a websocket connection to that endpoint at ```https://192.168.1.2/```  

## Wait, isn't this the Edge-o-Matic 3000?

This is a fork of that project with a different set of goals.  The main goal is to make the project accessible to the maker community by keeping the hardware and software simple and DIY friendly - like [the original Nogasm](https://github.com/nogasm/nogasm) that started it all was.  The Edge-o-Matic code is designed around selling well-made but proprietary hardware and is [locked into it through closed-source code](https://github.com/MausTec/eom-hal-dist), crippling attempts to use it with DIY hardware.  It also included a lot of code to connect to related devices and services.

This project is vastly simplified.  You can try it using almost any modern ESP32 device with no additional customization.  You will need to connect a pressure sensor and inflatable buttplug to make it functional, but that's it.  It's meant to be operated wirelessly rather than through the additional hardware of a screen, knob, LEDs, network jack, etc.

## WebSocket API

Documentation for the WebSocket API can be found in [doc/WebSocket.md](doc/WebSocket.md).

## BLE Usage
Using Xtoys you can connect to the EOM and let it control your other Xtoys devices.  It emulates a [MonsterPub Mr. Devil Kegel (since renamed)](https://monsterpub.com/products/mp2-u-shaped-remote-egg-with-kegel).  The "pressure" reading corresponds to the "pleasure" output of the EOM.  Vibrator strength can be controlled in manual mode.  

Why Mr Devil?  Because it's one of the few BT devices in Xtoys that has 2 way communication and high resolution (16 bit).  If you wish to connect Xtoys to the EOM over wifi you can choose the Edge-o-matic 3000 and it is backwards compatible.  This is less useful because Xtoys only chose to implement "arousal" and "pressure" inputs, so all the logic that goes into calculating the amount of pleasure is ignored.  

If you have the skills you could control other toys through bluetooth with your own controller rather than using Xtoys, but its popular and free.

### Vibration Modes:

|ID|Name|Description|
|---|---|---|
|1|Ramp-Stop|Vibrator ramps up from set min speed to max speed, stopping abruptly on arousal threshold crossing.|
|2|Depletion (default)|Vibrator speed ramps up from min to max, but is reduced as arousal approaches threshold.|
|3|Enhancement|Vibrator speed ramps up as arousal increases, holding a peak for ramp_time.|
|0|Global Sync|When set on secondary vibrators, they will follow the primary vibrator speed.|

### post_orgasm_duration_seconds:
|Seconds|Description|
|---|---|
|0|Turn off stimulation at orgasm dectection - Failed Orgasm|
|5-15|Turn off stimulation after amount of seconds - Normal orgasm|
|16-4095|Turn off stimulation after amount of seconds - Post orgasm Torture|

## Hardware

This can be tested on almost any ESP32 device without any extra hardware by touching the pressure sensor pin.  This will simulate a pressure sensor being squeezed.  To actually play with it you will need to attach a pressure sensor and plug.  The [MPX5100DP](https://www.digikey.com/en/products/detail/nxp-usa-inc/MPX5100DP/464060) is the sensor it was designed around but anything capable of 15psi or greater should work whether analog or SPI.  Plug the port of the pressure sensor into the air hose for the butt plug.  

To get it to resemble [the original Nogasm device](https://github.com/nogasm/nogasm) you can then add a [12v power supply](https://www.amazon.com/ACEIRMC-Battery-Plastic-Storage-Connect/dp/B0986RMKBJ) and [charger](https://www.amazon.com/Battery-Charger-Lithium-Display-RC123A/dp/B0CRKSFTK9), a [simple transistor](https://www.amazon.com/ALLECIN-IRF4905-Transistors-IRF4905PBF-Transistor/dp/B0CBKGJT9N) or more protected [motor controller](https://www.amazon.com/High-Power-Adjustment-Electronic-Controller-Brightness/dp/B0DZP1NCVW), a [flyback diode](https://www.amazon.com/15SQ045-Diodes-Schottky-Blocking-Silicon/dp/B0D4F2WVS5), and [vibrator motor](https://www.amazon.com/RPTCOTU-R555-Vibration-Motor-Electrodynamic/dp/B0CSYWK5KQ).  You could share that power supply to the esp32 [through a buck converter](https://www.amazon.com/Regulator-Reducer-Converter-Aircraft-MP1584EN/dp/B0B779ZYN1) to make the whole thing wireless.  That's it.  You can set this all up to comfortably attach to the buttplug rather than having wires and tubes dangling out.  

### Customization suggestions 
- You can use xtoys as a hub to ramp up any number of vibrators, strokers, or e-stim units in sync with the EOM
- You can use an inexpensive [Vibrating inflatable buttplug](https://www.amazon.com/Lovehoney-Black-Inflatable-Vibrating-Back/dp/B092VVXM63) and remove its control box to wire it directly to the EOM and power supply.  A [non-vibrating plug](https://www.amazon.com/Inflatable-Expandable-Stimulator-Beginners-Detachable/dp/B0DSPKVPM1) is even less expensive if you'll be controlling your toys wirelessly.
- If the pressure sensor is too inconvenient to source you could use a [standard car exhaust pressure sensor](https://www.amazon.com/dp/B0997VKYQ9).  The pin with the notch is Vin, middle is ground, third is Vout.
- If you use an ESP32 board with an integrated screen ([Example 1](https://www.amazon.com/ideaspark-Development-Integrated-Wireless-Micropython), [Example 2](https://www.amazon.com/Waveshare-Development-Frequency-Single-Core-Processor/dp/B0DHTMYTCY)) you can enable the screen in [include/config.h](include/config.h) to see the IP of the device.
- As an alternative to a vibrating buttplug you can use [a plug made for enema play](https://www.amazon.com/Inflatable-Congestion-Cleaning-Expansion-Beginner/dp/B0CZRLPLQC) and pass a wire through the middle [to a vibrator on top](https://www.amazon.com/dp/B0024XI1LG)
- Someone posted build that used tubing wrapped around a 3d printed shaft that also housed the electronics.  It was a neat new twist to the inflatable plug since the tubing reacted to pressure just like an inflatable.
- You could put in a higher range pressure sensor if you prefer to inflate the plug to more intense levels.  The measurement is of change in pressure, not overall pressure.
- You could order a [prebuilt PCB like is used for the Nogasm-esp32](https://github.com/Mathew3000/nogasm-esp32) 
- There really are a ridiculous number of implementations of the nogasm concept from the last decade...you should do some googling.



### To-do
- Stand-alone wifi option
- ui and websockets using same endpoint if possible
- 2nd motor support from UI
- pinouts and schematics for suggested hardware
- finish integrated screen support
- finish bt motor control
- chart glitch on left side at startup
- X axis on chart
- Put some effort into UI styling
- LED and encoder options?
- UI crashing with rapid motor speed adjustments?
- Less BT logging
- Running average math seems off
- Implement patterns?
- Doesn't handle more than 2 clients well

### Notable differences from Edge-o-matic
- Not tied to specific hardware
- UI hosted on device
- UI is phone-friendly
- communicates with xtoys via BLE
- defaults to automatic orgasm denial in ramp-stop mode
- currently doesn't connect to lovense devices directly
- screen is optional
- SD card requirement removed
- menu system removed
- console system removed
- hardwire networked device support removed 
- edge times are in seconds
- smaller build size

