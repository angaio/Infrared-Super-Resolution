/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "MainWindow.h"
#include "ImageView.h"
#include "VideoController.h"
#include "SrWorker.h"

#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QSlider>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QFrame>
#include <QThread>
#include <QStyle>
#include <QMenuBar>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QIcon>
#include <QPixmap>
#include <QPainter>

#include <opencv2/imgproc.hpp>

#include "CameraEnum.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Infrared Super-Resolution Demo"));
    resize(1400, 860);

    buildUi();
    buildMenu();
    applyTheme();

    m_worker = new SrWorker(this);
    connect(m_worker, &SrWorker::resultReady, this, &MainWindow::onResult);
    connect(m_worker, &SrWorker::modelReady,  this, &MainWindow::onModelReady);
    connect(m_worker, &SrWorker::failed,      this, &MainWindow::onWorkerFailed);
    m_worker->start();

    populateModels();
    m_fpsTimer.start();
    updateTransport();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Tear everything down here, while the event loop is still alive and on the
    // main thread: stop the source, stop the worker, then destroy the engine.
    // Doing this on exit rather than during static/stack destruction avoids a
    // teardown race in the native inference runtime that can fault on close.
    releaseSource();
    if (m_worker) {
        m_worker->shutdown();
        m_worker->wait(3000);
        m_worker->releaseEngine();
    }
    QMainWindow::closeEvent(event);
}

MainWindow::~MainWindow()
{
    // Safety net for paths that bypass closeEvent(); all steps are idempotent.
    releaseSource();
    if (m_worker) {
        m_worker->shutdown();
        m_worker->wait(3000);
        m_worker->releaseEngine();
    }
}

/* ------------------------------------------------------------------ layout */

/* ------------------------------------------------------------------- menu */

void MainWindow::buildMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction *aOpenCam = fileMenu->addAction(QStringLiteral("Open &Camera"));
    connect(aOpenCam, &QAction::triggered, this, &MainWindow::onOpenCamera);
    QAction *aOpenFile = fileMenu->addAction(QStringLiteral("Open &File..."));
    connect(aOpenFile, &QAction::triggered, this, &MainWindow::onOpenFile);

    // Recently opened .mp4 clips, persisted across sessions via QSettings.
    m_recentFiles = QSettings().value(QStringLiteral("recentFiles")).toStringList();
    m_recentMenu = fileMenu->addMenu(QStringLiteral("Open &Recent"));
    rebuildRecentMenu();

    fileMenu->addSeparator();
    QAction *aExit = fileMenu->addAction(QStringLiteral("E&xit"));
    connect(aExit, &QAction::triggered, this, &QWidget::close);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    QAction *aAbout = helpMenu->addAction(QStringLiteral("&About"));
    connect(aAbout, &QAction::triggered, this, &MainWindow::onAbout);
}

void MainWindow::onAbout() { showAbout(); }

void MainWindow::showAbout()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("About"));
    dlg.setMinimumWidth(500);

    // Brand banner (colour logo on dark), scaled to a comfortable height.
    QLabel *logo = new QLabel(&dlg);
    QPixmap pm(QStringLiteral(":/brand/logo_banner.png"));
    if (!pm.isNull())
        logo->setPixmap(pm.scaledToHeight(46, Qt::SmoothTransformation));

    QLabel *title = new QLabel(QStringLiteral("Infrared Super-Resolution Demo"), &dlg);
    title->setObjectName(QStringLiteral("aboutTitle"));

    const QString ver = QStringLiteral("Application %1    -    %2")
                            .arg(QApplication::applicationVersion(),
                                 m_worker ? m_worker->engineVersion()
                                          : QStringLiteral("SrCoreLib"));
    QLabel *version = new QLabel(ver, &dlg);
    version->setObjectName(QStringLiteral("aboutSub"));

    QFrame *rule = new QFrame(&dlg);
    rule->setFrameShape(QFrame::HLine);
    rule->setObjectName(QStringLiteral("aboutRule"));

    // Company block: name, tagline, copyright, contact - English only.
    QLabel *company = new QLabel(
        QStringLiteral(
            "Wuhan Wavefront Technology Co., Ltd.\n"
            "Infrared - Optoelectronics - AI"),
        &dlg);
    company->setObjectName(QStringLiteral("aboutCompany"));

    QLabel *copyright = new QLabel(
        QStringLiteral(
            "Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. "
            "All rights reserved.\n"
            "wechat: angaio / 13707128443"),
        &dlg);
    copyright->setObjectName(QStringLiteral("aboutBody"));
    copyright->setWordWrap(true);
    copyright->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QDialogButtonBox *box =
        new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, &dlg);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);

    QVBoxLayout *lay = new QVBoxLayout(&dlg);
    lay->setContentsMargins(28, 24, 28, 18);
    lay->setSpacing(10);
    lay->addWidget(logo);
    lay->addSpacing(4);
    lay->addWidget(title);
    lay->addWidget(version);
    lay->addWidget(rule);
    lay->addWidget(company);
    lay->addWidget(copyright);
    lay->addSpacing(6);
    lay->addWidget(box);

    dlg.setStyleSheet(QStringLiteral(
        "QDialog { background: #1e222a; }"
        "QLabel { color: #c8d0da; }"
        "QLabel#aboutTitle { color: #e8edf4; font-size: 16px; font-weight: bold; }"
        "QLabel#aboutSub { color: #8b95a3; }"
        "QLabel#aboutCompany { color: #cdd5df; line-height: 150%; }"
        "QLabel#aboutBody { color: #8b95a3; font-size: 11px; }"
        "QFrame#aboutRule { color: #2c333d; }"
        "QPushButton { background: #0e58a8; color: #ffffff; border: 0;"
        "             border-radius: 4px; padding: 6px 22px; }"
        "QPushButton:hover { background: #1466c4; }"));

    dlg.exec();
}

