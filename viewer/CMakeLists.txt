cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

add_definitions(-DVK_ENABLE_BETA_EXTENSIONS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_program(CLANG_FORMAT_EXE NAMES "clang-format" DOC "Path to clang-format executable")

set(LUMEN_VIEWER_SHADER_SOURCES ${PROJECT_SOURCE_DIR}/viewer/shader/path_trace.rchit
                                ${PROJECT_SOURCE_DIR}/viewer/shader/path_trace.rgen
                                ${PROJECT_SOURCE_DIR}/viewer/shader/path_trace.rmiss
                                ${PROJECT_SOURCE_DIR}/viewer/shader/path_trace_debug.rchit
                                ${PROJECT_SOURCE_DIR}/viewer/shader/path_trace_debug.rgen
                                ${PROJECT_SOURCE_DIR}/viewer/shader/path_trace_debug.rmiss)

if (APPLE)
    add_executable(LumenViewer MACOSX_BUNDLE "main.cpp")
    set(MACOSX_BUNDLE_BUNDLE_NAME "com.dihara.lumen") 
else()
    add_executable(LumenViewer "main.cpp") 
endif()

target_link_libraries(LumenViewer Lumen)

foreach(GLSL ${LUMEN_VIEWER_SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_SOURCE_DIR}/bin/$(Configuration)/shader/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/$(Configuration)/shader"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(LumenViewer_Shaders DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(LumenViewer LumenViewer_Shaders)

if (WIN32)
    set_property(TARGET LumenViewer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$(Configuration)")
endif()