All source files compile with MS VC++ 6.0, but they should also compile with any other 32-bit compiler.
It should also work with 64-bit compilers, but this wasn't tested.

The source is best viewed with 4-space tabs.
Lines are up to 96 characters long.

Some tools need additional c-files. These are:
Tool		Additional Files
vgm_cmp		chip_cmp.c
vgm_sptd	vgm_trml.c
vgm_spts	vgm_trml.c
vgm_sro		chip_srom.c
vgm_trim	vgm_trml.c
vgm2txt		chiptext.c
