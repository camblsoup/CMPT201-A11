cmake_minimum_required(VERSION 3.14)
project(assignment11)

set(SANITIZE_FLAGS "-fsanitize=thread" "-g" "-O1")

add_compile_options(${SANITIZE_FLAGS})
add_link_options(${SANITIZE_FLAGS})

add_subdirectory(include)
add_subdirectory(src)

set_target_properties(server PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                        "${CMAKE_BINARY_DIR}")
set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                        "${CMAKE_BINARY_DIR}")
