cmake_minimum_required(VERSION 3.12)

project(visca-tcp2udp-bridge VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS FALSE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

find_package(Boost REQUIRED COMPONENTS log)

set(main_sources
	src/main.cc
	src/socket-moderator.cc
	src/upstream-udp.cc
	src/upstream-udp-socket.cc
	src/downstream.cc
	src/downstream-tcp.cc
	src/downstream-tcp-server.cc
	src/properties.cc
	src/log.cc
	src/os_gettime.c
)

add_executable(${PROJECT_NAME} ${main_sources})

target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
