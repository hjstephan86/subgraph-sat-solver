cmake_minimum_required(VERSION 3.16)
project(SubgraphSATSolver CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-spezifische Compiler-Flags
if(MSVC)
    # Visual Studio / MSVC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2")
else()
    # GCC / Clang / MinGW
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra -pedantic")
endif()

# =================================================================
# Dependencies
# =================================================================

find_package(nlohmann_json 3.2.0 QUIET)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found - downloading...")
    include(FetchContent)
    FetchContent_Declare(json 
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

# =================================================================
# Source Files
# =================================================================

set(SAT_SOLVER_SOURCES
    Cnf.cpp
    Reduction.cpp
    SubgraphSATSolver.cpp
)

set(SAT_SOLVER_HEADERS
    Cnf.h
    Reduction.h
    SubgraphSATSolver.h
)

# Include directory
set(SAT_SOLVER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# =================================================================
# Main Executable: SubgraphSATSolver
# =================================================================

add_executable(SubgraphSATSolver
    ${SAT_SOLVER_SOURCES}
    main.cpp
)

target_include_directories(SubgraphSATSolver PRIVATE
    ${SAT_SOLVER_INCLUDE_DIR}
)

target_link_libraries(SubgraphSATSolver PRIVATE nlohmann_json::nlohmann_json)

# =================================================================
# Library: satlib (für externe Nutzung)
# =================================================================

add_library(satlib STATIC
    ${SAT_SOLVER_SOURCES}
)

target_include_directories(satlib PUBLIC
    ${SAT_SOLVER_INCLUDE_DIR}
)

target_link_libraries(satlib PUBLIC nlohmann_json::nlohmann_json)

# =================================================================
# Testing
# =================================================================

enable_testing()

# Test executable
add_executable(sat-tests
    ${SAT_SOLVER_SOURCES}
    tests/TestCnf.cpp
)

target_include_directories(sat-tests PRIVATE
    ${SAT_SOLVER_INCLUDE_DIR}
)

target_link_libraries(sat-tests PRIVATE nlohmann_json::nlohmann_json)

add_test(NAME SAT-Solver-Tests COMMAND sat-tests)

# =================================================================
# Installation
# =================================================================

install(TARGETS SubgraphSATSolver satlib
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${SAT_SOLVER_HEADERS}
    DESTINATION include/sat
)

# =================================================================
# Configuration Output
# =================================================================

message(STATUS "")
message(STATUS "SubgraphSATSolver Configuration")
message(STATUS "==================================")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
if(MSVC)
    message(STATUS "C++ Flags: ${CMAKE_CXX_FLAGS} /W4 /O2")
else()
    message(STATUS "C++ Flags: ${CMAKE_CXX_FLAGS}")
endif()
message(STATUS "Installation Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Targets:")
message(STATUS "  - SubgraphSATSolver (executable)")
message(STATUS "  - satlib (static library)")
message(STATUS "  - sat-tests (test executable)")
message(STATUS "")
message(STATUS "To build:")
message(STATUS "  cmake --build . -j4")
message(STATUS "")