void MainWindow::buildUi()
{
    /* ---- top toolbar ---- */
    QToolBar *tb = addToolBar(QStringLiteral("Main"));
    tb->setMovable(false);
    tb->setObjectName(QStringLiteral("mainbar"));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);   // icon + label
    tb->setIconSize(QSize(20, 20));

    tb->addWidget(new QLabel(QStringLiteral("  Camera ")));
    m_cameraBox = new QComboBox(this);
    m_cameraBox->setMinimumWidth(200);
    m_cameraBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    tb->addWidget(m_cameraBox);
    populateCameras();

    QAction *aCam = tb->addAction(barIcon(QStringLiteral("camera")),
                                  QStringLiteral("Open Camera"));
    connect(aCam, &QAction::triggered, this, &MainWindow::onOpenCamera);
    QAction *aRescan = tb->addAction(barIcon(QStringLiteral("rescan")),
                                     QStringLiteral("Rescan"));
    aRescan->setToolTip(QStringLiteral("Re-detect connected cameras"));
    connect(aRescan, &QAction::triggered, this, &MainWindow::populateCameras);
    QAction *aFile = tb->addAction(barIcon(QStringLiteral("openfile")),
                                   QStringLiteral("Open File..."));
    connect(aFile, &QAction::triggered, this, &MainWindow::onOpenFile);
    m_actClose = tb->addAction(barIcon(QStringLiteral("close")),
                               QStringLiteral("Close"));
    m_actClose->setToolTip(QStringLiteral("Close the current video source"));
    connect(m_actClose, &QAction::triggered, this, &MainWindow::onCloseVideo);

    tb->addSeparator();

    /* ---- view modes ---- */
    m_viewGroup = new QActionGroup(this);
    m_viewGroup->setExclusive(true);
    struct { const char *text; const char *icon; ViewMode mode; } modes[] = {
        { "Before",  "before",  ViewBefore  },
        { "After",   "after",   ViewAfter   },
        { "Compare", "compare", ViewCompare },
    };
    for (const auto &m : modes) {
        QAction *a = tb->addAction(barIcon(QString::fromLatin1(m.icon)),
                                   QString::fromLatin1(m.text));
        a->setCheckable(true);
        a->setData(int(m.mode));
        m_viewGroup->addAction(a);
        if (m.mode == m_viewMode) a->setChecked(true);
        connect(a, &QAction::triggered, this, &MainWindow::onViewModeChanged);
    }

    tb->addSeparator();

    tb->addWidget(new QLabel(QStringLiteral("  Model ")));
    m_modelBox = new QComboBox(this);
    m_modelBox->setMinimumWidth(150);
    tb->addWidget(m_modelBox);
    connect(m_modelBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModelChanged);

    tb->addWidget(new QLabel(QStringLiteral("  Threads ")));
    m_threadBox = new QSpinBox(this);
    m_threadBox->setRange(1, 32);
    m_threadBox->setValue(qBound(1, QThread::idealThreadCount() / 2, 8));
    m_threadBox->setFixedWidth(58);
    tb->addWidget(m_threadBox);
    connect(m_threadBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onThreadsChanged);

    /* ---- central area: picture over transport bar ---- */
    m_view = new ImageView(this);

    m_btnPlay = new QToolButton(this);
    m_btnPlay->setObjectName(QStringLiteral("transport"));
    m_btnPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_btnPlay->setToolTip(QStringLiteral("Play / Pause"));
    connect(m_btnPlay, &QToolButton::clicked, this, &MainWindow::onPlayPause);

    m_btnStop = new QToolButton(this);
    m_btnStop->setObjectName(QStringLiteral("transport"));
    m_btnStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_btnStop->setToolTip(QStringLiteral("Stop"));
    connect(m_btnStop, &QToolButton::clicked, this, &MainWindow::onStop);

    // Record button: captures the current view (Before / After / Compare) to mp4.
    m_btnRecord = new QToolButton(this);
    m_btnRecord->setObjectName(QStringLiteral("record"));
    m_btnRecord->setCheckable(true);
    m_btnRecord->setToolTipDuration(0);
    m_btnRecord->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnRecord->setIcon(recIcon(false));
    m_btnRecord->setText(QStringLiteral(" Rec View"));
    m_btnRecord->setToolTip(QStringLiteral(
        "Record the current view (Before / After / Compare) to an .mp4 in the record folder"));
    connect(m_btnRecord, &QToolButton::clicked, this, &MainWindow::onRecordToggle);

    // Source record button: archives the original 256x192 frames with minimal
    // compression - the untouched input, for later re-processing or evidence.
    m_btnRecordSrc = new QToolButton(this);
    m_btnRecordSrc->setObjectName(QStringLiteral("record"));
    m_btnRecordSrc->setCheckable(true);
    m_btnRecordSrc->setToolTipDuration(0);
    m_btnRecordSrc->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnRecordSrc->setIcon(recIcon(false));
    m_btnRecordSrc->setText(QStringLiteral(" Rec 256"));
    m_btnRecordSrc->setToolTip(QStringLiteral(
        "Record the original 256x192 source to an .mp4 (high quality, low loss)"));
    connect(m_btnRecordSrc, &QToolButton::clicked, this, &MainWindow::onRecordSrcToggle);

    m_lblNow = new QLabel(QStringLiteral("00:00"));
    m_lblEnd = new QLabel(QStringLiteral("00:00"));
    m_lblNow->setObjectName(QStringLiteral("time"));
    m_lblEnd->setObjectName(QStringLiteral("time"));

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 0);
    m_slider->setTracking(true);
    connect(m_slider, &QSlider::sliderPressed,  this, &MainWindow::onSliderPressed);
    connect(m_slider, &QSlider::sliderMoved,    this, &MainWindow::onSliderMoved);
    connect(m_slider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);

    QFrame *bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("transportbar"));
    QHBoxLayout *hb = new QHBoxLayout(bar);
    hb->setContentsMargins(12, 8, 12, 8);
    hb->setSpacing(10);
    hb->addWidget(m_btnPlay);
    hb->addWidget(m_btnStop);
    hb->addWidget(m_lblNow);
    hb->addWidget(m_slider, 1);
    hb->addWidget(m_lblEnd);
    hb->addWidget(m_btnRecord);
    hb->addWidget(m_btnRecordSrc);

    QWidget *central = new QWidget(this);
    QVBoxLayout *vb = new QVBoxLayout(central);
    vb->setContentsMargins(0, 0, 0, 0);
    vb->setSpacing(0);
    vb->addWidget(m_view, 1);
    vb->addWidget(bar);
    setCentralWidget(central);

    /* ---- status bar ---- */
    m_stSource = new QLabel(QStringLiteral("Source: none"));
    m_stModel  = new QLabel(QStringLiteral("Model: loading..."));
    m_stPerf   = new QLabel(QStringLiteral("-"));
    statusBar()->addWidget(m_stSource, 2);
    statusBar()->addWidget(m_stModel, 3);
    statusBar()->addPermanentWidget(m_stPerf);
}

