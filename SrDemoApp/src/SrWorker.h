/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : SrWorker.h
* Description : Runs the upscaler on its own thread.
*
* Only the newest frame is kept: if one arrives while the previous is still in
* flight, it overwrites it. Dropping frames keeps the picture in step with the
* source; a queue would only add latency. The same policy is used by the
* shipping products this demo is derived from.
*
* Model switching is queued too and applied at the top of the loop, so it never
* interrupts a frame already being processed.
******************************************************************************/
#ifndef ___SrWorker_h___
#define ___SrWorker_h___

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <memory>
#include <opencv2/core.hpp>

#include "SrInterface.h"

class SrWorker : public QThread
{
    Q_OBJECT
public:
    explicit SrWorker(QObject *parent = nullptr);
    ~SrWorker() override;

    /* Queried by the UI to populate the model list. Safe before start(). */
    int         modelCount() const;
    QString     modelName(int index) const;
    QString     modelHint(int index) const;
    QString     engineVersion() const;

    void submitFrame(const cv::Mat &frame);
    void requestModel(int index, int threads);
    void shutdown();

    /* Destroy the inference engine. Call only after the thread has stopped
     * (shutdown() + wait()); it releases the native runtime deterministically
     * on the caller's thread instead of during process teardown. */
    void releaseEngine();

signals:
    /* before: the frame as fed to the engine; after: the upscaled result. */
    void resultReady(const cv::Mat &before, const cv::Mat &after, double ms);
    void modelReady(int index, const QString &summary);
    void failed(const QString &message);

protected:
    void run() override;

private:
    /* Crops the captured frame down to what the engine expects.
     * Width must match exactly. Height only has to be >= the engine height:
     * the top rows are the picture, anything beyond is sensor telemetry or a
     * second data plane, and is discarded. */
    bool prepare(const cv::Mat &raw, cv::Mat &out, QString &why) const;

    std::shared_ptr<SrEngine> m_engine;

    QMutex         m_mtx;
    QWaitCondition m_cv;
    cv::Mat        m_pending;
    bool           m_hasFrame = false;
    bool           m_quit     = false;

    int  m_reqModel   = -1;
    int  m_reqThreads = 0;
    bool m_hasModelReq = false;

    QString m_lastWhy;     // suppresses repeats of the same size complaint
};

#endif
