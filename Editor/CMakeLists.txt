set(EDITOR_TARGET Editor)

add_executable (${EDITOR_TARGET})
target_sources(${EDITOR_TARGET} PRIVATE
    "Editor.cpp"

    PUBLIC FILE_SET HEADERS TYPE HEADERS FILES
    "Editor.h"
    "MeshGenerator.h"
)

target_link_libraries(${EDITOR_TARGET} PRIVATE Ruby)

install(TARGETS ${EDITOR_TARGET})