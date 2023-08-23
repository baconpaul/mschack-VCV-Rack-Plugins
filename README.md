# mscHack Plugins 

The mscHack plugin [here](https://github.com/mschack/VCV-Rack-Plugins) were 
never ported to Rack 2.0. This repo is a fork of mscRack 1.1.0 by baconpaul and pgatt attempting to bring these back.  The original author has indicated they are fine with us proceeding with this attempt.

A few notes

- Please report issues [here](https://github.com/baconpaul/mschack-VCV-Rack-Plugins/issues/) and
not to the original author
- Please check @pgatt's longer post on the community forum for discussion and details
- If there are too many bugs in community testing, we may still abandon the project! It's a bit of a side-quest
for both of us.


![screenshot](modules.PNG)

# Updates
- Aug 23 23 [v2.0.0]
	- baconpaul and pgatt attempt a port over to Rack 2. Gulp!
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
