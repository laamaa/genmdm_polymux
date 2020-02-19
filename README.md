# genmdm_polymux

Preset manager + Poly mode enabler for Sega Megadrive GenMDM MIDI Interface + Retrokits RK-002 Midi cable

Default settings:
* MIDI Channel 1 routes to GenMDM channels 1-5, CCs are sent to all of the channels.
* MIDI Channel 2 routes to GenMDM channels 7-9 (PSG)

I've used this successfully with the included TouchOSC layout.

* Flash the cable+connect to GenMDM (Use 10K program/4K flash memory layout under Tools menu when flashing)
* Open the TouchOSC layout, set a value to all available controls (except preset rotary)
* Setup a decent default sound of your liking and click through each of the preset slots, pressing save after switching slots.
* Now you should be able to create your killer sounds to your preset slots without too many troubles.

Todo: 
* Mod wheel for PSG 