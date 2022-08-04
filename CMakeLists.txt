cmake_minimum_required(VERSION 2.8.12)

set (CMAKE_CXX_STANDARD 17)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(gltfViewer)

include(src/Sources.cmake)
add_executable(gltfViewer ${VIEWER_SOURCE_FILES})



if(MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gltfViewer)
  target_compile_options(gltfViewer PRIVATE /W3 /WX)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(gltfViewer PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(extern/SDL)
target_link_libraries(gltfViewer SDL2-static)

add_subdirectory(bgfx.cmake)
target_link_libraries(gltfViewer bgfx-static)

add_subdirectory(extern/meshoptimizer)
target_link_libraries(gltfViewer meshoptimizer)

target_include_directories(gltfViewer PRIVATE extern/cgltf)





