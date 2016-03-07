# set variables (options) for whole cmake
option(USE_MKL "USE_MKL" OFF)

if(${USE_MKL})
	message(STATUS "${Blue}loading MKL library${ColourReset}")

	# define variables for mkl include directories
	if(NOT DEFINED ENV{MKLROOT})
		message(FATAL_ERROR "${Red}MKLROOT has to be specified!${ColourReset}")
		return()
	endif()
	set(MKL_INCLUDE_DIR $ENV{MKLROOT}/include)
	include_directories(${MKL_INCLUDE_DIR})
	link_directories($ENV{MKLROOT}/lib/intel64)

endif()



# define print info (will be called in printsetting.cmake)
macro(PRINTSETTING_MKL)
	if(${USE_MKL})
		printinfo_green("USE_MKL\t\t\t\t" "YES")
		printinfo(" - MKL_INCLUDE\t\t\t" "${MKL_INCLUDE_DIR}")
	else()
		printinfo_red("USE_MKL\t\t\t" "NO")
	endif()

endmacro()
