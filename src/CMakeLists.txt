cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

find_program(CLANG_FORMAT_EXE NAMES "clang-format" DOC "Path to clang-format executable")

if (${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

add_definitions(-DVK_ENABLE_BETA_EXTENSIONS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

find_program(CLANG_FORMAT_EXE NAMES "clang-format" DOC "Path to clang-format executable")

file(GLOB_RECURSE HELIOS_HEADERS ${PROJECT_SOURCE_DIR}/include/*.h)

file(GLOB_RECURSE HELIOS_SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp)

list(APPEND HELIOS_HEADERS ${PROJECT_SOURCE_DIR}/external/imgui/imconfig.h
                          ${PROJECT_SOURCE_DIR}/external/imgui/imgui.h
                          ${PROJECT_SOURCE_DIR}/external/imgui/imgui_internal.h
                          ${PROJECT_SOURCE_DIR}/external/imgui/imstb_rectpack.h
                          ${PROJECT_SOURCE_DIR}/external/imgui/imstb_textedit.h
                          ${PROJECT_SOURCE_DIR}/external/imgui/imstb_truetype.h                    
                          ${PROJECT_SOURCE_DIR}/external/imgui/examples/imgui_impl_vulkan.h)

list(APPEND HELIOS_SOURCES ${PROJECT_SOURCE_DIR}/external/imgui/imgui.cpp
                          ${PROJECT_SOURCE_DIR}/external/imgui/imgui_demo.cpp
                          ${PROJECT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
                          ${PROJECT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
                          ${PROJECT_SOURCE_DIR}/external/imgui/examples/imgui_impl_vulkan.cpp
                          ${PROJECT_SOURCE_DIR}/external/imgui/examples/imgui_impl_glfw.cpp)

file(GLOB_RECURSE HELIOS_SHADER_SOURCES ${PROJECT_SOURCE_DIR}/src/shader/*.vert 
                                        ${PROJECT_SOURCE_DIR}/src/shader/*.frag
                                        ${PROJECT_SOURCE_DIR}/src/shader/*.rgen  
                                        ${PROJECT_SOURCE_DIR}/src/shader/*.rchit 
                                        ${PROJECT_SOURCE_DIR}/src/shader/*.rmiss
                                        ${PROJECT_SOURCE_DIR}/src/shader/*.rahit)

if (APPLE)
    add_library(Helios MACOSX_BUNDLE ${HELIOS_HEADERS} ${HELIOS_SOURCES})
    set(MACOSX_BUNDLE_BUNDLE_NAME "com.dihara.helios") 
else()
    add_library(Helios ${HELIOS_HEADERS} ${HELIOS_SOURCES}) 
endif()

target_link_libraries(Helios AssetCoreLoader)
target_link_libraries(Helios AssetCoreCommon)
target_link_libraries(Helios glfw)
target_link_libraries(Helios ${Vulkan_LIBRARY})

foreach(GLSL ${HELIOS_SHADER_SOURCES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${CMAKE_SOURCE_DIR}/bin/$(Configuration)/shader/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/bin/$(Configuration)/shader"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.2 -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(Helios_Shaders DEPENDS ${SPIRV_BINARY_FILES})

add_dependencies(Helios Helios_Shaders)

if(CLANG_FORMAT_EXE)
    add_custom_target(Helios-Code-Format COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${HELIOS_HEADERS} ${HELIOS_SOURCES})
endif()