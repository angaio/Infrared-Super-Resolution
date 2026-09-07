# Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
# wechat:angaio/13707128443

#-------------------------------------------------------------------------------
# SrDemoApp - the user interface.
#
# All algorithm work lives in SrCoreLib. This target sees only the public header
# SrInterface.h and links the SrCore target.
#-------------------------------------------------------------------------------
set(TARGET SrDemo)

# Brand icon compiled into the .exe on Windows (taskbar / Alt-Tab / explorer).
set(APP_WIN_RC)
if(WIN32)
    set(APP_WIN_RC ${CMAKE_CURRENT_SOURCE_DIR}/res/app.rc)
endif()

add_executable(${TARGET}
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/VideoController.h
    src/VideoController.cpp
    src/SrWorker.h
    src/SrWorker.cpp
    src/ImageView.h
    src/ImageView.cpp
    src/CameraEnum.h
    src/CameraEnum.cpp
    src/VideoRecorder.h
    src/VideoRecorder.cpp
    res/app.qrc          # brand logo shown in the window / About (AUTORCC)
    ${APP_WIN_RC}
)

# Windowed app on Windows (no console).
if(WIN32)
    set_target_properties(${TARGET} PROPERTIES WIN32_EXECUTABLE ON)
endif()

target_compile_definitions(${TARGET} PRIVATE QT_NO_CAST_FROM_ASCII)

# Only the library's public include dir - never its src/.
target_include_directories(${TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/SrCoreLib/include
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${TARGET} PRIVATE
    Qt5::Core Qt5::Gui Qt5::Widgets
    SrCore
    ${OpenCV_LIBS}     # capture, colour convert, display
)
if(WIN32)
    target_link_libraries(${TARGET} PRIVATE strmiids ole32 oleaut32)  # DirectShow enum
endif()
