#############################
#
# VGM Tools CMake build file
#
#############################

cmake_minimum_required (VERSION 3.2)
project (VGMTools LANGUAGES C CXX)

find_package(ZLIB REQUIRED)


add_executable(dro2vgm
    dro2vgm.c
)


add_executable(imf2vgm
    imf2vgm.c
)


add_executable(opl_23
    opl_23.c
)
target_link_libraries(opl_23 ${ZLIB_LIBRARIES})


add_executable(optvgmrf
    optvgmrf.c
)
target_link_libraries(optvgmrf ${ZLIB_LIBRARIES})


add_executable(raw2vgm
    raw2vgm.c
)


add_executable(vgm2txt
    vgm2txt.c
    chiptext.c
)
target_link_libraries(vgm2txt ${ZLIB_LIBRARIES})
if(NOT MSVC)
    target_link_libraries(vgm2txt -lm)
endif()


add_executable(vgm_cmp
    vgm_cmp.c
    chip_cmp.c
)
target_link_libraries(vgm_cmp ${ZLIB_LIBRARIES})


add_executable(vgm_cnt
    vgm_cnt.c
)
target_link_libraries(vgm_cnt ${ZLIB_LIBRARIES})
if(NOT MSVC)
    target_link_libraries(vgm_cnt -lm)
endif()


add_executable(vgm_dbc
    vgm_dbc.c
)
target_link_libraries(vgm_dbc ${ZLIB_LIBRARIES})


add_executable(vgm_dso
    vgm_dso.c
)
target_link_libraries(vgm_dso ${ZLIB_LIBRARIES})


add_executable(vgm_facc
    vgm_facc.c
)
target_link_libraries(vgm_facc ${ZLIB_LIBRARIES})


add_executable(vgm_mono
    vgm_mono.c
)
target_link_libraries(vgm_mono ${ZLIB_LIBRARIES})


add_executable(vgm_ndlz
    vgm_ndlz.c
)
target_link_libraries(vgm_ndlz ${ZLIB_LIBRARIES})


add_executable(vgm_ptch
    vgm_ptch.c
    chip_strp.c
)
target_link_libraries(vgm_ptch ${ZLIB_LIBRARIES})
if(NOT MSVC)
    target_link_libraries(vgm_ptch -lm)
endif()


add_executable(vgm_smp1
    vgm_smp1.c
)
target_link_libraries(vgm_smp1 ${ZLIB_LIBRARIES})


add_executable(vgm_sptd
    vgm_sptd.c
    vgm_trml.c
)
target_link_libraries(vgm_sptd ${ZLIB_LIBRARIES})


add_executable(vgm_spts
    vgm_spts.c
    vgm_trml.c
)
target_link_libraries(vgm_spts ${ZLIB_LIBRARIES})


add_executable(vgm_sro
    vgm_sro.c
    chip_srom.c
)
target_link_libraries(vgm_sro ${ZLIB_LIBRARIES})


add_executable(vgm_stat
    vgm_stat.c
)
target_link_libraries(vgm_stat ${ZLIB_LIBRARIES})


add_executable(vgm_tag
    vgm_tag.c
)
target_link_libraries(vgm_tag ${ZLIB_LIBRARIES})


add_executable(vgm_trim
    vgm_trim.c
    vgm_trml.c
)
target_link_libraries(vgm_trim ${ZLIB_LIBRARIES})


add_executable(vgm_tt
    vgm_tt.cpp
)
target_link_libraries(vgm_tt ${ZLIB_LIBRARIES})


add_executable(vgm_vol
    vgm_vol.c
)
if(NOT MSVC)
    target_link_libraries(vgm_vol -lm)
endif()


add_executable(vgmlpfnd
    vgmlpfnd.c
)
target_link_libraries(vgmlpfnd ${ZLIB_LIBRARIES})


add_executable(vgmmerge
    vgmmerge.c
)
target_link_libraries(vgmmerge ${ZLIB_LIBRARIES})


# extra tools not compiled by default, because they are
# either in beta state or not of use/shouldn't be used without special knowledge
add_executable(opt_oki EXCLUDE_FROM_ALL
    opt_oki.c
)
target_link_libraries(opt_oki ${ZLIB_LIBRARIES})


add_executable(optdac EXCLUDE_FROM_ALL
    optdac.c
)
target_link_libraries(optdac ${ZLIB_LIBRARIES})


add_executable(optvgm32 EXCLUDE_FROM_ALL
    optvgm32.c
)
target_link_libraries(optvgm32 ${ZLIB_LIBRARIES})

