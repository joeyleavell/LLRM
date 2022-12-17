cmake_minimum_required (VERSION 3.11)
include(FetchContent)

project ("LLRM")

set (LLRM_TARGET LLRM)
set (LLRM_TEST_TARGET LLRM-test)

option(LLRM_BUILD_VULKAN "Selects whether LLRM will build the Vulkan backend" ON)
option(LLRM_VULKAN_VALIDATION "Whether LLRM will enable vulkan validation layers. The Vulkan SDK is required for this." ON)
option(LLRM_VULKAN_MOLTENVK "Whether LLRM will need extra extensions for MoltenVK usage." OFF)
option(LLRM_BUILD_TEST "Whether to build the test application" ON)

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

target_sources(${LLRM_TARGET} PRIVATE
    "llrm_vulkan.cpp"

    PUBLIC FILE_SET HEADERS TYPE HEADERS FILES
    "llrm.h"
)