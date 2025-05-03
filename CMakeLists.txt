cmake_minimum_required(VERSION 3.10)
project(terminal_emulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-DNCURSES_WIDECHAR)

# Find ncurses
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)
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
    src/main.cpp
    src/ansi_logic.cpp
    src/curses_interface.cpp
)
target_include_directories(terminal_emulator PRIVATE
    ${gtest_SOURCE_DIR}/include
)
target_link_libraries(terminal_emulator ${CURSES_LIBRARIES})

# Test executable
add_executable(unit_tests
    src/unit_tests.cpp
    src/ansi_logic.cpp
)

target_link_libraries(unit_tests ${CURSES_LIBRARIES} GTest::gtest_main)

# Add tests to CTest
include(GoogleTest)
gtest_discover_tests(unit_tests)

# Installation
install(TARGETS terminal_emulator DESTINATION bin)
