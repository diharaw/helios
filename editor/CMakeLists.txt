cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

add_definitions(-DVK_ENABLE_BETA_EXTENSIONS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_program(CLANG_FORMAT_EXE NAMES "clang-format" DOC "Path to clang-format executable")

if (APPLE)
    add_executable(HeliosEditor MACOSX_BUNDLE "main.cpp")
    set(MACOSX_BUNDLE_BUNDLE_NAME "com.dihara.helios") 
else()
    add_executable(HeliosEditor "main.cpp") 
endif()

target_link_libraries(HeliosEditor Helios)

if (WIN32)
    set_property(TARGET HeliosEditor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$(Configuration)")
endif()