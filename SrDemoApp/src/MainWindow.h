/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : MainWindow.h
* Description : Application shell: source control, view modes, transport bar.
*
* The window knows nothing about how upscaling works. It talks to SrWorker,
* which in turn talks to the library. Model entries are whatever the library
* reports - the UI never spells out a file name or an implementation detail.
******************************************************************************/
#ifndef ___MainWindow_h___
#define ___MainWindow_h___

#include <QMainWindow>
#include <QElapsedTimer>
#include <QStringList>
#include <opencv2/core.hpp>

#include "VideoRecorder.h"

class ImageView;
class VideoController;
class SrWorker;
class QComboBox;
class QSpinBox;
class QLabel;
class QSlider;
class QToolButton;
class QAction;
class QActionGroup;
class QMenu;
class QIcon;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    enum ViewMode { ViewBefore = 0, ViewAfter, ViewCompare };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /* Command line entry points, handy for demos and scripted checks. */
    void applyStartupOptions(const QString &file, int camera,
                             const QString &model, const QString &view);

    // Opens the About dialog (also reachable via Help > About).
    void showAbout();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onOpenCamera();
    void onOpenFile();
    void onCloseVideo();
    void onPlayPause();
    void onStop();
    void onViewModeChanged();
    void onModelChanged(int index);
    void onThreadsChanged(int value);

    void onSliderPressed();
    void onSliderMoved(int value);
    void onSliderReleased();

    void onFrame(const cv::Mat &frame);
    void onResult(const cv::Mat &before, const cv::Mat &after, double ms);
    void onModelReady(int index, const QString &summary);
    void onWorkerFailed(const QString &message);
    void onPosition(qint64 frame, qint64 total);
    void onStateChanged(int state);
    void onSourceClosed();
    void onAbout();
    void onRecordToggle();      // record the current view (Before/After/Compare)
    void onRecordSrcToggle();   // record the original 256x192 source (archive)
    void openRecentFile();

private:
    void buildUi();
    void populateCameras();
    void buildMenu();
    void applyTheme();
    void populateModels();
    void startSource(bool camera, const QString &path);
    void releaseSource();
    void updateTransport();
    static QImage toQImage(const cv::Mat &m);
    static cv::Mat stitchPair(const cv::Mat &left, const cv::Mat &right);
    static QIcon recIcon(bool stop);            // custom REC dot / Stop square
    static QIcon barIcon(const QString &kind);  // custom top-toolbar glyphs
    void updateRecordUi(bool recording);
    void updateRecordSrcUi(bool recording);
    bool anyRecording() const;
    QString recordDir() const;                  // <exe>/record, created on demand
    QString newRecordPath(const QString &prefix) const;
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();

    ImageView       *m_view   = nullptr;
    VideoController *m_source = nullptr;
    SrWorker        *m_worker = nullptr;

    QComboBox   *m_cameraBox = nullptr;
    QComboBox   *m_modelBox  = nullptr;
    QSpinBox    *m_threadBox = nullptr;
    QActionGroup*m_viewGroup = nullptr;
    QAction     *m_actClose  = nullptr;   // close the current video source

    QToolButton *m_btnPlay = nullptr;
    QToolButton *m_btnStop = nullptr;
    QToolButton *m_btnRecord = nullptr;      // records the current view
    QToolButton *m_btnRecordSrc = nullptr;   // records the original 256x192
    QSlider     *m_slider  = nullptr;
    QLabel      *m_lblNow  = nullptr;
    QLabel      *m_lblEnd  = nullptr;

    QLabel *m_stSource = nullptr;
    QLabel *m_stModel  = nullptr;
    QLabel *m_stPerf   = nullptr;

    ViewMode m_viewMode  = ViewCompare;

    VideoRecorder m_recorder;      // view recorder (what the user sees)
    VideoRecorder m_srcRecorder;   // source recorder (original 256x192, low loss)
    cv::Mat       m_lastShown;     // last composited view frame
    cv::Mat       m_lastBefore;    // last original source frame (256x192)
    bool     m_scrubbing = false;

    QMenu      *m_recentMenu = nullptr;
    QStringList m_recentFiles;

    QElapsedTimer m_fpsTimer;
    int    m_fpsCount = 0;
    double m_fps = 0.0, m_avgMs = 0.0;
};

#endif
