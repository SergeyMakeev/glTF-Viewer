

set(BGFX_ROOT_DIR ${PROJECT_SOURCE_DIR}/extern/)


set(BGFX_SOURCE_FILES
  ${BGFX_ROOT_DIR}/bimg/src/image.cpp
  ${BGFX_ROOT_DIR}/bimg/src/image_gnf.cpp

  ${BGFX_ROOT_DIR}/bx/src/amalgamated.cpp
  ${BGFX_ROOT_DIR}/bgfx/src/amalgamated.cpp
)

add_library(bgfx-static STATIC ${BGFX_SOURCE_FILES})

target_compile_features(bgfx-static PRIVATE cxx_std_17)

# to correctly set __cplusplus macro
target_compile_options(bgfx-static PUBLIC "/Zc:__cplusplus")


add_subdirectory(${BGFX_ROOT_DIR}/bimg/3rdparty/astc-codec/src/base/ astc-codec-base)
add_subdirectory(${BGFX_ROOT_DIR}/bimg/3rdparty/astc-codec/src/decoder/ astc-codec-decoder)
target_link_libraries(bgfx-static astc-codec)

target_include_directories(bgfx-static PRIVATE
  ${BGFX_ROOT_DIR}/bimg/3rdparty/astc-codec/include/
  ${BGFX_ROOT_DIR}/bimg/include/
)

add_definitions(
  -D__STDC_LIMIT_MACROS=1
  -D__STDC_FORMAT_MACROS=1
  -D__STDC_CONSTANT_MACROS=1
)

target_compile_definitions(bgfx-static PRIVATE
  "$<$<CONFIG:Debug>:BX_CONFIG_DEBUG=1>"
)

target_include_directories(bgfx-static PRIVATE
  ${BGFX_ROOT_DIR}/bx/3rdparty/
)

target_include_directories(bgfx-static PUBLIC
  ${BGFX_ROOT_DIR}/bx/include/
  ${BGFX_ROOT_DIR}/bx/include/compat/msvc/
)

target_include_directories(bgfx-static PRIVATE
  ${BGFX_ROOT_DIR}/bgfx/3rdparty/
  ${BGFX_ROOT_DIR}/bgfx/3rdparty/dxsdk/include
  ${BGFX_ROOT_DIR}/bgfx/3rdparty/khronos
  ${BGFX_ROOT_DIR}/bgfx/src/
)

target_include_directories(bgfx-static PUBLIC
  ${BGFX_ROOT_DIR}/bgfx/include/
)
