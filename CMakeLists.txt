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
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)
    qt_standard_project_setup()

    # svg2png: the icon PNGs in src/icons are generated from the SVG marks
    # with this (alpha intact); re-run it when the marks change:
    #   build/svg2png src/icons/qcviewMAC.svg src/icons/QCBridgeN-mac-1024.png 1024
    #   build/svg2png src/icons/qcviewMAC.svg src/icons/tray-mac-live.png 36 --fill "#000000"
    #   build/svg2png src/icons/qcviewMAC.svg src/icons/tray-mac-idle.png 36 --fill "#000000" --opacity 0.42
    #   build/svg2png src/icons/qcviewWIN.svg src/icons/tray-win-64.png 64
    qt_add_executable(svg2png tools/svg2png/main.cpp)
    target_link_libraries(svg2png PRIVATE Qt6::Core Qt6::Gui Qt6::Svg)

    add_library(qcbridgen_ndi STATIC src/sink/ndi_lib.cpp src/sink/ndi_lib.h)
    target_include_directories(qcbridgen_ndi PUBLIC ${NDI_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/sink)
    if(NOT WIN32)
        target_link_libraries(qcbridgen_ndi PUBLIC ${CMAKE_DL_LIBS})
    endif()

    qt_add_executable(QCBridgeN
        src/sink/main.cpp
        src/sink/bus_sink.cpp
        src/sink/bus_sink.h
        src/sink/settings_dialog.cpp
        src/sink/settings_dialog.h
        src/icons/icons.qrc
    )
    target_compile_definitions(QCBridgeN PRIVATE QCBRIDGEN_VERSION="${PROJECT_VERSION}")
    target_link_libraries(QCBridgeN PRIVATE qcbridgen_ndi qcbae_wire Qt6::Core Qt6::Gui Qt6::Widgets)
    if(NOT MSVC)
        target_compile_options(QCBridgeN PRIVATE -Wall -Wextra)
    endif()

    if(APPLE)
        # The app bundle: a menu-bar accessory (LSUIElement in the plist),
        # the icns from the master PNG, and the NDI runtime inside
        # Contents/Frameworks (the SDK allows the runtime in the application
        # folder; ndi_lib.cpp looks there last, after the installed runtime).
        # Its licence text goes to Resources: codesign refuses non-code
        # files under Frameworks.
        set(QCBN_ICON_PNG    "${CMAKE_SOURCE_DIR}/src/icons/QCBridgeN-mac-1024.png")
        set(QCBN_ICONSET_DIR "${CMAKE_BINARY_DIR}/QCBridgeN.iconset")
        set(QCBN_ICNS_FILE   "${CMAKE_BINARY_DIR}/QCBridgeN.icns")
        add_custom_command(
            OUTPUT  "${QCBN_ICNS_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${QCBN_ICONSET_DIR}"
            COMMAND sips -z 16   16   "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_16x16.png"
            COMMAND sips -z 32   32   "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_16x16@2x.png"
            COMMAND sips -z 32   32   "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_32x32.png"
            COMMAND sips -z 64   64   "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_32x32@2x.png"
            COMMAND sips -z 128  128  "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_128x128.png"
            COMMAND sips -z 256  256  "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_128x128@2x.png"
            COMMAND sips -z 256  256  "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_256x256.png"
            COMMAND sips -z 512  512  "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_256x256@2x.png"
            COMMAND sips -z 512  512  "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_512x512.png"
            COMMAND sips -z 1024 1024 "${QCBN_ICON_PNG}" --out "${QCBN_ICONSET_DIR}/icon_512x512@2x.png"
            COMMAND iconutil -c icns "${QCBN_ICONSET_DIR}" -o "${QCBN_ICNS_FILE}"
            DEPENDS "${QCBN_ICON_PNG}"
            COMMENT "Generating QCBridgeN.icns"
            VERBATIM
        )
        set_source_files_properties("${QCBN_ICNS_FILE}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(QCBridgeN PRIVATE "${QCBN_ICNS_FILE}")

        set(MACOSX_BUNDLE_EXECUTABLE_NAME "QCBridgeN")
        set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
        set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
        configure_file("${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in" "${CMAKE_BINARY_DIR}/Info.plist")
        set_target_properties(QCBridgeN PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
            OUTPUT_NAME "QCBridgeN")

        get_filename_component(NDI_SDK_ROOT "${NDI_INCLUDE_DIR}" DIRECTORY)
        set(NDI_DYLIB "${NDI_SDK_ROOT}/lib/macOS/libndi.dylib")
        set(NDI_LICENSES "${NDI_SDK_ROOT}/lib/macOS/libndi_licenses.txt")
        if(EXISTS "${NDI_DYLIB}")
            add_custom_command(TARGET QCBridgeN POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:QCBridgeN>/../Frameworks"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NDI_DYLIB}" "$<TARGET_FILE_DIR:QCBridgeN>/../Frameworks/libndi.dylib"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NDI_LICENSES}" "$<TARGET_FILE_DIR:QCBridgeN>/../Resources/libndi_licenses.txt"
                COMMENT "Embedding the NDI runtime into QCBridgeN.app/Contents/Frameworks"
                VERBATIM)
        else()
            message(WARNING "NDI runtime dylib not found at ${NDI_DYLIB}; the bundle will rely on an installed runtime")
        endif()
    endif()

    add_executable(ndi-recv tools/ndi-recv/main.cpp)
    target_link_libraries(ndi-recv PRIVATE qcbridgen_ndi)
else()
    message(STATUS "NDI SDK not found — building the probe only (set NDI_SDK_DIR to build the sink)")
endif()
