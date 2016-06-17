include_directories("${CMAKE_SOURCE_DIR}/test_other/")

# decide which example to compile
option(TEST_PROJECTION "TEST_PROJECTION" OFF)
option(TEST_MAT_BLOCKGRAPH_FREE_TO_DENSE "TEST_MAT_BLOCKGRAPH_FREE_TO_DENSE" OFF)
option(TEST_MAT_BLOCKLAPLACE_FREE_TO_DENSE "TEST_MAT_BLOCKLAPLACE_FREE_TO_DENSE" OFF)

# print info
printinfo_onoff("TEST_PROJECTION\t\t\t" "${TEST_PROJECTION}")
printinfo_onoff("TEST_MAT_BLOCKGRAPH_FREE_TO_DENSE\t" "${TEST_MAT_BLOCKGRAPH_FREE_TO_DENSE}")
printinfo_onoff("TEST_MAT_BLOCKLAPLACE_FREE_TO_DENSE\t" "${TEST_MAT_BLOCKLAPLACE_FREE_TO_DENSE}")

if(${TEST_PROJECTION})
	# this is projection test
	if(${USE_CUDA})
		pascadd_executable("test_other/test_projection.cu" "test_projection")
	else()
		pascadd_executable("test_other/test_projection.cpp" "test_projection")
	endif()
endif()

if(${TEST_MAT_BLOCKGRAPH_FREE_TO_DENSE})
	# this is a test of graphmatrix multiplication
	if(${USE_CUDA})
		pascadd_executable("test_other/test_mat_blockgraph_free_to_dense.cu" "test_mat_blockgraph_free_to_dense")
	else()
		pascadd_executable("test_other/test_mat_blockgraph_free_to_dense.cpp" "test_mat_blockgraph_free_to_dense")
	endif()
endif()

if(${TEST_MAT_BLOCKLAPLACE_FREE_TO_DENSE})
	# this is a test of laplacematrix multiplication
	if(${USE_CUDA})
		pascadd_executable("test_other/test_mat_blocklaplace_free_to_dense.cu" "test_mat_blocklaplace_free_to_dense")
	else()
		pascadd_executable("test_other/test_mat_blocklaplace_free_to_dense.cpp" "test_mat_blocklaplace_free_to_dense")
	endif()
endif()

