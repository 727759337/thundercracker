#ifndef __CONFIG_H__
#define __CONFIG_H__

#define CUBE_ID_BASE	0
#define NUM_CUBES		3
#define MUSIC_ON      	0
#define SFX_ON        	1
#define LOAD_ASSETS   	1

#define GFX_ARTIFACT_WORKAROUNDS	1

#if SIFTEO_SIMULATOR
	// these settings only applied to the emulator build
	#define FAST_FORWARD	1
#else
	// these settings only applied to the real hardware build
	#define FAST_FORWARD	0
#endif

#endif

