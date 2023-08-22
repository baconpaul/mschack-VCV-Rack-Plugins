# mscHack Plugins 

The mscHack plugins [here](https://github.com/mschack/VCV-Rack-Plugins) were 
never ported to Rack 2.0. This repo is a fork and fix of mscRack 1.1.0 by pgatt and baconpaul
trying to bring these back.  To date we have not been able to reach the original author but
would happily return these changes to them.

A few notes

1. Please report issue [here](https://github.com/baconpaul/mschack-VCV-Rack-Plugins/issues/) and
not to the original author
2. Please check @pgatt's longer post on the community forum for discussion and details
3. We may decide to abandon this project, even though the port is materially
complete. With the original author not responding, a path to the library is 
still a bit murky.

![screenshot](modules.PNG)

# Updates
- 10 July 19 [v1.0.2]
	-Re-added compressor
	-New Module "Lorenz" noise generator
	-Fix 4ch mixer mute/solo
- 30 June 19 [v1.0.1]
	-Re-added Step Delay and Maude221
- 29 June 19 [v1.0.0]
	- Updates mostly for Rack version 1.0
		- Some fixes here and there
		- Mixer inputs do receive poly inputs

	- Removed the following:
		- Compressor, 3x16 step sequencer, XFade
		- obsolete mixers (1x4 2x4 and 4x4)
		- If I get enough feedback for a certain modules return I will consider it

	- Added an even slower mod speed for Dronez, Windz etc..

# Includes
- "ENVY 9", a 9 channel envelope editor/generator
- 4 Channel Master Clock
- 6 Channel 32 Step Sequencer
- Mixers 4_0_4, 9_3_4, 16_4_4 and 24_4_4 ( Audio channels - Groups - AUX channels )
- Triad Sequencer, with Independantly clocked channels
- Synth Drums
- 3 Channel Oscillator with Filter and Amp Envelope 
- Wave Morph Oscillator, Morph between 3 editable waveforms.  With filter.
- Ping Pong Delay
- ARP 700, 7 note arpeggiator.
- The 'Z' collection: Alienz, Dronez, Morze and Windz
- ASAF-8, 8 channel Auto Stereo Audio Fader
