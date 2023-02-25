add_executable(01-Mesh "Mesh.cpp" "MeshGenerator.cpp")
target_link_libraries(01-Mesh PRIVATE ${RUBY_TARGET})
target_link_libraries(01-Mesh PRIVATE glm::glm)

install(TARGETS 01-Mesh)