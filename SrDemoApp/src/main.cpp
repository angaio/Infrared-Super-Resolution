/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : main.cpp
* Description : Entry point. Registers cv::Mat for queued signals, parses a few
*               convenience command-line options, shows the window.
******************************************************************************/
#include <QApplication>
#include <QCommandLineParser>
#include <QMetaType>
#include <QIcon>
#include <QTimer>
#include <opencv2/core.hpp>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Infrared SR Demo"));
    app.setApplicationVersion(QStringLiteral("1.0"));
    app.setOrganizationName(QStringLiteral("Wuhan Wavefront Technology Co., Ltd."));
    app.setWindowIcon(QIcon(QStringLiteral(":/brand/app_icon.png")));

    qRegisterMetaType<cv::Mat>("cv::Mat");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Real-time infrared super-resolution demo (input width 256)."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption optFile(QStringList{QStringLiteral("f"), QStringLiteral("file")},
        QStringLiteral("Open a video file on start."), QStringLiteral("path"));
    QCommandLineOption optCam(QStringList{QStringLiteral("c"), QStringLiteral("camera")},
        QStringLiteral("Open a camera on start."), QStringLiteral("index"));
    QCommandLineOption optModel(QStringList{QStringLiteral("m"), QStringLiteral("model")},
        QStringLiteral("Select a model by its display name."), QStringLiteral("name"));
    // No short flag for --view: -v already belongs to addVersionOption().
    QCommandLineOption optView(QStringList{QStringLiteral("view")},
        QStringLiteral("before | after | compare"), QStringLiteral("mode"),
        QStringLiteral("compare"));
    QCommandLineOption optAbout(QStringList{QStringLiteral("about")},
        QStringLiteral("Show the About dialog on start."));
    parser.addOption(optFile);
    parser.addOption(optCam);
    parser.addOption(optModel);
    parser.addOption(optView);
    parser.addOption(optAbout);
    parser.process(app);

    MainWindow w;
    w.applyStartupOptions(parser.value(optFile),
                          parser.isSet(optCam) ? parser.value(optCam).toInt() : -1,
                          parser.value(optModel),
                          parser.value(optView));
    w.show();
    if (parser.isSet(optAbout))
        QTimer::singleShot(0, &w, &MainWindow::showAbout);
    return app.exec();
}
