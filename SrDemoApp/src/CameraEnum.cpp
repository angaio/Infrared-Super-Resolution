/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "CameraEnum.h"

#ifdef _WIN32
#include <dshow.h>
#include <windows.h>
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// Enumerate DirectShow video-input devices. Their order matches the index
// OpenCV's MSMF/DShow backend uses, so the position in the returned vector is
// the camera index for VideoController::openCamera().
QVector<CameraInfo> enumerateCameras()
{
    QVector<CameraInfo> list;

    const bool didInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    ICreateDevEnum *devEnum = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&devEnum)))) {
        IEnumMoniker *enumMon = nullptr;
        HRESULT hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                                    &enumMon, 0);
        if (hr == S_OK && enumMon) {
            IMoniker *mon = nullptr;
            int idx = 0;
            while (enumMon->Next(1, &mon, nullptr) == S_OK) {
                IPropertyBag *bag = nullptr;
                QString name;
                if (SUCCEEDED(mon->BindToStorage(nullptr, nullptr, IID_PPV_ARGS(&bag)))) {
                    VARIANT v; VariantInit(&v);
                    // Prefer the friendly name; fall back to the description.
                    if (SUCCEEDED(bag->Read(L"FriendlyName", &v, nullptr)) ||
                        SUCCEEDED(bag->Read(L"Description", &v, nullptr))) {
                        if (v.vt == VT_BSTR && v.bstrVal)
                            name = QString::fromWCharArray(v.bstrVal);
                    }
                    VariantClear(&v);
                    bag->Release();
                }
                if (name.isEmpty())
                    name = QStringLiteral("Camera %1").arg(idx);
                list.push_back({idx, name});
                ++idx;
                mon->Release();
            }
            enumMon->Release();
        }
        devEnum->Release();
    }

    if (didInit)
        CoUninitialize();
    return list;
}

#else  // ------------------------------------------------------------- Linux

#include <QDir>
#include <QFile>

// Read the friendly name from sysfs for each /dev/video* node.
QVector<CameraInfo> enumerateCameras()
{
    QVector<CameraInfo> list;
    QDir dev(QStringLiteral("/dev"));
    const QStringList nodes = dev.entryList(QStringList{QStringLiteral("video*")},
                                            QDir::System, QDir::Name);
    for (const QString &node : nodes) {
        bool ok = false;
        const int idx = node.mid(5).toInt(&ok);   // "videoN"
        if (!ok) continue;
        QString name;
        QFile f(QStringLiteral("/sys/class/video4linux/%1/name").arg(node));
        if (f.open(QIODevice::ReadOnly))
            name = QString::fromUtf8(f.readAll()).trimmed();
        if (name.isEmpty())
            name = QStringLiteral("Camera %1").arg(idx);
        list.push_back({idx, name});
    }
    return list;
}

#endif
