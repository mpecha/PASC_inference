# print settings

message("\n----------------------------------------------------------\n")

# Compiler
printinfo("Cmake" "")
printinfo(" - build type\t\t\t" "${CMAKE_BUILD_TYPE}")
printinfo(" - CMAKE_LIBRARY_OUTPUT_DIRECTORY" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
printinfo(" - COMPILE_FIRST\t\t" "${COMPILE_FIRST}")

printinfo("Compiler" "")
printinfo(" - C compiler\t\t\t" "${CMAKE_C_COMPILER}")
printinfo(" - C flags\t\t\t" "${CMAKE_C_FLAGS}")
printinfo(" - C++ compiler\t\t" "${CMAKE_CXX_COMPILER}")
printinfo(" - C++ flags\t\t\t" "${CMAKE_CXX_FLAGS}")
printinfo(" - linker\t\t\t" "${CMAKE_LINKER}")
printinfo(" - linker flags\t\t" "${CMAKE_EXE_LINKER_FLAGS}")
printinfo(" - make\t\t\t" "${CMAKE_MAKE_PROGRAM}")
printinfo(" - shared linker flags\t\t" "${CMAKE_SHARED_LINKER_FLAGS}")

printinfo("Flags" "")
printinfo(" - FLAGS_DEF\t\t\t" "${FLAGS_DEF}")
printinfo(" - FLAGS_DEF_D\t\t\t" "${FLAGS_DEF_D}")
printinfo(" - LIBRARIES_DEF\t\t" "${LIBRARIES_DEF}")
printinfo(" - LIBRARY_PATH\t\t" "$ENV{LIBRARY_PATH}")

# cuda
printsetting_cuda()

# boost
printsetting_boost()

# Petsc
printsetting_petsc()

# PERMON
printsetting_permon()

# MKL
printsetting_mkl()

# MinLin
printsetting_minlin()

# Metis
printsetting_metis()

# Craypower
printsetting_craypower()

# Dlib
printsetting_dlib()

# PASCInference
printsetting_pascinference()


message("\n----------------------------------------------------------\n")





