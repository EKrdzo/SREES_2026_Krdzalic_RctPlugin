set(MPPLUGIN_NAME mpconv4)				#Naziv DLL-a koji ce se generisati

file(GLOB MPPLUGIN_CPP_SOURCES  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
file(GLOB MPPLUGIN_CPP_INCS     ${CMAKE_CURRENT_LIST_DIR}/src/*.h)
file(GLOB MPPLUGIN_INC_GUI      ${NATID_SDK_INC}/gui/*.h)
file(GLOB MPPLUGIN_INC_TD       ${NATID_SDK_INC}/td/*.h)
file(GLOB MPPLUGIN_INC_CNT      ${NATID_SDK_INC}/cnt/*.h)
file(GLOB MPPLUGIN_INC_MU       ${NATID_SDK_INC}/mu/*.h)
file(GLOB MPPLUGIN_INC_MEM      ${NATID_SDK_INC}/mem/*.h)
file(GLOB MPPLUGIN_INC_FO       ${NATID_SDK_INC}/fo/*.h)
file(GLOB MPPLUGIN_INC_SC       ${NATID_SDK_INC}/sc/*.h)
file(GLOB MPPLUGIN_INC_SYST     ${NATID_SDK_INC}/syst/*.h)

add_library(${MPPLUGIN_NAME} SHARED
	${MPPLUGIN_CPP_SOURCES}
	${MPPLUGIN_CPP_INCS}
	${MPPLUGIN_INC_GUI}
	${MPPLUGIN_INC_TD}
	${MPPLUGIN_INC_CNT}
	${MPPLUGIN_INC_MU}
	${MPPLUGIN_INC_MEM}
	${MPPLUGIN_INC_FO}
	${MPPLUGIN_INC_SC}
	${MPPLUGIN_INC_SYST}
)

source_group("inc\\inc"   FILES ${MPPLUGIN_CPP_INCS})
source_group("inc\\gui"   FILES ${MPPLUGIN_INC_GUI})
source_group("inc\\td"    FILES ${MPPLUGIN_INC_TD})
source_group("inc\\cnt"   FILES ${MPPLUGIN_INC_CNT})
source_group("inc\\mu"    FILES ${MPPLUGIN_INC_MU})
source_group("inc\\mem"   FILES ${MPPLUGIN_INC_MEM})
source_group("inc\\fo"    FILES ${MPPLUGIN_INC_FO})
source_group("inc\\sc"    FILES ${MPPLUGIN_INC_SC})
source_group("inc\\syst"  FILES ${MPPLUGIN_INC_SYST})
source_group("src\\cpp"   FILES ${MPPLUGIN_CPP_SOURCES})

target_link_libraries(${MPPLUGIN_NAME}
	debug ${MU_LIB_DEBUG}       optimized ${MU_LIB_RELEASE}
	debug ${MATRIX_LIB_DEBUG}   optimized ${MATRIX_LIB_RELEASE}
	debug ${NATGUI_LIB_DEBUG}   optimized ${NATGUI_LIB_RELEASE}
)

target_compile_definitions(${MPPLUGIN_NAME} PUBLIC PLUGIN_EXPORTS)
