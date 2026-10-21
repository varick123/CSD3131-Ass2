cmake_minimum_required(VERSION 3.20)
project(SeamCarvingStarter LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(seam_core
    src/energy.cpp
    src/dp.cpp
    src/greedy.cpp
    src/graph_cut.cpp
    src/resize.cpp
    src/image_io.cpp
)
target_include_directories(seam_core PUBLIC include)
add_executable(seamcarve src/main.cpp)
target_link_libraries(seamcarve PRIVATE seam_core)
foreach(target seam_core seamcarve)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

include(CTest)
if(BUILD_TESTING)
    # This only verifies that the starter executable launches. Add real
    # algorithm tests as modules are implemented; a smoke pass is not correctness.
    add_test(NAME starter_help COMMAND seamcarve --help)
    add_test(NAME starter_status COMMAND seamcarve --status)
endif()
