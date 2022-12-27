#############################
#
# VGM Tools CMake build file
#
#############################

cmake_minimum_required (VERSION 3.2)
project (VGMTools LANGUAGES C CXX)

find_package(ZLIB REQUIRED)


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
target_include_directories(opl_23 PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(opl_23 ${ZLIB_LIBRARIES})
install(TARGETS opl_23 RUNTIME DESTINATION "bin")


add_executable(opt_oki
    opt_oki.c
)
target_include_directories(opt_oki PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(opt_oki ${ZLIB_LIBRARIES})
install(TARGETS opt_oki RUNTIME DESTINATION "bin")


add_executable(optvgm
    optvgm.cpp
    pcm_optimizer.cpp
)
target_include_directories(optvgm PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(optvgm ${ZLIB_LIBRARIES})
install(TARGETS optvgm RUNTIME DESTINATION "bin")


add_executable(optvgmrf
    optvgmrf.c
)
target_include_directories(optvgmrf PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(optvgmrf ${ZLIB_LIBRARIES})
install(TARGETS optvgmrf RUNTIME DESTINATION "bin")


add_executable(raw2vgm
    raw2vgm.c
)
install(TARGETS raw2vgm RUNTIME DESTINATION "bin")


add_executable(vgm2txt
    vgm2txt.c
    chiptext.c
)
target_include_directories(vgm2txt PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm2txt ${ZLIB_LIBRARIES})
if(NOT MSVC)
    target_link_libraries(vgm2txt -lm)
endif()
install(TARGETS vgm2txt RUNTIME DESTINATION "bin")


add_executable(vgm_cmp
    vgm_cmp.c
    chip_cmp.c
)
target_include_directories(vgm_cmp PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_cmp ${ZLIB_LIBRARIES})
install(TARGETS vgm_cmp RUNTIME DESTINATION "bin")


add_executable(vgm_cnt
    vgm_cnt.c
)
target_include_directories(vgm_cnt PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_cnt ${ZLIB_LIBRARIES})
if(NOT MSVC)
    target_link_libraries(vgm_cnt -lm)
endif()
install(TARGETS vgm_cnt RUNTIME DESTINATION "bin")


add_executable(vgm_dbc
    vgm_dbc.c
)
target_include_directories(vgm_dbc PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_dbc ${ZLIB_LIBRARIES})
install(TARGETS vgm_dbc RUNTIME DESTINATION "bin")


add_executable(vgm_dso
    vgm_dso.c
)
target_include_directories(vgm_dso PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_dso ${ZLIB_LIBRARIES})
install(TARGETS vgm_dso RUNTIME DESTINATION "bin")


add_executable(vgm_facc
    vgm_facc.c
)
target_include_directories(vgm_facc PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_facc ${ZLIB_LIBRARIES})
install(TARGETS vgm_facc RUNTIME DESTINATION "bin")


add_executable(vgm_mono
    vgm_mono.c
)
target_include_directories(vgm_mono PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_mono ${ZLIB_LIBRARIES})
install(TARGETS vgm_mono RUNTIME DESTINATION "bin")


add_executable(vgm_ndlz
    vgm_ndlz.c
)
target_include_directories(vgm_ndlz PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_ndlz ${ZLIB_LIBRARIES})
install(TARGETS vgm_ndlz RUNTIME DESTINATION "bin")


add_executable(vgm_ptch
    vgm_ptch.c
    chip_strp.c
)
target_include_directories(vgm_ptch PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_ptch ${ZLIB_LIBRARIES})
if(NOT MSVC)
    target_link_libraries(vgm_ptch -lm)
endif()
install(TARGETS vgm_ptch RUNTIME DESTINATION "bin")


add_executable(vgm_ren
    vgm_ren.c
)
target_include_directories(vgm_ren PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_ren ${ZLIB_LIBRARIES})
install(TARGETS vgm_ren RUNTIME DESTINATION "bin")


add_executable(vgm_smp1
    vgm_smp1.c
)
target_include_directories(vgm_smp1 PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_smp1 ${ZLIB_LIBRARIES})
install(TARGETS vgm_smp1 RUNTIME DESTINATION "bin")


add_executable(vgm_sptd
    vgm_sptd.c
    vgm_trml.c
)
target_include_directories(vgm_sptd PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_sptd ${ZLIB_LIBRARIES})
install(TARGETS vgm_sptd RUNTIME DESTINATION "bin")


add_executable(vgm_spts
    vgm_spts.c
    vgm_trml.c
)
target_include_directories(vgm_spts PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_spts ${ZLIB_LIBRARIES})
install(TARGETS vgm_spts RUNTIME DESTINATION "bin")


add_executable(vgm_sro
    vgm_sro.c
    chip_srom.c
)
target_include_directories(vgm_sro PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_sro ${ZLIB_LIBRARIES})
install(TARGETS vgm_sro RUNTIME DESTINATION "bin")


add_executable(vgm_stat
    vgm_stat.c
)
target_include_directories(vgm_stat PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_stat ${ZLIB_LIBRARIES})
install(TARGETS vgm_stat RUNTIME DESTINATION "bin")


add_executable(vgm_tag
    vgm_tag.c
)
target_include_directories(vgm_tag PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_tag ${ZLIB_LIBRARIES})
install(TARGETS vgm_tag RUNTIME DESTINATION "bin")


add_executable(vgm_trim
    vgm_trim.c
    vgm_trml.c
)
target_include_directories(vgm_trim PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_trim ${ZLIB_LIBRARIES})
install(TARGETS vgm_trim RUNTIME DESTINATION "bin")


add_executable(vgm_tt
    vgm_tt.cpp
)
target_include_directories(vgm_tt PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgm_tt ${ZLIB_LIBRARIES})
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
target_include_directories(vgmlpfnd PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgmlpfnd ${ZLIB_LIBRARIES})
install(TARGETS vgmlpfnd RUNTIME DESTINATION "bin")


add_executable(vgmmerge
    vgmmerge.c
)
target_include_directories(vgmmerge PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(vgmmerge ${ZLIB_LIBRARIES})
install(TARGETS vgmmerge RUNTIME DESTINATION "bin")


# extra tools not compiled by default, because they are
# either in beta state or not of use/shouldn't be used without special knowledge
add_executable(optdac EXCLUDE_FROM_ALL
    optdac.c
)
target_include_directories(optdac PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(optdac ${ZLIB_LIBRARIES})
install(TARGETS optdac RUNTIME DESTINATION "bin" OPTIONAL)


add_executable(optvgm32 EXCLUDE_FROM_ALL
    optvgm32.c
)
target_include_directories(optvgm32 PRIVATE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(optvgm32 ${ZLIB_LIBRARIES})
install(TARGETS optvgm32 RUNTIME DESTINATION "bin" OPTIONAL)

