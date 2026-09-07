/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : CameraEnum.h
* Description : Enumerate connected video-capture devices with their names.
*
* OpenCV's VideoCapture is index-only, so the friendly name comes from the OS:
* DirectShow on Windows, /sys or /dev metadata on Linux. The vector index is
* the same index VideoCapture::open() expects.
******************************************************************************/
#ifndef ___CameraEnum_h___
#define ___CameraEnum_h___

#include <QString>
#include <QVector>

struct CameraInfo
{
    int     index;   // pass to VideoController::openCamera()
    QString name;    // human-readable device name
};

// Best-effort enumeration. If names cannot be obtained, falls back to a plain
// "Camera <index>" list so the UI still works.
QVector<CameraInfo> enumerateCameras();

#endif
