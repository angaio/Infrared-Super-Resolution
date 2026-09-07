# Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
# wechat:angaio/13707128443

#-------------------------------------------------------------------------------
# SrDemoQt - top-level build.
#
#   SrCoreLib   the algorithm, built as a shared library (SrCore.dll / .so)
#   SrDemoApp   the Qt user interface, links SrCoreLib
#
# Dependency roots are cache variables; set them on the command line or in the
# CMake GUI without editing this file, e.g.
#   cmake -DOpenCV_DIR=/path/to/opencv -DCMAKE_PREFIX_PATH=<Qt dir> ...
#-------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)

project(SrDemoQt LANGUAGES CXX)

# ---- global defaults --------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Everything lands in one bin/ so the app finds SrCore next to it.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
# The import library for the DLL goes to bin/ too, so the app can -L it.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---- dependency roots (set on the command line or via environment) ----------
# The public prebuilt build needs only Qt and OpenCV. Point OpenCV_DIR at the
# folder holding OpenCVConfig.cmake, or pass it (and the Qt dir) via
# CMAKE_PREFIX_PATH; $ENV{OpenCV_DIR} is used as a fallback if set.
if(NOT OpenCV_DIR AND NOT "$ENV{OpenCV_DIR}" STREQUAL "")
    set(OpenCV_DIR "$ENV{OpenCV_DIR}" CACHE PATH "OpenCV build dir with OpenCVConfig.cmake")
endif()

# ---- shared packages --------------------------------------------------------
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCV 4 REQUIRED)
message(STATUS "Qt      : ${Qt5Core_VERSION}")
message(STATUS "OpenCV  : ${OpenCV_VERSION}")

# Source files are UTF-8 without BOM; MSVC must be told, or it mis-reads CJK
# bytes ending in 0x5C as line continuations.
if(MSVC)
    add_compile_options(/utf-8)
endif()

#-------------------------------------------------------------------------------
# SrCore: build from source, or consume the prebuilt library.
#
# The public tree ships SrCoreLib as its header (include/SrInterface.h) plus a
# prebuilt SrCore.dll / SrCore.lib in bin/ - the algorithm source is not
# distributed. When SrCoreLib/src is present (the internal tree) SrCore is built
# from source; otherwise the prebuilt binary is imported under the same target
# name, so SrDemoApp links "SrCore" identically either way.
#-------------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/SrCoreLib/src/SrEngineImpl.cpp")
    message(STATUS "SrCore  : building from source")

    # Inference runtime root - needed only by this from-source build. Set it on
    # the command line (-DORT_DIR=...) or via the ORT_ROOT environment variable.
    if(NOT ORT_DIR)
        set(ORT_DIR "$ENV{ORT_ROOT}" CACHE PATH "Inference runtime root")
    endif()

    # The runtime ships no CMake package; describe it as an imported target.
    # Only the from-source build needs it - the prebuilt DLL already embeds it.
    add_library(onnxruntime SHARED IMPORTED GLOBAL)
    set_target_properties(onnxruntime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ORT_DIR}/include")
    if(WIN32)
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_IMPLIB "${ORT_DIR}/lib/onnxruntime.lib"
            IMPORTED_LOCATION "${ORT_DIR}/lib/onnxruntime.dll")
    else()
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION "${ORT_DIR}/lib/libonnxruntime.so")
    endif()

    add_subdirectory(SrCoreLib)
else()
    message(STATUS "SrCore  : using the prebuilt library in bin/")
    if(WIN32 AND NOT EXISTS "${CMAKE_SOURCE_DIR}/bin/SrCore.lib")
        message(FATAL_ERROR
            "Prebuilt bin/SrCore.lib not found and SrCoreLib/src is absent. "
            "Obtain the SrCore binaries (SrCore.dll + SrCore.lib) and place "
            "them in bin/, or build against the internal tree that has the "
            "algorithm source.")
    endif()
    add_library(SrCore SHARED IMPORTED GLOBAL)
    set_target_properties(SrCore PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/SrCoreLib/include")
    if(WIN32)
        set_target_properties(SrCore PROPERTIES
            IMPORTED_IMPLIB   "${CMAKE_SOURCE_DIR}/bin/SrCore.lib"
            IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/bin/SrCore.dll")
    else()
        set_target_properties(SrCore PROPERTIES
            IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/bin/libSrCore.so")
    endif()
endif()

add_subdirectory(SrDemoApp)
