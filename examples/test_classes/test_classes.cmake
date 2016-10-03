include_directories("${CMAKE_SOURCE_DIR}/test_classes/")

# decide which test to compile
option(TEST_CONSOLEARG "TEST_CONSOLEARG" OFF)
option(TEST_CONSOLEOUTPUT "TEST_CONSOLEOUTPUT" OFF)
option(TEST_GLOBALMANAGER "TEST_GLOBALMANAGER" OFF)
option(TEST_LOGGINGCLASS "TEST_LOGGINGCLASS" OFF)
option(TEST_MEMORYCHECK "TEST_MEMORYCHECK" OFF)
option(TEST_OFFSETCLASS "TEST_OFFSETCLASS" OFF)
option(TEST_SHORTINFOCLASS "TEST_SHORTINFOCLASS" OFF)
option(TEST_STACKTIMER "TEST_STACKTIMER" OFF)
option(TEST_TIMER "TEST_TIMER" OFF)

# print info
print("\nClasses tests")
print(" common")
printinfo_onoff("  TEST_CONSOLEARG         (ConsoleArg)         " "${TEST_CONSOLEARG}")
printinfo_onoff("  TEST_CONSOLEOUTPUT      (ConsoleOutput)      " "${TEST_CONSOLEOUTPUT}")
printinfo_onoff("  TEST_GLOBALMANAGER      (GlobalManager)      " "${TEST_GLOBALMANAGER}")
printinfo_onoff("  TEST_LOGGINGCLASS       (LoggingClass)       " "${TEST_LOGGINGCLASS}")
printinfo_onoff("  TEST_MEMORYCHECK        (MemoryCheck)        " "${TEST_MEMORYCHECK}")
printinfo_onoff("  TEST_OFFSETCLASS        (OffsetClass)        " "${TEST_OFFSETCLASS}")
printinfo_onoff("  TEST_SHORTINFOCLASS     (ShortinfoClass)     " "${TEST_SHORTINFOCLASS}")
printinfo_onoff("  TEST_STACKTIMER         (StackTimer)         " "${TEST_STACKTIMER}")
printinfo_onoff("  TEST_TIMER              (Timer)              " "${TEST_TIMER}")


if(${TEST_CONSOLEARG})
	# ConsoleArgClass
	pascadd_executable("test_classes/test_consolearg.cpp" "test_consolearg")
endif()

if(${TEST_CONSOLEOUTPUT})
	# ConsoleOutput
	pascadd_executable("test_classes/test_consoleoutput.cpp" "test_consoleoutput")
endif()

if(${TEST_GLOBALMANAGER})
	# GlobalManager
	pascadd_executable("test_classes/test_globalmanager.cpp" "test_globalmanager")
endif()



