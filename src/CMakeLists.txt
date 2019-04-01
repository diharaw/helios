cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_program(CLANG_FORMAT_EXE NAMES "clang-format" DOC "Path to clang-format executable")

file(GLOB_RECURSE LUMEN_HEADERS ${PROJECT_SOURCE_DIR}/src/*.h)

file(GLOB_RECURSE LUMEN_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

if (APPLE)
    add_executable(Lumen MACOSX_BUNDLE ${LUMEN_HEADERS} ${LUMEN_SOURCES})
    set(MACOSX_BUNDLE_BUNDLE_NAME "com.dihara.lumen") 
else()
    add_executable(Lumen ${LUMEN_HEADERS} ${LUMEN_SOURCES}) 
endif()

target_link_libraries(Lumen AssetCore)

if (WIN32)
    set_property(TARGET Lumen PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$(Configuration)")
endif()

if(CLANG_FORMAT_EXE)
    add_custom_target(clang-format-project-files COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${LUMEN_HEADERS} ${LUMEN_SOURCES})
endif()