void MainWindow::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget      { background: #171a1f; color: #d5dbe3; }
        QMenuBar                  { background: #1e222a; color: #c8d0da;
                                    border-bottom: 1px solid #2c333d; }
        QMenuBar::item            { background: transparent; padding: 5px 12px; }
        QMenuBar::item:selected   { background: #2b313a; }
        QMenu                     { background: #232830; color: #c8d0da;
                                    border: 1px solid #333b46; }
        QMenu::item:selected      { background: #0e58a8; color: #ffffff; }
        QMenu::separator          { height: 1px; background: #2c333d; margin: 4px 0; }
        QToolBar#mainbar          { background: #1e222a; border: 0;
                                    border-bottom: 1px solid #2c333d; padding: 5px; spacing: 4px; }
        QToolBar QToolButton      { color: #d5dbe3; padding: 5px 12px; border-radius: 4px; }
        QToolBar QToolButton:hover        { background: #2b313a; }
        QToolBar QToolButton:checked      { background: #0e58a8; color: #ffffff; }
        QLabel                    { color: #98a3b1; }
        QComboBox, QSpinBox       { background: #252a33; color: #d5dbe3;
                                    border: 1px solid #333b46; border-radius: 4px;
                                    padding: 4px 8px; }
        QComboBox:hover, QSpinBox:hover   { border-color: #46506010; }
        QComboBox QAbstractItemView       { background: #252a33; color: #d5dbe3;
                                            selection-background-color: #0e58a8; }
        QFrame#transportbar       { background: #1e222a; border-top: 1px solid #2c333d; }
        QToolButton#transport     { background: #252a33; border: 1px solid #333b46;
                                    border-radius: 5px; padding: 6px 12px; }
        QToolButton#transport:hover       { background: #2f3742; }
        QToolButton#transport:disabled    { color: #4b535e; border-color: #262c34; }
        QToolButton#record        { background: #252a33; border: 1px solid #333b46;
                                    border-radius: 5px; padding: 6px 14px; color: #d5dbe3; }
        QToolButton#record:hover          { background: #2f3742; }
        QToolButton#record:checked        { background: #c0392b; color: #ffffff;
                                            border-color: #c0392b; }
        QToolButton#record:disabled       { color: #4b535e; border-color: #262c34; }
        QLabel#time               { color: #8b95a3; min-width: 44px;
                                    font-family: Consolas, monospace; }
        QSlider::groove:horizontal        { height: 4px; background: #333b46; border-radius: 2px; }
        QSlider::sub-page:horizontal      { background: #0e58a8; border-radius: 2px; }
        QSlider::handle:horizontal        { background: #e8edf4; width: 13px;
                                            margin: -5px 0; border-radius: 6px; }
        QSlider::handle:horizontal:hover  { background: #ffffff; }
        QSlider:disabled QSlider::sub-page:horizontal { background: #333b46; }
        QStatusBar                { background: #1e222a; border-top: 1px solid #2c333d; }
        QStatusBar QLabel         { color: #8b95a3; padding: 0 6px; }
    )"));
}

void MainWindow::populateCameras()
{
    const int keep = m_cameraBox->currentData().isValid()
                         ? m_cameraBox->currentData().toInt() : -1;
    m_cameraBox->blockSignals(true);
    m_cameraBox->clear();

    const QVector<CameraInfo> cams = enumerateCameras();
    for (const CameraInfo &c : cams)
        m_cameraBox->addItem(c.name, c.index);   // show the name, carry the index

    if (m_cameraBox->count() == 0) {
        // Nothing detected: still offer index 0 so a plugged-in-later cam works.
        m_cameraBox->addItem(QStringLiteral("(no camera detected)"), 0);
    }
    if (keep >= 0) {
        const int i = m_cameraBox->findData(keep);
        if (i >= 0) m_cameraBox->setCurrentIndex(i);
    }
    m_cameraBox->blockSignals(false);
}

void MainWindow::populateModels()
{
    m_modelBox->blockSignals(true);
    m_modelBox->clear();
    const int n = m_worker->modelCount();
    for (int i = 0; i < n; ++i) {
        m_modelBox->addItem(m_worker->modelName(i), i);
        m_modelBox->setItemData(i, m_worker->modelHint(i), Qt::ToolTipRole);
    }
    m_modelBox->blockSignals(false);

    if (n == 0) {
        m_stModel->setText(QStringLiteral("Model: none available"));
        m_view->setPlaceholder(QStringLiteral("The engine reports no models."));
        return;
    }

    // Default to the "Detail" model by name, so it stays the default even if the
    // model table is ever reordered; fall back to the first model if it is absent.
    int defaultIdx = 0;
    for (int i = 0; i < n; ++i)
        if (m_worker->modelName(i).startsWith(QStringLiteral("Detail"),
                                              Qt::CaseInsensitive)) {
            defaultIdx = i;
            break;
        }
    m_modelBox->blockSignals(true);
    m_modelBox->setCurrentIndex(defaultIdx);
    m_modelBox->blockSignals(false);
    m_worker->requestModel(defaultIdx, m_threadBox->value());
}

/* ------------------------------------------------------------------ source */

void MainWindow::releaseSource()
{
    if (m_recorder.isOpen())    { m_recorder.close();    updateRecordUi(false); }
    if (m_srcRecorder.isOpen()) { m_srcRecorder.close(); updateRecordSrcUi(false); }
    if (m_source) {
        m_source->stop();
        m_source->wait(2000);
        m_source->deleteLater();
        m_source = nullptr;
    }
}

void MainWindow::startSource(bool camera, const QString &path)
{
    releaseSource();
    m_source = new VideoController(this);
    connect(m_source, &VideoController::frameReady,      this, &MainWindow::onFrame);
    connect(m_source, &VideoController::positionChanged, this, &MainWindow::onPosition);
    connect(m_source, &VideoController::stateChanged,    this, &MainWindow::onStateChanged);
    connect(m_source, &VideoController::sourceClosed,    this, &MainWindow::onSourceClosed);

    bool ok = camera
        ? m_source->openCamera(m_cameraBox->currentData().toInt(), 256, 192)
        : m_source->openFile(path);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Cannot open source"),
                             m_source->lastError());
        releaseSource();
        updateTransport();
        return;
    }

    if (!camera && !path.isEmpty())
        addRecentFile(path);

    m_stSource->setText(QStringLiteral("Source: %1").arg(m_source->sourceName()));
    m_slider->setRange(0, int(qMax<qint64>(0, m_source->frameCount() - 1)));
    m_slider->setValue(0);
    m_lblEnd->setText(m_source->timeLabel(m_source->frameCount()));
    m_source->play();
    updateTransport();
}

void MainWindow::onOpenCamera() { startSource(true, QString()); }

void MainWindow::onOpenFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open video"), QString(),
        QStringLiteral("Video files (*.mp4 *.avi *.wmv *.mkv *.mov);;All files (*.*)"));
    if (path.isEmpty()) return;
    startSource(false, path);
}

/* --------------------------------------------------------------- transport */

void MainWindow::onPlayPause()
{
    if (!m_source) return;
    if (m_source->state() == VideoController::Playing) m_source->pause();
    else                                               m_source->play();
}

void MainWindow::onStop()
{
    if (m_recorder.isOpen())    { m_recorder.close();    updateRecordUi(false); }
    if (m_srcRecorder.isOpen()) { m_srcRecorder.close(); updateRecordSrcUi(false); }
    releaseSource();
    m_view->clear();
    m_view->setPlaceholder(QStringLiteral("Stopped"));
    m_stSource->setText(QStringLiteral("Source: none"));
    m_slider->setRange(0, 0);
    m_lblNow->setText(QStringLiteral("00:00"));
    m_lblEnd->setText(QStringLiteral("00:00"));
    updateTransport();
}

void MainWindow::onCloseVideo()
{
    if (!m_source) return;
    // Finalise any recording in progress so its file is saved, then release.
    if (m_recorder.isOpen())    { m_recorder.close();    updateRecordUi(false); }
    if (m_srcRecorder.isOpen()) { m_srcRecorder.close(); updateRecordSrcUi(false); }
    releaseSource();
    m_view->clear();
    m_view->setPlaceholder(QStringLiteral("No source open"));
    m_stSource->setText(QStringLiteral("Source: none"));
    m_slider->setRange(0, 0);
    m_lblNow->setText(QStringLiteral("00:00"));
    m_lblEnd->setText(QStringLiteral("00:00"));
    updateTransport();
}

void MainWindow::updateTransport()
{
    const bool live = (m_source != nullptr);
    const bool rec  = anyRecording();
    const bool seekable = live && m_source->seekable();
    m_btnPlay->setEnabled(live);
    m_btnStop->setEnabled(live && !rec);   // stopping the source would cut the file
    m_slider->setEnabled(seekable);
    // Both need a live source; they are independent (you can archive the raw
    // 256x192 while also recording the view). Each button stays live so it can
    // still be pressed to stop its own recording.
    m_btnRecord->setEnabled(live);
    m_btnRecordSrc->setEnabled(live);
    if (m_actClose) m_actClose->setEnabled(live);   // only closable when open
    m_btnPlay->setIcon(style()->standardIcon(
        (live && m_source->state() == VideoController::Playing)
            ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

void MainWindow::onSliderPressed()  { m_scrubbing = true; }

void MainWindow::onSliderMoved(int value)
{
    /* Seek on every move: VideoController keeps only the newest request, so
     * dragging stays responsive even when decoding is slow. */
    if (m_source) m_source->seek(value);
    m_lblNow->setText(m_source ? m_source->timeLabel(value)
                               : QStringLiteral("00:00"));
}

void MainWindow::onSliderReleased() { m_scrubbing = false; }

void MainWindow::onPosition(qint64 frame, qint64 total)
{
    if (!m_source) return;
    if (!m_scrubbing) {
        if (total > 0 && m_slider->maximum() != int(total - 1))
            m_slider->setRange(0, int(total - 1));
        m_slider->setValue(int(frame));
    }
    m_lblNow->setText(m_source->timeLabel(frame));
}

void MainWindow::onStateChanged(int) { updateTransport(); }

void MainWindow::onSourceClosed()
{
    m_stSource->setText(QStringLiteral("Source: ended"));
    updateTransport();
}

/* ------------------------------------------------------------------- model */

void MainWindow::onViewModeChanged()
{
    if (QAction *a = m_viewGroup->checkedAction())
        m_viewMode = ViewMode(a->data().toInt());
}

void MainWindow::onModelChanged(int index)
{
    if (index < 0) return;
    m_stModel->setText(QStringLiteral("Model: switching..."));
    m_worker->requestModel(index, m_threadBox->value());
}

void MainWindow::onThreadsChanged(int value)
{
    const int idx = m_modelBox->currentIndex();
    if (idx >= 0) m_worker->requestModel(idx, value);
}

void MainWindow::onModelReady(int, const QString &summary)
{
    m_stModel->setText(QStringLiteral("Model: %1").arg(summary));
    m_avgMs = 0.0;
}

void MainWindow::onWorkerFailed(const QString &message)
{
    statusBar()->showMessage(message, 6000);
    m_view->setPlaceholder(message);
}

/* -------------------------------------------------------------- frame path */

void MainWindow::onFrame(const cv::Mat &frame)
{
    if (m_worker) m_worker->submitFrame(frame);
}

QImage MainWindow::toQImage(const cv::Mat &m)
{
    if (m.empty()) return QImage();
    if (m.type() == CV_8UC1)
        return QImage(m.data, m.cols, m.rows, int(m.step),
                      QImage::Format_Grayscale8).copy();
    if (m.type() == CV_8UC3)
        return QImage(m.data, m.cols, m.rows, int(m.step),
                      QImage::Format_BGR888).copy();
    return QImage();
}

cv::Mat MainWindow::stitchPair(const cv::Mat &left, const cv::Mat &right)
{
    // Same channel count for both, then place side by side with an 8px divider.
    cv::Mat l = left, r = right;
    if (l.channels() != r.channels()) {
        if (l.channels() == 1) cv::cvtColor(l, l, cv::COLOR_GRAY2BGR);
        if (r.channels() == 1) cv::cvtColor(r, r, cv::COLOR_GRAY2BGR);
    }
    const int h = std::max(l.rows, r.rows);
    const int gap = 8;
    cv::Mat out(h, l.cols + gap + r.cols, l.type(), cv::Scalar::all(20));
    l.copyTo(out(cv::Rect(0, 0, l.cols, l.rows)));
    r.copyTo(out(cv::Rect(l.cols + gap, 0, r.cols, r.rows)));
    return out;
}

void MainWindow::onRecordToggle()
{
    if (m_recorder.isOpen()) {
        const QString p = m_recorder.path();
        const long n = m_recorder.frameCount();
        m_recorder.close();
        updateRecordUi(false);
        statusBar()->showMessage(
            QStringLiteral("Saved %1 frames to %2").arg(n).arg(p), 6000);
        updateTransport();
        return;
    }

    if (!m_source) {
        QMessageBox::information(this, QStringLiteral("Record"),
            QStringLiteral("Open a camera or a file first."));
        return;
    }
    if (m_lastShown.empty()) {
        QMessageBox::information(this, QStringLiteral("Record"),
            QStringLiteral("Waiting for the first processed frame - try again in a moment."));
        return;
    }

    // Auto-named into the record folder, tagged by the view so the three modes
    // do not overwrite each other. No save dialog by design.
    const char *tag = (m_viewMode == ViewBefore) ? "before"
                    : (m_viewMode == ViewAfter)  ? "after" : "compare";
    const QString path =
        newRecordPath(QStringLiteral("view_%1").arg(QString::fromLatin1(tag)));

    const double fps = m_source->fps() > 0 ? m_source->fps() : 25.0;
    if (!m_recorder.open(path, m_lastShown, fps)) {
        QMessageBox::warning(this, QStringLiteral("Record"),
            QStringLiteral("Could not start recording:\n%1").arg(m_recorder.lastError()));
        return;
    }
    updateRecordUi(true);
    updateTransport();
    statusBar()->showMessage(QStringLiteral("Recording %1 view -> %2")
                                 .arg(QString::fromLatin1(tag), path), 4000);
}

void MainWindow::onRecordSrcToggle()
{
    if (m_srcRecorder.isOpen()) {
        const QString p = m_srcRecorder.path();
        const long n = m_srcRecorder.frameCount();
        m_srcRecorder.close();
        updateRecordSrcUi(false);
        statusBar()->showMessage(
            QStringLiteral("Saved %1 source frames to %2").arg(n).arg(p), 6000);
        updateTransport();
        return;
    }

    if (!m_source) {
        QMessageBox::information(this, QStringLiteral("Record"),
            QStringLiteral("Open a camera or a file first."));
        return;
    }
    if (m_lastBefore.empty()) {
        QMessageBox::information(this, QStringLiteral("Record"),
            QStringLiteral("Waiting for the first frame - try again in a moment."));
        return;
    }

    // Original 256x192 at the source frame rate, recorded with the low-loss
    // Archive profile (intra-frame MJPEG) to keep information loss minimal.
    const QString path = newRecordPath(QStringLiteral("src_256x192"));
    const double fps = m_source->fps() > 0 ? m_source->fps() : 25.0;
    if (!m_srcRecorder.open(path, m_lastBefore, fps, VideoRecorder::Archive)) {
        QMessageBox::warning(this, QStringLiteral("Record"),
            QStringLiteral("Could not start recording:\n%1").arg(m_srcRecorder.lastError()));
        return;
    }
    updateRecordSrcUi(true);
    updateTransport();
    statusBar()->showMessage(
        QStringLiteral("Recording source 256x192 -> %1").arg(path), 4000);
}

bool MainWindow::anyRecording() const
{
    return m_recorder.isOpen() || m_srcRecorder.isOpen();
}

void MainWindow::updateRecordUi(bool recording)
{
    if (!m_btnRecord) return;
    m_btnRecord->setChecked(recording);
    m_btnRecord->setIcon(recIcon(recording));
    m_btnRecord->setText(recording ? QStringLiteral(" Stop") : QStringLiteral(" Rec View"));
    // Lock the view while view-recording: switching would change the frame
    // geometry mid-file. The user stops recording first, then changes the view.
    if (m_viewGroup) m_viewGroup->setEnabled(!recording);
    updateTransport();
}

void MainWindow::updateRecordSrcUi(bool recording)
{
    if (!m_btnRecordSrc) return;
    m_btnRecordSrc->setChecked(recording);
    m_btnRecordSrc->setIcon(recIcon(recording));
    m_btnRecordSrc->setText(recording ? QStringLiteral(" Stop") : QStringLiteral(" Rec 256"));
    // The source is the same 256x192 in every view, so no view lock is needed.
    updateTransport();
}

/* -------------------------------------------------------------- record io */

QString MainWindow::recordDir() const
{
    QDir d(QCoreApplication::applicationDirPath());
    d.mkpath(QStringLiteral("record"));
    return d.filePath(QStringLiteral("record"));
}

QString MainWindow::newRecordPath(const QString &prefix) const
{
    const QString ts =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(recordDir())
        .filePath(QStringLiteral("%1_%2.mp4").arg(prefix, ts));
}

QIcon MainWindow::recIcon(bool stop)
{
    const int S = 28;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!stop) {
        // Classic REC dot: a red disc with a soft highlight and a faint ring.
        QRadialGradient g(S * 0.42, S * 0.40, S * 0.55);
        g.setColorAt(0.0, QColor(0xff, 0x6b, 0x63));
        g.setColorAt(1.0, QColor(0xcc, 0x2b, 0x24));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawEllipse(QRectF(6, 6, S - 12, S - 12));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(255, 255, 255, 70), 1.4));
        p.drawEllipse(QRectF(5.2, 5.2, S - 10.4, S - 10.4));
    } else {
        // Recording: a white rounded Stop square (reads on the red button).
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xff, 0xff));
        p.drawRoundedRect(QRectF(8, 8, S - 16, S - 16), 2.5, 2.5);
    }
    p.end();
    return QIcon(pm);
}

QIcon MainWindow::barIcon(const QString &kind)
{
    const int S = 28;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor ink(0xd8, 0xde, 0xe6);      // light stroke for the dark bar
    const QColor accent(0x39, 0xba, 0xd6);   // brand cool blue
    QPen pen(ink, 2.0);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (kind == QLatin1String("camera")) {
        p.drawRoundedRect(QRectF(4, 9, 20, 13), 2.5, 2.5);      // body
        p.drawLine(QPointF(9, 9), QPointF(11, 6.5));            // viewfinder bump
        p.drawLine(QPointF(11, 6.5), QPointF(15, 6.5));
        p.drawLine(QPointF(15, 6.5), QPointF(17, 9));
        p.drawEllipse(QPointF(14, 15.5), 3.4, 3.4);            // lens
    } else if (kind == QLatin1String("rescan")) {
        p.drawArc(QRectF(6.5, 6.5, 15, 15), 25 * 16, 300 * 16);   // ~5/6 circle
        p.setPen(Qt::NoPen);                                      // filled arrowhead
        p.setBrush(ink);
        QPolygonF head;
        head << QPointF(22.6, 12.2) << QPointF(18.4, 13.1) << QPointF(21.0, 8.9);
        p.drawPolygon(head);
    } else if (kind == QLatin1String("openfile")) {
        p.drawLine(QPointF(4, 8.5), QPointF(10, 8.5));         // folder tab
        p.drawLine(QPointF(10, 8.5), QPointF(12, 11));
        p.drawRoundedRect(QRectF(4, 11, 20, 11), 2, 2);        // folder body
    } else if (kind == QLatin1String("close")) {
        p.drawEllipse(QPointF(14, 14), 9.5, 9.5);              // ring
        p.setPen(QPen(accent, 2.3, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(10.6, 10.6), QPointF(17.4, 17.4));  // X
        p.drawLine(QPointF(17.4, 10.6), QPointF(10.6, 17.4));
    } else if (kind == QLatin1String("before")) {
        p.drawRoundedRect(QRectF(6, 8, 16, 12), 2, 2);         // single panel
    } else if (kind == QLatin1String("after")) {
        p.drawRoundedRect(QRectF(6, 8, 16, 12), 2, 2);         // panel + sparkle
        p.setPen(QPen(accent, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(14, 10.6), QPointF(14, 17.4));
        p.drawLine(QPointF(10.6, 14), QPointF(17.4, 14));
    } else if (kind == QLatin1String("compare")) {
        p.drawRoundedRect(QRectF(4, 8, 9, 12), 1.5, 1.5);      // two panels
        p.drawRoundedRect(QRectF(15, 8, 9, 12), 1.5, 1.5);
    }
    p.end();
    return QIcon(pm);
}

/* ------------------------------------------------------------- recent files */

void MainWindow::addRecentFile(const QString &path)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    m_recentFiles.removeAll(abs);
    m_recentFiles.prepend(abs);
    while (m_recentFiles.size() > 8) m_recentFiles.removeLast();
    QSettings().setValue(QStringLiteral("recentFiles"), m_recentFiles);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    if (m_recentFiles.isEmpty()) {
        QAction *none = m_recentMenu->addAction(QStringLiteral("(none)"));
        none->setEnabled(false);
        return;
    }
    int i = 1;
    for (const QString &f : m_recentFiles) {
        QAction *a = m_recentMenu->addAction(
            QStringLiteral("&%1  %2").arg(i++).arg(QFileInfo(f).fileName()));
        a->setData(f);
        a->setToolTip(f);
        connect(a, &QAction::triggered, this, &MainWindow::openRecentFile);
    }
    m_recentMenu->addSeparator();
    QAction *clr = m_recentMenu->addAction(QStringLiteral("Clear List"));
    connect(clr, &QAction::triggered, this, [this] {
        m_recentFiles.clear();
        QSettings().setValue(QStringLiteral("recentFiles"), m_recentFiles);
        rebuildRecentMenu();
    });
}

void MainWindow::openRecentFile()
{
    QAction *a = qobject_cast<QAction *>(sender());
    if (!a) return;
    const QString path = a->data().toString();
    if (path.isEmpty()) return;
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Open Recent"),
            QStringLiteral("This file no longer exists:\n%1").arg(path));
        m_recentFiles.removeAll(path);
        QSettings().setValue(QStringLiteral("recentFiles"), m_recentFiles);
        rebuildRecentMenu();
        return;
    }
    startSource(false, path);
}

void MainWindow::onResult(const cv::Mat &before, const cv::Mat &after, double ms)
{
    /* Bring "before" up to the output size, otherwise the two panels are not
     * comparable. Bicubic is the baseline any viewer would get for free. */
    cv::Mat beforeUp;
    cv::resize(before, beforeUp, after.size(), 0, 0, cv::INTER_CUBIC);

    const QString capL = QStringLiteral("BEFORE  %1x%2  (conventional upscale)")
                             .arg(before.cols).arg(before.rows);
    const QString capR = QStringLiteral("AFTER  %1x%2").arg(after.cols).arg(after.rows);

    // Build the frame the current view shows, so recording captures exactly
    // what the user sees: Before -> beforeUp, After -> after, Compare -> the two
    // panels stitched left|right with a thin divider.
    cv::Mat shown;
    switch (m_viewMode) {
    case ViewBefore:
        m_view->setSingle(toQImage(beforeUp), capL);
        shown = beforeUp;
        break;
    case ViewAfter:
        m_view->setSingle(toQImage(after), capR);
        shown = after;
        break;
    default:
        m_view->setPair(toQImage(beforeUp), toQImage(after), capL, capR);
        shown = stitchPair(beforeUp, after);
        break;
    }

    m_lastShown  = shown;             // representative frame for view recording
    m_lastBefore = before;            // original 256x192, for source recording
    if (m_recorder.isOpen())
        m_recorder.writeFrame(shown);
    if (m_srcRecorder.isOpen())
        m_srcRecorder.writeFrame(before);

    m_avgMs = (m_avgMs <= 0.0) ? ms : m_avgMs * 0.9 + ms * 0.1;
    ++m_fpsCount;
    const qint64 el = m_fpsTimer.elapsed();
    if (el >= 500) {
        m_fps = m_fpsCount * 1000.0 / double(el);
        m_fpsCount = 0;
        m_fpsTimer.restart();
    }
    m_stPerf->setText(QStringLiteral("%1 ms   %2 fps")
                          .arg(m_avgMs, 0, 'f', 1).arg(m_fps, 0, 'f', 1));
}

/* --------------------------------------------------------------- startup */

void MainWindow::applyStartupOptions(const QString &file, int camera,
                                     const QString &model, const QString &view)
{
    if (!view.isEmpty()) {
        const ViewMode m =
            view.compare(QStringLiteral("before"), Qt::CaseInsensitive) == 0 ? ViewBefore :
            view.compare(QStringLiteral("after"),  Qt::CaseInsensitive) == 0 ? ViewAfter  :
                                                                               ViewCompare;
        m_viewMode = m;
        const auto acts = m_viewGroup->actions();
        for (QAction *a : acts)
            if (a->data().toInt() == int(m)) a->setChecked(true);
    }
    if (!model.isEmpty()) {
        const int idx = m_modelBox->findText(model, Qt::MatchFixedString);
        if (idx >= 0) m_modelBox->setCurrentIndex(idx);
        else statusBar()->showMessage(
            QStringLiteral("No such model: %1").arg(model), 5000);
    }
    if (camera >= 0) {
        const int i = m_cameraBox->findData(camera);
        if (i >= 0) m_cameraBox->setCurrentIndex(i);
        startSource(true, QString());
    } else if (!file.isEmpty()) {
        startSource(false, file);
    }
}
