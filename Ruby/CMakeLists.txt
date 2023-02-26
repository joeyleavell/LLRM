set(RUBY_TARGET Ruby)

option(BUILD_RUBY_EXAMPLES "Whether to build the ruby examples" ON)

FetchContent_Declare(
    glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG        11.13.0
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8
)

FetchContent_MakeAvailable(glslang glm)

add_library (${RUBY_TARGET})
target_sources(${RUBY_TARGET} PRIVATE
    "Ruby.cpp"
    "ShaderManager.cpp"
    "Util.cpp"

    PUBLIC FILE_SET HEADERS TYPE HEADERS FILES
    "Ruby.h"
    "ShaderManager.h" 
    "Utill.h"
    "Uniforms.h"
  "Vertex.h")

target_link_libraries(${RUBY_TARGET} PRIVATE glm)
target_link_libraries(${RUBY_TARGET} PRIVATE glslang)
target_link_libraries(${RUBY_TARGET} PRIVATE SPIRV)
target_link_libraries(${RUBY_TARGET} PUBLIC ${LLRM_TARGET})

install(TARGETS ${RUBY_TARGET})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Shaders DESTINATION .)

if(BUILD_RUBY_EXAMPLES)
    add_subdirectory(Examples)
endif()