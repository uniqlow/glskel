cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("glskel")

set(GLAD_PROFILE "core" CACHE STRING "" FORCE)
set(GLAD_API "gl=4.5" CACHE STRING "" FORCE)
set(GLAD_GENERATOR "c" CACHE STRING "" FORCE)
set(GLAD_EXTENSIONS "" CACHE STRING "" FORCE)
set(GLAD_SPEC "gl" CACHE STRING "" FORCE)
add_subdirectory(3pp/glad)
include_directories(${GLAD_INCLUDE_DIRS})

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(3pp/glfw)
include_directories(3pp/glfw/include)

add_executable(glskel src/glskel.cpp)
target_link_libraries(
	glskel
	glfw ${GLFW_LIBRARIES}
	glad ${GLAD_LIBRARIES}
)
