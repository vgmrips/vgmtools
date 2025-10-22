#############################
#
# VGM Tools CMake build file
#
#############################

cmake_minimum_required(VERSION 3.6)	# actual minimum is 3.3
project (VGMTools LANGUAGES C CXX)

set(CMAKE_C_STANDARD 90)
set(CMAKE_CXX_STANDARD 98)

find_package(ZLIB REQUIRED)

if(MSVC)
    if(NOT MSVC_VERSION LESS 1400)
        add_definitions("-D _CRT_SECURE_NO_WARNINGS")
    endif()
endif()


add_executable(dacopt
    dacopt.c
)
install(TARGETS dacopt RUNTIME DESTINATION "bin")


add_executable(dro2vgm
    dro2vgm.c
)
install(TARGETS dro2vgm RUNTIME DESTINATION "bin")


add_executable(imf2vgm
    imf2vgm.c
)
install(TARGETS imf2vgm RUNTIME DESTINATION "bin")


add_executable(opl_23
    opl_23.c
)
target_link_libraries(opl_23 ZLIB::ZLIB)
install(TARGETS opl_23 RUNTIME DESTINATION "bin")


add_executable(opt_oki
    opt_oki.c
)
target_link_libraries(opt_oki ZLIB::ZLIB)
install(TARGETS opt_oki RUNTIME DESTINATION "bin")


add_executable(optvgm
    optvgm.cpp
    pcm_optimizer.cpp
)
target_link_libraries(optvgm ZLIB::ZLIB)
install(TARGETS optvgm RUNTIME DESTINATION "bin")


add_executable(optvgmrf
    optvgmrf.c
)
target_link_libraries(optvgmrf ZLIB::ZLIB)
install(TARGETS optvgmrf RUNTIME DESTINATION "bin")


add_executable(raw2vgm
    raw2vgm.c
)
install(TARGETS raw2vgm RUNTIME DESTINATION "bin")


add_executable(vgm2txt
    vgm2txt.c
    chiptext.c
)
target_link_libraries(vgm2txt ZLIB::ZLIB)
if(NOT MSVC)
    target_link_libraries(vgm2txt -lm)
endif()
install(TARGETS vgm2txt RUNTIME DESTINATION "bin")


add_executable(vgm_cmp
    vgm_cmp.c
    chip_cmp.c
)
target_link_libraries(vgm_cmp ZLIB::ZLIB)
install(TARGETS vgm_cmp RUNTIME DESTINATION "bin")


add_executable(vgm_cnt
    vgm_cnt.c
)
target_link_libraries(vgm_cnt ZLIB::ZLIB)
if(NOT MSVC)
    target_link_libraries(vgm_cnt -lm)
endif()
install(TARGETS vgm_cnt RUNTIME DESTINATION "bin")


add_executable(vgm_dbc
    vgm_dbc.c
)
target_link_libraries(vgm_dbc ZLIB::ZLIB)
install(TARGETS vgm_dbc RUNTIME DESTINATION "bin")


add_executable(vgm_dso
    vgm_dso.c
)
target_link_libraries(vgm_dso ZLIB::ZLIB)
install(TARGETS vgm_dso RUNTIME DESTINATION "bin")


add_executable(vgm_facc
    vgm_facc.c
)
target_link_libraries(vgm_facc ZLIB::ZLIB)
install(TARGETS vgm_facc RUNTIME DESTINATION "bin")


add_executable(vgm_mono
    vgm_mono.c
)
target_link_libraries(vgm_mono ZLIB::ZLIB)
install(TARGETS vgm_mono RUNTIME DESTINATION "bin")


add_executable(vgm_ndlz
    vgm_ndlz.c
)
target_link_libraries(vgm_ndlz ZLIB::ZLIB)
install(TARGETS vgm_ndlz RUNTIME DESTINATION "bin")


add_executable(vgm_ptch
    vgm_ptch.c
    chip_strp.c
)
target_link_libraries(vgm_ptch ZLIB::ZLIB)
if(NOT MSVC)
    target_link_libraries(vgm_ptch -lm)
endif()
install(TARGETS vgm_ptch RUNTIME DESTINATION "bin")


add_executable(vgm_ren
    vgm_ren.c
)
target_link_libraries(vgm_ren ZLIB::ZLIB)
install(TARGETS vgm_ren RUNTIME DESTINATION "bin")


add_executable(vgm_smp1
    vgm_smp1.c
)
target_link_libraries(vgm_smp1 ZLIB::ZLIB)
install(TARGETS vgm_smp1 RUNTIME DESTINATION "bin")


add_executable(vgm_sptd
    vgm_sptd.c
    vgm_trml.c
)
target_link_libraries(vgm_sptd ZLIB::ZLIB)
install(TARGETS vgm_sptd RUNTIME DESTINATION "bin")


add_executable(vgm_spts
    vgm_spts.c
    vgm_trml.c
)
target_link_libraries(vgm_spts ZLIB::ZLIB)
install(TARGETS vgm_spts RUNTIME DESTINATION "bin")


add_executable(vgm_sro
    vgm_sro.c
    chip_srom.c
)
target_link_libraries(vgm_sro ZLIB::ZLIB)
install(TARGETS vgm_sro RUNTIME DESTINATION "bin")


add_executable(vgm_stat
    vgm_stat.c
)
target_link_libraries(vgm_stat ZLIB::ZLIB)
install(TARGETS vgm_stat RUNTIME DESTINATION "bin")


add_executable(vgm_tag
    vgm_tag.c
)
target_link_libraries(vgm_tag ZLIB::ZLIB)
install(TARGETS vgm_tag RUNTIME DESTINATION "bin")


add_executable(vgm_trim
    vgm_trim.c
    vgm_trml.c
)
target_link_libraries(vgm_trim ZLIB::ZLIB)
install(TARGETS vgm_trim RUNTIME DESTINATION "bin")


add_executable(vgm_tt
    vgm_tt.cpp
)
target_link_libraries(vgm_tt ZLIB::ZLIB)
install(TARGETS vgm_tt RUNTIME DESTINATION "bin")


add_executable(vgm_vol
    vgm_vol.c
)
if(NOT MSVC)
    target_link_libraries(vgm_vol -lm)
endif()
install(TARGETS vgm_vol RUNTIME DESTINATION "bin")


add_executable(vgmlpfnd
    vgmlpfnd.c
)
target_link_libraries(vgmlpfnd ZLIB::ZLIB)
install(TARGETS vgmlpfnd RUNTIME DESTINATION "bin")


add_executable(vgmmerge
    vgmmerge.c
)
target_link_libraries(vgmmerge ZLIB::ZLIB)
install(TARGETS vgmmerge RUNTIME DESTINATION "bin")


# extra tools not compiled by default, because they are
# either in beta state or not of use/shouldn't be used without special knowledge
add_executable(optdac EXCLUDE_FROM_ALL
    optdac.c
)
target_link_libraries(optdac ZLIB::ZLIB)
install(TARGETS optdac RUNTIME DESTINATION "bin" OPTIONAL)


add_executable(optvgm32 EXCLUDE_FROM_ALL
    optvgm32.c
)
target_link_libraries(optvgm32 ZLIB::ZLIB)
install(TARGETS optvgm32 RUNTIME DESTINATION "bin" OPTIONAL)

