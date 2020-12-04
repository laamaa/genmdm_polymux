# genmdm_polymux

Preset manager + Poly mode enabler for Sega Megadrive GenMDM MIDI Interface + Retrokits RK-002 Midi cable

Default settings:
* MIDI Channel 1 routes to GenMDM channels 1-5, CCs are sent to all of the channels.
* MIDI Channel 2 routes to GenMDM channels 7-9 (PSG)

New CCs:
* CC #116: Select preset slot (data range: 0-10, maximum can be changed with NUM_PRESETS definition)
* CC #117: Send currently selected preset to device (data range: n/a, all values trigger the function)
* CC #118: Store all presets in RK002 flash (data range: n/a, all values trigger the function)
* CC #119: Send all presets to device and store to RAM slots (data range: n/a, all values trigger the function)

I've used this successfully with the included TouchOSC layout.

* Flash the cable+connect to GenMDM (Use 10K program/4K flash memory layout under Tools menu when flashing)
* Open the TouchOSC layout, set a value to all available controls (except preset rotary)
* Setup a decent default sound of your liking and click through each of the preset slots, pressing save after switching slots.
* Now you should be able to create your killer sounds to your preset slots without too many troubles.

There's also a simple html/javascript webmidi tool available for importing .TFI presets. It should work with Chrome.
