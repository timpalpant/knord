find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)

# The parsing layer is deliberately free of QML and GUI types, so it can be
# compiled straight into the test binary and run headless in CI.
qt_add_executable(tst_nordparse
    tst_nordparse.cpp
    ../src/clirunner.cpp
    ../src/ptychannel.cpp
    ../src/nordparse.cpp
    ../src/countryflags.cpp
)

target_include_directories(tst_nordparse PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(tst_nordparse PRIVATE Qt6::Core Qt6::Test)

add_test(NAME nordparse COMMAND tst_nordparse)

# Drives a real child process over a real pty, so it needs nothing but a shell.
qt_add_executable(tst_ptychannel
    tst_ptychannel.cpp
    ../src/ptychannel.cpp
)

target_include_directories(tst_ptychannel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(tst_ptychannel PRIVATE Qt6::Core Qt6::Test)

add_test(NAME ptychannel COMMAND tst_ptychannel)
