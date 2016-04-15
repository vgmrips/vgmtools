########################
#
# VGM Tools Makefile
#
########################

# TODO:
#	- link dro2vgm/imf2vgm/raw2vgm/vgm_vol without -lz
#	- link -lm only to vgm_cnt, vgm_ptch, vgm_vol, vgm2txt

CC = gcc
CFLAGS := -O2 -g0 -Wall
CFLAGS += -Wno-maybe-uninitialized -Wno-unused-but-set-variable -Wno-unused-result
LDFLAGS := -lm -lz

SRC = .
OBJ = obj

TOOLS = \
	dro2vgm \
	imf2vgm \
	optvgmrf \
	raw2vgm \
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

# extra tools not compiled by default, because they are
# either in beta state or not of use without special knowledge
EXTRA_TOOLS = \
	optdac \
	optvgm32 \
	opt_oki

all: $(OBJ) $(TOOLS)

.SECONDEXPANSION:
dro2vgm_OBJS = dro2vgm.o
imf2vgm_OBJS = imf2vgm.o
opt_oki_OBJS = opt_oki.o
optdac_OBJS = optdac.o
optvgm32_OBJS = optvgm32.o
optvgmrf_OBJS = optvgmrf.o
raw2vgm_OBJS = raw2vgm.o
vgm2txt_OBJS = vgm2txt.o chiptext.o
vgm_cmp_OBJS = vgm_cmp.o chip_cmp.o
vgm_cnt_OBJS = vgm_cnt.o
vgm_dbc_OBJS = vgm_dbc.o
vgm_facc_OBJS = vgm_facc.o
vgm_mono_OBJS = vgm_mono.o
vgm_ndlz_OBJS = vgm_ndlz.o
vgm_ptch_OBJS = vgm_ptch.o chip_strp.o
vgm_smp1_OBJS = vgm_smp1.o
vgm_sptd_OBJS = vgm_sptd.o vgm_trml.o
vgm_spts_OBJS = vgm_spts.o vgm_trml.o
vgm_sro_OBJS = vgm_sro.o chip_srom.o
vgm_stat_OBJS = vgm_stat.o
vgm_tag_OBJS = vgm_tag.o
vgm_trim_OBJS = vgm_trim.o vgm_trml.o
vgm_vol_OBJS = vgm_vol.o
vgmlpfnd_OBJS = vgmlpfnd.o
vgmmerge_OBJS = vgmmerge.o

$(TOOLS) $(EXTRA_TOOLS): $$(addprefix $(OBJ)/,$$($$@_OBJS))
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ):
	mkdir $(OBJ)

clean:
	rm -f $(TOOLS) $(EXTRA_TOOLS) $(addsuffix .exe,$(TOOLS)) $(addsuffix .exe,$(EXTRA_TOOLS)) $(OBJ)/*.o
