add_executable(01-Mesh "Mesh.cpp" "MeshGenerator.h")
target_link_libraries(01-Mesh PRIVATE ${RUBY_TARGET})

install(TARGETS 01-Mesh)