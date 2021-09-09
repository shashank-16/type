cmake_minimum_required(VERSION 3.16.3)

project(type VERSION 0.1)

add_executable(${PROJECT_NAME} type.cpp)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)