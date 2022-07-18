# VGM Tools 
by Valley Bell (@valleybell)

## Tool List
[dro2vgm - DRO to VGM Converter](#dro-to-vgm-converter-dro2vgm)  
[imf2vgm - IMF to VGM Converter](#imf-to-vgm-converter-imf2vgm)  
[opl_23 - OPL 2<->3 Converter](#opl-2-3-converter-opl_23)  
[opt_oki - VGM OKIM6258 Optimizer](#vgm-okim6258-optimizer-opt_oki)  
[optdac - VGM DAC Optimizer](#vgm-dac-optimizer-optdac)  
[optvgm32 - VGM PWM Optimizer](#vgm-pwm-optimizer-optvgm32)  
[optvgmrf - VGM RF-PCM Optimizer](#vgm-rf-pcm-optimizer-optvgmrf)  
[raw2vgm - RAW to VGM Converter](#raw-to-vgm-converter-raw2vgm)  
[vgm_cmp - VGM Compressor](#vgm-compressor-vgm_cmp)  
[vgm_cnt - VGM Command Counter](#vgm-command-counter-vgm_cnt)  
[vgm_dbc - VGM Data Block Compressor](#vgm-data-block-compressor-vgm_dbc)  
[vgm_dso - VGM DAC Stream Optimizer](#vgm-dac-stream-optimizer-vgm_dso)  
[vgm_facc - Make VGM Frame Accurate](#make-vgm-frame-accurate-vgm_facc)  
[vgm_mono - VGM Mono](#vgm-mono-vgm_mono)  
[vgm_ndlz - VGM Undualizer](#vgm-undualizer-vgm_ndlz)  
[vgm_ptch - VGM Patcher](#vgm-patcher-vgm_ptch)  
[vgm_ren.c - VGM Renamer](#vgm-renamer-vgm_ren)  
[vgm_smp1 - Remove 1 Sample Delays](#remove-1-sample-delays-vgm_smp1)  
[vgm_sptd - VGM Splitter (Delay Edition)](#vgm-splitter-delay-edition-vgm_sptd)  
[vgm_spts - VGM Splitter (Sample Edition)](#vgm-splitter-sample-edition-vgm_spts)  
[vgm_sro - VGM Sample-ROM Optimizer](#vgm-sample-rom-optimizer-vgm_sro)  
[vgm_stat - VGM Statistics](#vgm-statistics-vgm_stat)  
[vgm_tag - VGM Tagger](#vgm-tagger-vgm_tag)  
[vgm_tt - VGM Tag Transfer](#vgm-tag-transfer-vgm_tt)  
[vgm_trim - VGM Trimmer](#vgm-trimmer-vgm_trim)  
[vgm_vol - VGM Volume Detector](#vgm-volume-detector-vgm_vol)  
[vgm2txt - VGM Text Writer](#vgm-text-writer-vgm2txt)  
[vgmlpfnd - VGM Loop Finder](#vgm-loop-finder-vgmlpfnd)  
[vgmmerge - VGM Merger](#vgm-merger-vgmmerge)  

### DRO to VGM Converter (dro2vgm)

This tool converts DRO files (DOSBox RAW OPL) to VGM.

These DRO-versions are supported:

| Version | DosBox | Release Date |
| ------- | ------ | ------------ |
|    0    |  0.61  |  4 Jul 2004  |
|    1    |  0.63  | 18 Nov 2004  |
|    2    |  0.73  | 14 Jul 2008  |

Note: The program may print a warning about incorrect delays. That's caused by old DRO-files (v0 and v1) because of the way DOSBox logs them. The program already HAS a detection to detect and fix some of these bugs, so most files should work.


### IMF to VGM Converter (imf2vgm)

This tool converts IMF files (id Software Music Format) to VGM.

It supports IMF type-0 and type-1 files. The type is autodetected, but you can override the auto-detection with "-Type0" or "-Type1".

Note: The actual playback speed of IMF files depends on the game they are from. The files themselves contain no information about the actual playback speed.

Currently, the playback speed defaults to 700 Hz for files that use the .WLF extension and to 560 Hz for all others.

You can override the playback speed with the "-Hz" parameter.

The resulting VGMs can be forced to loop with the "-Loop" parameter.


### OPL 2<->3 Converter (opl_23)

This handy tool allows you to convert between "single OPL2", "dual OPL2" and OPL3 VGMs.
It can help you if you accidentally logged music with the wrong "oplmode" setting in DOSBox.

The tool can convert in 3 ways and performs additional actions, depending on the mode.

* Mode `3d2`: convert OPL3 to Dual OPL2 (L/R hard-panned)
	- enforces "waveform select" enable (register 01h, data 20h)
	- warns when Dual OPL2 panning will be different from original OPL3 panning
	- warns when OPL3-specific waveforms are used
* Mode `d23`: convert Dual OPL2 to OPL3
	- enables OPL3 mode
	- enforces OPL3 panning to match Dual OPL2 panning
* Mode `d22`: convert Dual OPL2 to Single OPL2
	- checks that _**every**_ command was written to both OPL2 chips (so that it can be reduced to single OPL2) and warns if not


### VGM OKIM6258 Optimizer (opt_oki)

This tool scans streamed OKIM6258 data (from VGM logs of X68000 games) and optimizes them into "play sample" commands for the VGM "DAC Stream" system.

As of June 2022, the tool is stable and easy to use. However the VGMs need to log the X68000 DMA commands.
These commands are logged by XM6 VGM mod, 2021 release and MAME VGM mod 0.236 and later.

Note: The tool does a slightly fuzzy sample detection by default. (It ignores the last 4 bytes of each PCM sample.)
The fuzzy comparision can be disabled using the `-nofuzz` parameter.

The tool will dump all PCM samples if you pass the `-dump` parameter.

### VGM DAC Optimizer (optdac)

This tool cleans up YM2612 DAC writes.

I wrote this tool for optimizing logs of the MegaDrive version of "Worms".

The game uses a multi-channel DAC driver and constantly streams data to the YM2612's DAC channel.
So when no sample is playing, it will constantly write the byte 0x80.

This tool detects when the same value is consecutively written 128 times or more to the DAC and removes all of them but the first one.

This allows optvgm to work better on VGM logs of this game.

This is a very special case though and using the tool on games that don't constantly write to the DAC might make the optvgm optimization worse. So please use with caution!


### VGM PWM Optimizer (optvgm32)

***NOTE:** still in pre-alpha and not for public use*

This tool is supposed to optimize the Sega 32X PWM data writes into data blocks.

I never got the actual detection of "repeating data" done, so right now it just turns everything into one large stream. (with synchronization commands every second)


### VGM RF-PCM Optimizer (optvgmrf)

If you know optvgm, you know what optvgmrf does. (Internally it works completely different, but the result is the same.)

This tool optimized VGM files, that use PCM chips of the RF-family (RF5C68 and Sega MegaCD's RF5C164).

The file size of optimized files usually decreases to 5-10%.

The resulting output-file will be updated to vgm v1.60.


### RAW to VGM Converter (raw2vgm)

This tool converts RAW files (Rdos Raw OPL Capture) to VGM.

The resulting VGMs can be forced to loop with the "-Loop" parameter.
If the RAW file uses two OPL chips, the VGM will use two centered YM3812 chips.


### VGM Compressor (vgm_cmp)

This tool can greatly reduce the size of a VGM by stripping unneccessary commands. Delays are also highly optimized.

The file size of compressed files usually decreases to 50-60% (Sega Master System with YM2413) or 15-40% (NeoGeo Pocket).

Usage: `vgm_cmp [-justtmr] Input.vgm [Output.vgm]`

If you insert the argument `-justtmr`, vgm_cmp will only strip commands that can't affect the playback in any way.

Notes:

- The tool resets its internal data at the Loop Offset to make loops safer.
- Although there is a loss of data, a sample-by-sample comparison results in the exactly same output. (verified with T6W28, YM2413, YM2612, RF5C68, AY8910, YMF271, YMF262, YMZ280B, Y8950)
- NEVER use this tool on untrimmed files. Trimming will NOT work vgm_trim.
- Nevertheless it can be useful to use this tool with `-justtmr` on (a copy of) an untrimmed file.


### VGM Command Counter (vgm_cnt)

This tool counts the commands/notes for each chip.

TODO: better description


### VGM Data Block Compressor (vgm_dbc)

This tool optimizes VGM "Sample Database" blocks using lossless bit-packing, where possible.

TODO: better description


### VGM DAC Stream Optimizer (vgm_dso)

This tool optimizes VGMs that use DAC Stream command 0x93 (Play From Offset) to use command 0x95 (Play Data Block)

TODO: better description


### Make VGM Frame Accurate (vgm_facc)

This tool does exactly what is says - it rounds all delays to frames.

You can set the Frame Rate with VGMTool ("Playback rate").

All rates are accepted as long as a can be rounded to whole samples (44100 % Rate = 0).

**WARNING:** You should NOT use this tool unless you know EXACTLY what you are doing!

The only time you may want to use this tool is when:

- You are logging Sega Master System or Sega Game Gear music.
- AND you are logging with Kega Fusion. (NOT Meka or MAME, because those don't log at 60.00 Hz in sample-accurate mode)

Even then, make sure the left/right errors aren't too high, else you will introduce jitter.

After a conversion is finished, the rounding statistics are displayed. They show the maximum rounding errors (OldSample - RoundedSample) relative to the beginning of the frames. 

Line 1: Left Error [always negative or 0]

Line 2: Right Error (all but 1st frame)

Line 3: Right Error (1st frame only)

The 3rd line is included, because initialisation can take many commands and takes more time than the rest of a song.

If the value of the first and/or the second line is quite high (e.g. >= 300 for 60 Hz) a frame accurate conversion is a bad idea.

Note: Even delays of 65535 samples are rounded. (e.g. to 65415 for 60 Hz)


### VGM Mono (vgm_mono)

This tool makes VGMs mono, works with YM2610, YMF278B, YMW258/MultiPCM and X1-010.

TODO: better description


### VGM Undualizer (vgm_ndlz)

This tool splits one VGM with "2x chip" into two VGMs with "1x chip" each.

TODO: better description


### VGM Patcher (vgm_ptch)

general VGM patching utility, allows editing the VGM header (chip clocks/chip settings), checking/fixing VGMs and stripping chips/channels.

TODO: better description


### VGM Renamer (vgm_ren)

This tool renames a VGM so that its file name follows the VGM's title tag.

When a playlist is given, track numbers are added and the playlist is rewritten with the new file names.
The tool can also be used to easily renumber all files after changing the track order.


### Remove 1 Sample Delays (vgm_smp1)

This tool helps to reduce the size of VGMs by removing delays of 1 sample length.

Example:

	Delay 12 - Event A - Delay 1 - Event B - Delay 5

is rounded to:

	Delay 12 - Event A - Event B - Delay 6

You may use this tool with:

- VGMs that use the SN76489 and variants (SEGA PSG, T6W28, ...)
  - minor size reduction without audible artifacts
- VGMs converted from DRO files
  - use with `-delay:50`, might *remove* glitches that happen due to frequency writes being split by a 1/1000 Hz delay

You should **NOT** use this tool with:
- anything not mentioned above (using this tool is especially dangerous to commands for FM chips)

**Note:** If you remove samples from VGMs compressed with vgm_cmp you should run vgm_cmp again. It may reduce the file size by some bytes.


### VGM Splitter (Delay Edition, vgm_sptd)

This tool splits a vgm file into smaller pieces. This is useful if you record many songs to one file (e.g. in MAME or MESS).

It splits after a certain delay which you have to enter after the filename. It's given in samples, 1 second has 44100 samples in VGMs. (default is 32768)

The delay where a file is split is stripped, so you shouldn't have any silence at the beginning or end of the file.

Note: Sometimes it seems to produce some empty files if you use delay values >= 65536, but all others should be bug-free.


### VGM Splitter (Sample Edition, vgm_spts)

This tool splits a vgm file into smaller pieces. This is useful if you record many songs to one file (e.g. in MAME or MESS).

The difference to the tool above is, that you have to enter the sample where you want to split the file. Entering 0 (simply pressing enter without typing has the same effect) closes the program.

The delay where a file is split is stripped, so you shouldn't have any silence at the beginning or end of the file.

Notes:
- To get the last part of the vgm, you need to enter -1, as 0 will close vgm_spts instantly.
- The "current sample" value can be higher than the sample value that was entered previously. That's nothing unusual because it won't split inside a delay.
- The commandline can be used like this: "vgm_spts Stream.vgm 44100 176400 0". It will then split at sample 44100 and 176400 and will end the program. Without the zero at the end it will wait for another input.


### VGM Sample-ROM Optimizer (vgm_sro)

Like vgm_cmp, this tool can greatly reduce the size of a VGM. It strips unused data from Sample-ROMs.

The file size of optimized files usually decreases to 5-10%.

Before any data is stripped, vgm_sro will display all ROM Regions. So you can see what data is stripped.

The following chips are supported:
- C140
- C219
- C352
- ES5503
- ES5506
- Irem GA20
- K053260
- K054539
- NES APU (DPCM data)
- OKIM6295
- Q-Sound
- RF5C68/RF5C164 (if not streamed)
- SegaPCM
- UPD7759 (if not streamed)
- X1-010
- Y8950 (DELTA-T)
- YM2608 (DELTA-T)
- YM2610 (ADPCM and DELTA-T)
- YMF278B (ROM and RAM)
- YMF271
- YMZ280B
- YRW258/MultiPCM

Notes:
- If a vgm-file doesn't use one of these chips, vgm_sro can't do anything and will refuse to process the file.
- YM2610 ADPCM only: If many ROM Regions have a length of 100 or vgm_sro complains about "end out of range" (and wants to use the complete ROM), you may want to try vgm_sro_adpcm1. This is a patch that avoids a bug of "Puzzle De Pon!".
- SegaPCM support isn't 100% safe. That means, that there may be stripped off, although they're used. That can happen between 2 memory writes that relocate the  playing address and shouldn't be audible.


### VGM Statistics (vgm_stat)

This tool prints song length/loop length statistics for a folder or M3U playlist.

TODO: better description


### VGM Tagger (vgm_tag)

This tool allows VGM tagging via commandline, HTML NCRs can be used in place of Unicode characters.

TODO: better description


### VGM Tag Transfer (vgm_tt)

This tool transfers tags and/or file names of VGMs from one folder to the VGMs of another folder, based on song/loop length.

TODO: better description


### VGM Trimmer (vgm_trim)

This is a simple vgm trimmer. It's recommended to use it if you have a completely clean loop (e.g. one found with the VGM Loop Finder).

You will be asked for a file name and three sample positions.

- Start Sample: the 1st sample of the new VGM file (default: 0)
- Loop Sample: sample to which the VGM loops back (default: 0)
  - Special Values:
	- `0` - Looping off
	- `-1` - Loop from Start Sample (you must use this if Start Sample is 0)
	- `-2` - Keep old Loop Point (if there was no loop there will be no loop)

- End Sample: sample, where the VGM ends or loops back (default: 0, data of this sample is deleted if EndSample < TotalSamples)`
  - Special Values:
	- `0` - actually not a special value, but can be used to cancel trimming
	- `-1` - use Total Samples-Value

In order to make a safe but optimized trimming, the following commands are rewritten:
- PSG: NoiseMode (Reg 0xE?) / GG Stereo
- YM2612: LFO Frequency (Reg 0x22) / Ch 3 Mode (Reg 0x27) / DAC Enable (Reg 0x02B)
- YM2203: Ch 3 Mode (Reg 0x27)
- YM2608: LFO Frequency (Reg 0x22) / Ch 3 Mode (Reg 0x27) / ADPCM Volume (Reg 0x011) / Delta-T Volume (Reg 0x10B)
- YM2610: LFO Frequency (Reg 0x22) / Ch 3 Mode (Reg 0x27) / ADPCM Volume (Reg 0x101) / Delta-T Volume (Reg 0x01B)
- YM3812/YM3526/Y8950: Wave Select (Reg 0x01) / CSM/KeySplit (Reg 0x08)
- YMF262/YMF278B: Wave Select (Reg 0x01) / CSM/KeySplit (Reg 0x08) / OPL3/4 Mode Enable (Reg 0x105) / 4-Ch-Mode (Reg 0x104)
- Y8950: Delta-T Volume (Reg 0x12)
- YMZ280B: Key On Enable (Reg 0xFF)
- YMF271: Group Registers (Reg 0x600 - 0x60F)
- [Note: And some more. Still need to fix the ReadMe]

Commandline Usage: `vgm_trim [-state] Input.vgm StartSmpl LoopSmpl EndSmpl [Output.vgm]`

If you insert the argument `-state` then vgm_trim will put a save state of all used chips at the beginning of the VGM.

You can use this, if the instruments don't want to sound right even with silence at the beginning.

**Notes:**
- It's possible to add silence to a file by setting Start Sample < 0 or End Sample > TotalSamples.
- Unlike VGMTool there's NO data added to make safe loops. This makes files smaller, but you have to find the right loop point. (vgmlpfnd may like your game :) )
- There can be problems at the beginning of a vgm due to stripped commands, but usually music engines send enough commands or even spam the chips so that should be no problem.
  * If this is still a problem, please contact me and I'll implement a mode that can simulate VGMTool's way of trimming.
- `-state` is not yet supported for all chips.


### VGM Volume Detector (vgm_vol)

This tool detects the peak volume of WAV files logged from VGMs.

TODO: better description


### VGM Text Writer (vgm2txt)

This tool converts a VGM to a text file.

It's not as good as the vgm2txt of VGMTool (it doesn't print note names or frequencies), but supports VGM version 1.60 and almost all current chips.

Notes:
- unsupported are: YM2413, PWM, SCSP, ES5505/6, Irem GA20
- note names are supported for YM2151, because of the way the chip works.


### VGM Loop Finder (vgmlpfnd)

This tool can be a big help if you search for loop points.

It searches for blocks that match exactly, but have a different position in the file. Delays are ignored to make things possible.

After opening a file and entering a Step Size (higher values speed things up, lower ones are more accurate), and another 2 values there is a display like this:

```
     Source Block	      Block Copy	   Copy Information
Start	End	Smpl	Start	End	Smpl	Length	Cmds	Samples
```

It searches for matching blocks, so there's a "Source Block" and a "Block Copy". Each block has a start and end position (byte-values in hex) and also a sample start position. `Copy Information` shows the block length in bytes ("Length"), commands ("Cmds") and samples ("Samples").

If `Source Block: End` is greater than `Block Copy: Start` or at least equal, then congratulations - you have found the loop! However, don't forget to check the length of your loop or else you might cut the tune.

Then you can copy the sample values to a VGM Trimming tool of your choice - `Source Block: Smpl` as Loop Point and `Block Copy: Smpl` as End Point. (In this case vgm_trim should work well.)

Sometimes there are some letters shown before the `Source Block: Smpl` value:
- `e` is an incomplete loop, that was terminated by an EOF
- `f` is a full loop (but didn't last to the EOF)
- `!` is a full loop that's terminated by an EOF - you've found the optimal loop!

While searching, you can press "Pause" to pause the search. (Press any other key to continue.) And you can press Ctrl + C to close the program. I'm sure you will WANT to do this if your file is big.

Successfully tested with:
- Sega Master System (YM2413)
- OutRun on MAME (YM2151)
- MSX Computers (YM2413 and YMF278)
- Sega MegaDrive/Genesis (YM2612, with DAC used too)

There's absolute no way to use it with:
- NeoGeo Pocket (T6W28, the sound engine is weird)

Some music engines that use the SN76496 or T6W28 seem to shuffle the order of commands sometimes.
Music engines with dynamic channel allocation (like GEMS on MegaDrive) make it impossible to find loops.

Multi-chip-systems also may not work very well.

To speed the search and make it possible to find loops the following commands are ignored:
- timer registers in general
- YM2612 DAC write
- RF5Cxx Memory write
- PWM Channel write
- Data Block (comparing isn't needed)

**Notes:**
- Searching can take a very long time, because it searches very many times through the whole file.
- Although the display may be a little confusing at the beginning, it can greatly reduce the time you need to make a vgm pack.


### VGM Merger (vgmmerge)

This small tool merges two (or more) vgms into one.

That's it. Really. You can use it e.g. to combine 2 mid2vgm files to get YM2413 + YM2612.

Usage: `vgmmerge [-f:#] [-nodual] Input1.vgm Input2.vgm [Output.vgm]`
* `-f:#` merge # files (default is 2)
* `-nodual` disables generating dual-chip vgms when two vgm with the same chip are merged (because generating dual-chip vgms isn't yet supported, this is always on)

**Note:** You can get quite many warnings if you merge multiple vgms with the same chips, but different chip attributes (e.g. clocks).


## General Notes
- Almost all inputs can also be given through the command line.
- No tool will overwrite an original file, unless you specify an output-filename that matches the input-filename.
    * By default the resulting file has always the name "sourcefile_something.vgm", but will be overwritten without any question.
- The resulting files are always uncompressed vgm-files. (vgz files will be decompressed.)
- Using the VGM Compressor should be the last step before tagging.
- Handling of 0x8? commands should work and was tested but I'm not 100% sure it'll always work correctly.
- Most tools don't check if a file was changed during the process. A tool may output the file without changing anything.
- All tools wait for a keypress at end, if there're called with an absolute path. So they don't close instantly when double-clicked.

**IMPORTANT NOTE:** Some parts of this file are outdated. Many of the VGM v1.61 features, as well as some newer tools aren't mentioned.
