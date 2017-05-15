include_directories("${CMAKE_SOURCE_DIR}/util/")

# decide which util to compile
option(UTIL "UTIL" OFF)
option(UTIL_DIFF_NORM_VEC "UTIL_DIFF_NORM_VEC" OFF)
option(UTIL_STAT_VEC "UTIL_STAT_VEC" OFF)
option(UTIL_PRECISION "UTIL_PRECISION" OFF)
option(UTIL_PRECISION "UTIL_PROCESS_LOG" OFF)

option(UTIL_GUI "UTIL_GUI" OFF)
option(UTIL_GUI_SHOW_VEC "UTIL_GUI_SHOW_VEC" OFF)
option(UTIL_GUI_SHOW_GAMMA "UTIL_GUI_SHOW_GAMMA" OFF)

if(${UTIL})
	# define shortcut to compile all utils
	getListOfVarsStartingWith("UTIL_" matchedVars)
	foreach (_var IN LISTS matchedVars)
		set(${_var} ON)
	endforeach()
endif()

if(${UTIL_GUI})
	# define shortcut to compile all utils
	getListOfVarsStartingWith("UTIL_GUI_" matchedVars)
	foreach (_var IN LISTS matchedVars)
		set(${_var} ON)
	endforeach()
endif()


# print info
print("Utils")
printinfo_onoff(" UTIL                                                  (...)                          " "${UTIL}")
printinfo_onoff("   UTIL_DIFF_NORM_VEC                                    (ConsoleArg)                 " "${UTIL_DIFF_NORM_VEC}")
printinfo_onoff("   UTIL_STAT_VEC                                         (ConsoleArg)                 " "${UTIL_STAT_VEC}")
printinfo_onoff("   UTIL_PRECISION                                        (ConsoleArg)                 " "${UTIL_PRECISION}")
printinfo_onoff("   UTIL_PROCESS_LOG                                      (ConsoleArg)                 " "${UTIL_PROCESS_LOG}")
printinfo_onoff("   UTIL_GUI                                              (ConsoleArg)                 " "${UTIL_GUI}")
printinfo_onoff("     UTIL_GUI_SHOW_VEC                                     (ConsoleArg)               " "${UTIL_GUI_SHOW_VEC}")
printinfo_onoff("     UTIL_GUI_SHOW_GAMMA                                   (ConsoleArg)               " "${UTIL_GUI_SHOW_GAMMA}")

if(${UTIL_DIFF_NORM_VEC})
	testadd_executable("util/diff_norm_vec.cpp" "diff_norm_vec")
endif()

if(${UTIL_STAT_VEC})
	testadd_executable("util/stat_vec.cpp" "stat_vec")
endif()

if(${UTIL_PRECISION})
#	if(${USE_CUDA})
#		testadd_executable("util/precision.cu" "precision")
#	else()
#		testadd_executable("util/precision.cpp" "precision")
#	endif()
endif()

if(${UTIL_PROCESS_LOG})
	testadd_executable("util/process_log.cpp" "process_log")
endif()

if(${UTIL_GUI_SHOW_VEC})
	testadd_executable("util/gui_show_vec.cpp" "gui_show_vec")
endif()

if(${UTIL_GUI_SHOW_GAMMA})
	testadd_executable("util/gui_show_gamma.cpp" "gui_show_gamma")
endif()
