cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project("Helios")

IF(APPLE)
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++17")
	set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
ENDIF()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_subdirectory(external/AssetCore)
add_subdirectory(external/glfw)  

# External
set_target_properties(glfw PROPERTIES FOLDER external)

find_package(Vulkan REQUIRED)

set(HELIOS_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")
set(GLFW_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/glfw/include")
set(GLM_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/AssetCore/external/glm/glm")
set(IMGUI_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/imgui")
set(VMA_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/VulkanMemoryAllocator/src")
set(NATIVEFILEDIALOG_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/nativefiledialog/src/include")

include_directories("${HELIOS_INCLUDE_DIRS}"
					"${ASSET_CORE_INCLUDES}"
                    "${GLFW_INCLUDE_DIRS}"
                    "${GLM_INCLUDE_DIRS}"
                    "${IMGUI_INCLUDE_DIRS}"
					"${VMA_INCLUDE_DIRS}"
					"${Vulkan_INCLUDE_DIR}"
					"${NATIVEFILEDIALOG_INCLUDE_DIRS}")

add_subdirectory(src/engine)
add_subdirectory(src/viewer)
add_subdirectory(src/editor)