cmake_minimum_required(VERSION 2.8.12)

set (CMAKE_CXX_STANDARD 17)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(gltfViewer)


# ---- bgfx ----
set(BGFX_SOURCE_FILES
  ${PROJECT_SOURCE_DIR}/extern/bimg/src/image.cpp
  ${PROJECT_SOURCE_DIR}/extern/bimg/src/image_gnf.cpp


  ${PROJECT_SOURCE_DIR}/extern/bx/src/amalgamated.cpp
  ${PROJECT_SOURCE_DIR}/extern/bgfx/src/amalgamated.cpp
)
# ---- bgfx ----

include(src/Sources.cmake)
add_executable(gltfViewer ${VIEWER_SOURCE_FILES} ${BGFX_SOURCE_FILES})

if(MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT gltfViewer)
  target_compile_options(gltfViewer PRIVATE /W3 /WX)
  # to correctly set __cplusplus macro
  target_compile_options(gltfViewer PUBLIC "/Zc:__cplusplus")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(gltfViewer PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

add_subdirectory(extern/SDL)
target_link_libraries(gltfViewer SDL2-static)




# ---- bgfx ----
add_subdirectory("${PROJECT_SOURCE_DIR}/extern/bimg/3rdparty/astc-codec/src/base/")
add_subdirectory("${PROJECT_SOURCE_DIR}/extern/bimg/3rdparty/astc-codec/src/decoder/")
target_link_libraries(gltfViewer astc-codec)

target_include_directories(gltfViewer PRIVATE
  "${PROJECT_SOURCE_DIR}/extern/bimg/3rdparty/astc-codec/include/"
  "${PROJECT_SOURCE_DIR}/extern/bimg/include/"
)

add_definitions(
  -D__STDC_LIMIT_MACROS=1
  -D__STDC_FORMAT_MACROS=1
  -D__STDC_CONSTANT_MACROS=1
)

target_compile_definitions(gltfViewer PRIVATE
  "$<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>"
)
target_include_directories(gltfViewer PRIVATE
  "${PROJECT_SOURCE_DIR}/extern/bx/3rdparty/"
  "${PROJECT_SOURCE_DIR}/extern/bx/include/"
  "${PROJECT_SOURCE_DIR}/extern/bx/include/compat/msvc/"
)

target_include_directories(gltfViewer PRIVATE
  "${PROJECT_SOURCE_DIR}/extern/bgfx/3rdparty/"
  "${PROJECT_SOURCE_DIR}/extern/bgfx/3rdparty/dxsdk/include"
  "${PROJECT_SOURCE_DIR}/extern/bgfx/3rdparty/khronos"
  "${PROJECT_SOURCE_DIR}/extern/bgfx/include/"
  "${PROJECT_SOURCE_DIR}/extern/bgfx/src/"
)
# ---- bgfx ----