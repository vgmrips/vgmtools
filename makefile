########################
#
# VGM Tools Makefile
# (for GNU Make 3.81)
#
########################

CC = gcc

SRC = .
OBJ = obj

TOOLS = \
	dro2vgm \
	optdac \
	optvgm32 \
	optvgmrf \
	vgm2txt \
	vgm_cmp \
	vgm_cnt \
	vgm_dbc \
	vgm_facc \
	vgm_mono \
	vgm_ndlz \
	vgm_ptch \
	vgm_smp1 \
	vgm_sptd \
	vgm_spts \
	vgm_sro \
	vgm_stat \
	vgm_tag \
	vgm_trim \
	vgm_vol \
	vgmlpfnd \
	vgmmerge

all:	$(TOOLS)

DRO2VGM_OBJS = \
	$(OBJ)/dro2vgm.o

dro2vgm:	$(DRO2VGM_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

OPTDAC_OBJS = \
	$(OBJ)/optdac.o

optdac:		$(OPTDAC_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

OPTVGM32_OBJS = \
	$(OBJ)/optvgm32.o

optvgm32:	$(OPTVGM32_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

OPTVGMRF_OBJS = \
	$(OBJ)/optvgmrf.o

optvgmrf:	$(OPTVGMRF_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM2TXT_OBJS = \
	$(OBJ)/vgm2txt.o \
	$(OBJ)/chiptext.o

vgm2txt:	$(VGM2TXT_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_CMP_OBJS = \
	$(OBJ)/vgm_cmp.o \
	$(OBJ)/chip_cmp.o

vgm_cmp:	$(VGM_CMP_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_CNT_OBJS = \
	$(OBJ)/vgm_cnt.o

vgm_cnt:	$(VGM_CNT_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_DBC_OBJS = \
	$(OBJ)/vgm_dbc.o

vgm_dbc:	$(VGM_DBC_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_FACC_OBJS = \
	$(OBJ)/vgm_facc.o

vgm_facc:	$(VGM_FACC_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_MONO_OBJS = \
	$(OBJ)/vgm_mono.o

vgm_mono:	$(VGM_MONO_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_NDLZ_OBJS = \
	$(OBJ)/vgm_ndlz.o

vgm_ndlz:	$(VGM_NDLZ_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_PTCH_OBJS = \
	$(OBJ)/vgm_ptch.o \
	$(OBJ)/chip_strp.o

vgm_ptch:	$(VGM_PTCH_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_SMP1_OBJS = \
	$(OBJ)/vgm_smp1.o

vgm_smp1:	$(VGM_SMP1_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_SPTD_OBJS = \
	$(OBJ)/vgm_sptd.o \
	$(OBJ)/vgm_trml.o

vgm_sptd:	$(VGM_SPTD_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_SPTS_OBJS = \
	$(OBJ)/vgm_spts.o \
	$(OBJ)/vgm_trml.o

vgm_spts:	$(VGM_SPTS_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_SRO_OBJS = \
	$(OBJ)/vgm_sro.o \
	$(OBJ)/chip_srom.o

vgm_sro:	$(VGM_SRO_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_STAT_OBJS = \
	$(OBJ)/vgm_stat.o

vgm_stat:
	@echo . > $(VGM_STAT_OBJS)
#vgm_stat:	$(VGM_STAT_OBJS)
#	$(CC) $^ -lm -Wl,-lz -o $@

VGM_TAG_OBJS = \
	$(OBJ)/vgm_tag.o

vgm_tag:	$(VGM_TAG_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_TRIM_OBJS = \
	$(OBJ)/vgm_trim.o \
	$(OBJ)/vgm_trml.o

vgm_trim:	$(VGM_TRIM_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGM_VOL_OBJS = \
	$(OBJ)/vgm_vol.o

vgm_vol:	$(VGM_VOL_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGMLPFND_OBJS = \
	$(OBJ)/vgmlpfnd.o

vgmlpfnd:	$(VGMLPFND_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

VGMMERGE_OBJS = \
	$(OBJ)/vgmmerge.o

vgmmerge:	$(VGMMERGE_OBJS)
	$(CC) $^ -lm -Wl,-lz -o $@

$(OBJ)/%.o:	$(SRC)/%.c
	$(CC) $(CCFLAGS) $(MAINFLAGS) -c $< -o $@

$(OBJ):
	mkdir $(OBJ)

clean:
	rm -f $(TOOLS) $(DRO2VGM_OBJS) $(OPTDAC_OBJS) $(OPTVGM32_OBJS) $(OPTVGMRF_OBJS) $(VGM2TXT_OBJS) \
		$(VGM_CMP_OBJS) $(VGM_CNT_OBJS) $(VGM_DBC_OBJS) $(VGM_FACC_OBJS) $(VGM_MONO_OBJS) \
		$(VGM_NDLZ_OBJS) $(VGM_PTCH_OBJS) $(VGM_SMP1_OBJS) $(VGM_SPTD_OBJS) $(VGM_SPTS_OBJS) \
		$(VGM_SRO_OBJS) $(VGM_STAT_OBJS) $(VGM_TAG_OBJS) $(VGM_TRIM_OBJS) $(VGM_VOL_OBJS) \
		$(VGMLPFND_OBJS) $(VGMMERGE_OBJS)
