cmake_minimum_required(VERSION 3.23)
project(PowerStreamingHost LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "PowerStreamingHost requires Windows (DXGI Desktop Duplication).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(AMF_ROOT "" CACHE PATH "AMF SDK root, containing amf/public/include")
if(NOT AMF_ROOT)
  message(FATAL_ERROR "Set AMF_ROOT to the extracted AMD AMF SDK directory.")
endif()
if(NOT EXISTS "${AMF_ROOT}/amf/public/include")
  message(FATAL_ERROR "AMF_ROOT must contain amf/public/include")
endif()

add_executable(PowerStreamingHost WIN32
  src/main.cpp
  src/desktop_capture.cpp
  src/desktop_capture.h
  src/amf_encoder.cpp
  src/amf_encoder.h
  src/rtp_h264.cpp
  src/rtp_h264.h
)

target_include_directories(PowerStreamingHost PRIVATE
  "${AMF_ROOT}/amf/public/include"
  src
)

target_compile_definitions(PowerStreamingHost PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(PowerStreamingHost PRIVATE d3d11 dxgi dxguid ws2_32)

if(MSVC)
  target_compile_options(PowerStreamingHost PRIVATE /W4 /permissive-)
endif()
