project(sample_lab)

cmake_minimum_required(VERSION 3.16)

add_executable(client file-le-client.c err.c)
add_executable(server file-le-server.c err.c)

include_directories(/opt/libevent/include)
link_directories(/opt/libevent/lib)