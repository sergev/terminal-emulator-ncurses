cmake_minimum_required(VERSION 3.10)
project(terminal_emulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ncurses
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

# Use FetchContent to download Googletest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    EXCLUDE_FROM_ALL
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()

# Main executable
add_executable(terminal_emulator
    main.cpp
    terminal_logic.cpp
    system_interface.cpp
)

target_link_libraries(terminal_emulator ${CURSES_LIBRARIES})

# Test executable
add_executable(unit_tests
    unit_tests.cpp
    terminal_logic.cpp
)

target_link_libraries(unit_tests ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} ${CURSES_LIBRARIES})

enable_testing()
add_test(NAME TerminalLogicTest COMMAND unit_tests)
