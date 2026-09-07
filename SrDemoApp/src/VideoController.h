/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : VideoController.h
* Description : Video source with transport control.
*
* Handles both a UVC camera and a media file behind one interface:
*
*   file   - play / pause / resume / stop, and seeking anywhere on the timeline
*   camera - play / pause / resume / stop; seeking is meaningless and reported
*            as unavailable so the UI can grey the slider out
*
* Seeks are coalesced: while the user drags the slider only the newest request
* survives, which keeps scrubbing smooth on slow codecs. A seek always emits
* one frame, even while paused, so the picture follows the handle.
******************************************************************************/
#ifndef ___VideoController_h___
#define ___VideoController_h___

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

Q_DECLARE_METATYPE(cv::Mat)

class VideoController : public QThread
{
    Q_OBJECT
public:
    enum State { Stopped = 0, Playing, Paused };

    explicit VideoController(QObject *parent = nullptr);
    ~VideoController() override;

    /* Both return false on failure; see lastError(). */
    bool openCamera(int index, int reqW = 0, int reqH = 0);
    bool openFile(const QString &path);

    void play();      // start, or resume from Paused
    void pause();
    void stop();      // ends the thread and releases the source
    void seek(qint64 frame);

    State   state()      const { return m_state; }
    bool    seekable()   const { return m_isFile && m_frames > 0; }
    qint64  frameCount() const { return m_frames; }
    double  fps()        const { return m_fps; }
    QString sourceName() const { return m_name; }
    QString lastError()  const { return m_err; }

    /* Formats a frame index as mm:ss using the source frame rate. */
    QString timeLabel(qint64 frame) const;

signals:
    void frameReady(const cv::Mat &frame);
    void positionChanged(qint64 frame, qint64 total);
    void stateChanged(int state);
    void sourceClosed();

protected:
    void run() override;

private:
    void setState(State s);

    cv::VideoCapture m_cap;
    QString m_err, m_name;
    bool    m_isFile = false;
    qint64  m_frames = 0;
    double  m_fps    = 25.0;

    mutable QMutex m_mtx;
    QWaitCondition m_cv;
    State  m_state     = Stopped;
    bool   m_quit      = false;
    bool   m_seekPend  = false;
    qint64 m_seekTo    = 0;
    qint64 m_pos       = 0;
};

#endif
