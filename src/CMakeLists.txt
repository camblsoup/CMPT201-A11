set(SERVER_SOURCES queue.c servertools.c server.c)

set(CLIENT_SOURCES queue.c clienttools.c client.c)

add_executable(server ${SERVER_SOURCES})
add_executable(client ${CLIENT_SOURCES})

target_include_directories(server PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(client PRIVATE ${PROJECT_SOURCE_DIR}/include)
