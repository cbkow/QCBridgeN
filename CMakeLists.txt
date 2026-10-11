cmake_minimum_required(VERSION 3.24)
project(QCBridgeN VERSION 0.1.0 LANGUAGES CXX)

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
