cmake_minimum_required(VERSION 3.24)
project(QCBridgeN VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# qcbae_wire — the vendored wire contract (MIT, vendor/qcbae/LICENSE): the
# shared mapping, the slot protocol, the output ring and the audio ring.
# Re-copied from QCBridgeAE src/common/, never edited here.
# ---------------------------------------------------------------------------
add_library(qcbae_wire STATIC
    vendor/qcbae/ring_mapping.cpp
    vendor/qcbae/output_ring.cpp
    vendor/qcbae/audio_ring.cpp
)
target_include_directories(qcbae_wire PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
if(MSVC)
    target_compile_options(qcbae_wire PRIVATE /W4)
else()
    target_compile_options(qcbae_wire PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# qcvo-probe — both ends of the bus, for measuring either side alone.
# ---------------------------------------------------------------------------
add_executable(qcvo-probe tools/qcvo-probe/main.cpp)
target_link_libraries(qcvo-probe PRIVATE qcbae_wire)
if(NOT MSVC)
    target_compile_options(qcvo-probe PRIVATE -Wall -Wextra)
endif()

# The NDI sink itself (P3) arrives behind an option once the SDK is located
# outside the tree; nothing here references it yet.

# ---------------------------------------------------------------------------
# The NDI® sink. Built only when the NDI SDK is installed: its headers are
# confidential and never enter this repository; the library is loaded at
# run time (src/sink/ndi_lib.cpp). Point NDI_SDK_DIR at the SDK if it is
# not in the default place.
# ---------------------------------------------------------------------------
set(NDI_SDK_DIR "" CACHE PATH "NDI SDK root (contains include/Processing.NDI.Lib.h)")
find_path(NDI_INCLUDE_DIR Processing.NDI.Lib.h
    HINTS "${NDI_SDK_DIR}/include" "$ENV{NDI_SDK_DIR}/include"
          "/Library/NDI SDK for Apple/include"
          "$ENV{PROGRAMFILES}/NDI/NDI 6 SDK/Include"
          "C:/Program Files/NDI/NDI 6 SDK/Include")
if(NDI_INCLUDE_DIR)
    message(STATUS "NDI SDK headers: ${NDI_INCLUDE_DIR}")
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
    qt_standard_project_setup()

    add_library(qcbridgen_ndi STATIC src/sink/ndi_lib.cpp src/sink/ndi_lib.h)
    target_include_directories(qcbridgen_ndi PUBLIC ${NDI_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/sink)
    if(NOT WIN32)
        target_link_libraries(qcbridgen_ndi PUBLIC ${CMAKE_DL_LIBS})
    endif()

    qt_add_executable(QCBridgeN
        src/sink/main.cpp
        src/sink/bus_sink.cpp
        src/sink/bus_sink.h
    )
    target_compile_definitions(QCBridgeN PRIVATE QCBRIDGEN_VERSION="${PROJECT_VERSION}")
    target_link_libraries(QCBridgeN PRIVATE qcbridgen_ndi qcbae_wire Qt6::Core Qt6::Gui Qt6::Widgets)
    if(NOT MSVC)
        target_compile_options(QCBridgeN PRIVATE -Wall -Wextra)
    endif()

    add_executable(ndi-recv tools/ndi-recv/main.cpp)
    target_link_libraries(ndi-recv PRIVATE qcbridgen_ndi)
else()
    message(STATUS "NDI SDK not found — building the probe only (set NDI_SDK_DIR to build the sink)")
endif()
