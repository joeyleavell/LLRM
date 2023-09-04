cmake_minimum_required (VERSION 3.11)
include(FetchContent)

project ("LLRM")

set (LLRM_TARGET LLRM)
set (LLRM_TEST_TARGET LLRM-test)
set (LLRM_TEST_TARGET LLRM-test)

option(LLRM_BUILD_VULKAN "Selects whether LLRM will build the Vulkan backend" ON)
option(LLRM_VULKAN_VALIDATION "Whether LLRM will enable vulkan validation layers. The Vulkan SDK is required for this." ON)
option(LLRM_VULKAN_MOLTENVK "Whether LLRM will need extra extensions for MoltenVK usage." OFF)
option(LLRM_BUILD_TEST "Whether to build the test application" ON)
option(LLRM_IMGUI "Selects whether to include the optional imgui support" ON)
option(BUILD_RUBY "Whether to build the ruby rendering engine" ON)

# TODO: make this separate project
option(BUILD_RUBY "Builds the ruby rendering engine" ON)

FetchContent_Declare(
  VulkanHeaders
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
  GIT_TAG        v1.3.237
  OVERRIDE_FIND_PACKAGE
)
FetchContent_Declare(
  vulkan
  GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
  GIT_TAG        v1.3.237
)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)

add_library (${LLRM_TARGET})

# Create optional imgui target
if(LLRM_IMGUI)
    add_library(imgui)
    target_sources(imgui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp      
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp

        PUBLIC FILE_SET HEADERS TYPE HEADERS FILES BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        imgui.h
        imgui_internal.h
     )

    if(LLRM_BUILD_VULKAN)
        target_sources(imgui PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_vulkan.cpp
        )
        target_link_libraries(imgui PRIVATE Vulkan::Headers)
        target_link_libraries(imgui PRIVATE Vulkan::Vulkan)
    endif()
    target_link_libraries(imgui PUBLIC glfw)

    target_compile_definitions(${LLRM_TARGET} PUBLIC LLRM_BUILD_IMGUI)
    target_link_libraries(${LLRM_TARGET} PUBLIC imgui)
endif()

FetchContent_MakeAvailable(VulkanHeaders vulkan glfw)


target_compile_features(${LLRM_TARGET} PUBLIC cxx_std_20)
target_link_libraries(${LLRM_TARGET} PUBLIC glfw)

if(LLRM_BUILD_VULKAN)
    target_compile_definitions(${LLRM_TARGET} PUBLIC LLRM_VULKAN)
    if(LLRM_VULKAN_VALIDATION)
        target_compile_definitions(${LLRM_TARGET} PRIVATE LLRM_VULKAN_VALIDATION)
    endif()
    if(LLRM_VULKAN_MOLTENVK)
        target_compile_definitions(${LLRM_TARGET} PRIVATE LLRM_VULKAN_MOLTENVK)
    endif()

    target_link_libraries(${LLRM_TARGET} PRIVATE Vulkan::Headers)
    target_link_libraries(${LLRM_TARGET} PRIVATE Vulkan::Vulkan)
endif()

if(LLRM_BUILD_TEST)
    FetchContent_Declare(
      glslang
      GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
      GIT_TAG        11.13.0
    )
    FetchContent_MakeAvailable(glslang)

    add_executable (${LLRM_TEST_TARGET} "test.cpp" "shadercompile.h" "shadercompile.cpp")
    target_link_libraries(${LLRM_TEST_TARGET} PRIVATE glslang)
    target_link_libraries(${LLRM_TEST_TARGET} PRIVATE SPIRV)
    target_link_libraries(${LLRM_TEST_TARGET} PRIVATE ${LLRM_TARGET})
        
    install(TARGETS ${LLRM_TEST_TARGET})
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Shaders DESTINATION .)
endif()

if(BUILD_RUBY)
    add_subdirectory(Ruby)
    add_subdirectory(Editor)
endif()

target_sources(${LLRM_TARGET} PRIVATE
    "llrm_vulkan.cpp"
    "ImGuiSupport.cpp"

    PUBLIC FILE_SET HEADERS TYPE HEADERS FILES
    "llrm.h"
    "ImGuiSupport.h" 
)