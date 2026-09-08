cmake_minimum_required(VERSION 3.20)
project(d3dref9自治 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Source files contain UTF-8 Chinese labels.  MSVC otherwise decodes them as
# the active CP936 code page and corrupts string literals during compilation.
if (MSVC)
  add_compile_options(/utf-8)
endif()
set(D3DREF9_SOURCES src/dllmain.cpp src/feature_manager.cpp src/game_handlers.cpp src/entity_adapter.cpp src/render_adapter.cpp src/input_router.cpp src/macro_engine.cpp src/home_panel.cpp src/instant_sniper.cpp src/aim_controller.cpp)
add_library(d3dref9_autonomy SHARED ${D3DREF9_SOURCES})
target_include_directories(d3dref9_autonomy PRIVATE include)
target_compile_definitions(d3dref9_autonomy PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(d3dref9_autonomy PRIVATE user32 gdi32)
set_target_properties(d3dref9_autonomy PROPERTIES OUTPUT_NAME "d3dref9自治" PREFIX "" SUFFIX ".dll")

# Direct-replacement build.  It intentionally keeps the regular自治 target
# separate, while exporting only the reference ordinal-1 compatibility entry.
add_library(d3dref9_direct SHARED ${D3DREF9_SOURCES} src/ordinal_entry.cpp D3DREF9.def)
target_include_directories(d3dref9_direct PRIVATE include)
target_compile_definitions(d3dref9_direct PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN D3DREF9_DIRECT)
target_link_libraries(d3dref9_direct PRIVATE user32 gdi32)
set_target_properties(d3dref9_direct PROPERTIES OUTPUT_NAME "D3DREF9" PREFIX "" SUFFIX ".DLL")
