/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "VideoController.h"
#include "CameraEnum.h"

#include <QFileInfo>
#include <QElapsedTimer>

VideoController::VideoController(QObject *parent) : QThread(parent) {}

VideoController::~VideoController()
{
    stop();
    wait(3000);
}

bool VideoController::openCamera(int index, int reqW, int reqH)
{
    m_err.clear();
#ifdef _WIN32
    if (!m_cap.open(index, cv::CAP_MSMF))
        m_cap.open(index, cv::CAP_ANY);
#else
    if (!m_cap.open(index, cv::CAP_V4L2))
        m_cap.open(index, cv::CAP_ANY);
#endif
    if (!m_cap.isOpened()) {
        m_err = QStringLiteral("Cannot open camera #%1").arg(index);
        return false;
    }
    if (reqW > 0 && reqH > 0) {
        m_cap.set(cv::CAP_PROP_FRAME_WIDTH,  reqW);
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, reqH);
    }
    const int w = int(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int h = int(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    // This is a 256-wide IR upscaler. Reject anything else up front, so the user
    // gets a clear message instead of a mysterious "input width" error later.
    // Width must be exactly 256; height must be >= 192 (some modules append a few
    // telemetry rows, e.g. 256x196, or stack a temperature plane, 256x384 - those
    // are fine, the extra rows are dropped downstream).
    if (w != 256 || h < 192) {
        m_err = QStringLiteral(
                    "This camera outputs %1x%2, which is not supported.\n\n"
                    "A 256-wide infrared source is required "
                    "(width must be 256, height at least 192).")
                    .arg(w).arg(h);
        m_cap.release();
        return false;
    }

    m_isFile = false;
    m_frames = 0;                       // live source: no timeline
    m_fps    = m_cap.get(cv::CAP_PROP_FPS);
    if (m_fps <= 1.0 || m_fps > 240.0) m_fps = 25.0;
    m_pos = 0;

    // Resolve the friendly device name for this index for the status bar.
    QString cam = QStringLiteral("Camera %1").arg(index);
    for (const CameraInfo &c : enumerateCameras())
        if (c.index == index) { cam = c.name; break; }
    m_name = QStringLiteral("%1  %2x%3").arg(cam).arg(w).arg(h);
    return true;
}

bool VideoController::openFile(const QString &path)
{
    m_err.clear();
    if (!m_cap.open(path.toLocal8Bit().constData())) {
        m_err = QStringLiteral("Cannot open file: %1").arg(path);
        return false;
    }
    const int w = int(m_cap.get(cv::CAP_PROP_FRAME_WIDTH));
    const int h = int(m_cap.get(cv::CAP_PROP_FRAME_HEIGHT));

    // Only native 256x192 infrared clips are accepted. An upscaled result or a
    // Compare recording (e.g. 1024x768 / 2056x768) is not a valid input, so
    // reject it here with a clear message rather than upscaling an upscale.
    if (w != 256 || h != 192) {
        m_err = QStringLiteral(
                    "This clip is %1x%2. Only 256x192 infrared clips can be opened.")
                    .arg(w).arg(h);
        m_cap.release();
        return false;
    }

    m_isFile = true;
    m_fps    = m_cap.get(cv::CAP_PROP_FPS);
    if (m_fps <= 1.0 || m_fps > 240.0) m_fps = 25.0;
    m_frames = qint64(m_cap.get(cv::CAP_PROP_FRAME_COUNT));
    if (m_frames < 0) m_frames = 0;
    m_pos = 0;
    m_name = QStringLiteral("%1  %2x%3  %4 fps")
                 .arg(QFileInfo(path).fileName()).arg(w).arg(h)
                 .arg(m_fps, 0, 'f', 1);
    return true;
}

QString VideoController::timeLabel(qint64 frame) const
{
    const double sec = (m_fps > 0.0) ? double(frame) / m_fps : 0.0;
    const int t = int(sec + 0.5);
    return QStringLiteral("%1:%2").arg(t / 60, 2, 10, QLatin1Char('0'))
                                  .arg(t % 60, 2, 10, QLatin1Char('0'));
}

void VideoController::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged(int(s));
}

void VideoController::play()
{
    QMutexLocker lk(&m_mtx);
    if (m_state == Playing) return;
    setState(Playing);
    m_cv.wakeAll();
    if (!isRunning()) start();
}

void VideoController::pause()
{
    QMutexLocker lk(&m_mtx);
    if (m_state != Playing) return;
    setState(Paused);
}

void VideoController::stop()
{
    {
        QMutexLocker lk(&m_mtx);
        m_quit = true;
        setState(Stopped);
        m_cv.wakeAll();
    }
    wait(2000);
}

void VideoController::seek(qint64 frame)
{
    QMutexLocker lk(&m_mtx);
    if (!m_isFile) return;
    if (frame < 0) frame = 0;
    if (m_frames > 0 && frame >= m_frames) frame = m_frames - 1;
    m_seekTo   = frame;
    m_seekPend = true;      // newest request wins; older ones are dropped
    m_cv.wakeAll();
}

void VideoController::run()
{
    {
        QMutexLocker lk(&m_mtx);
        m_quit = false;
    }

    const int stepMs = (m_isFile && m_fps > 0.0) ? int(1000.0 / m_fps) : 0;
    QElapsedTimer tick;
    tick.start();

    while (true) {
        bool doSeek = false;
        qint64 seekTo = 0;

        {
            QMutexLocker lk(&m_mtx);
            /* Sleep while paused, but stay responsive to seek and quit. */
            while (!m_quit && m_state == Paused && !m_seekPend)
                m_cv.wait(&m_mtx);
            if (m_quit) break;
            if (m_seekPend) { doSeek = true; seekTo = m_seekTo; m_seekPend = false; }
        }

        if (doSeek) {
            m_cap.set(cv::CAP_PROP_POS_FRAMES, double(seekTo));
            m_pos = seekTo;
            /* Deliver one frame even while paused so the picture tracks the
             * slider handle - this is what makes scrubbing feel smooth. */
            cv::Mat f;
            if (m_cap.read(f) && !f.empty()) {
                ++m_pos;
                emit frameReady(f);
                emit positionChanged(m_pos, m_frames);
            }
            tick.restart();
            continue;
        }

        cv::Mat frame;
        if (!m_cap.read(frame) || frame.empty()) {
            if (m_isFile) {
                m_cap.set(cv::CAP_PROP_POS_FRAMES, 0.0);   // loop for demoing
                m_pos = 0;
                if (!m_cap.read(frame) || frame.empty()) break;
            } else {
                break;
            }
        }
        ++m_pos;
        emit frameReady(frame);
        emit positionChanged(m_pos, m_frames);

        if (stepMs > 0) {
            const qint64 used = tick.elapsed();
            if (used < stepMs) msleep(ulong(stepMs - used));
            tick.restart();
        }
    }

    m_cap.release();
    {
        QMutexLocker lk(&m_mtx);
        setState(Stopped);
    }
    emit sourceClosed();
}
