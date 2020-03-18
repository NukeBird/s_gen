cmake_minimum_required(VERSION 3.2)

project(s_gen)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

file(GLOB PROJECT_SOURCE_FILES "*.h" "*.cpp")

add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} ${CONAN_LIBS})