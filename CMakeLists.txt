cmake_minimum_required(VERSION 3.2)

### Hunter snapshot that will be used ###
include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/ruslo/hunter/archive/v0.23.83.tar.gz"
    SHA1 "12dec078717539eb7b03e6d2a17797cba9be9ba9"
)

project(s_gen)

hunter_add_package(stb)
find_package(stb CONFIG REQUIRED)

hunter_add_package(glm)
find_package(glm REQUIRED)

hunter_add_package(glfw)
find_package(glfw3 CONFIG REQUIRED)

hunter_add_package(Assimp)
find_package(Assimp CONFIG REQUIRED)

hunter_add_package(glew)
find_package(glew CONFIG REQUIRED)
 
hunter_add_package(cxxopts)
find_package(cxxopts CONFIG REQUIRED)

file(GLOB SOURCES "*.h" "*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} Assimp::assimp glm glfw stb::stb glew::glew cxxopts::cxxopts)