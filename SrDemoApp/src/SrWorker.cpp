/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "SrWorker.h"

#include <opencv2/imgproc.hpp>

SrWorker::SrWorker(QObject *parent) : QThread(parent)
{
    m_engine = SrEngineFactory::createEngine();
}

SrWorker::~SrWorker()
{
    shutdown();
    wait(3000);
}

int SrWorker::modelCount() const
{
    return m_engine ? m_engine->modelCount() : 0;
}

QString SrWorker::modelName(int index) const
{
    return m_engine ? QString::fromUtf8(m_engine->modelName(index)) : QString();
}

QString SrWorker::modelHint(int index) const
{
    return m_engine ? QString::fromUtf8(m_engine->modelHint(index)) : QString();
}

QString SrWorker::engineVersion() const
{
    return QString::fromUtf8(SrEngineFactory::version());
}

void SrWorker::submitFrame(const cv::Mat &frame)
{
    QMutexLocker lk(&m_mtx);
    m_pending = frame;      // cv::Mat is refcounted, this is not a deep copy
    m_hasFrame = true;
    m_cv.wakeOne();
}

void SrWorker::requestModel(int index, int threads)
{
    QMutexLocker lk(&m_mtx);
    m_reqModel = index;
    m_reqThreads = threads;
    m_hasModelReq = true;
    m_cv.wakeOne();
}

void SrWorker::shutdown()
{
    QMutexLocker lk(&m_mtx);
    m_quit = true;
    m_cv.wakeAll();
}

void SrWorker::releaseEngine()
{
    m_engine.reset();     // releases the inference engine on this thread
}

bool SrWorker::prepare(const cv::Mat &raw, cv::Mat &out, QString &why) const
{
    const int w = m_engine->inputWidth();
    const int h = m_engine->inputHeight();

    if (raw.cols != w) {
        why = QStringLiteral("Input width must be %1, got %2x%3")
                  .arg(w).arg(raw.cols).arg(raw.rows);
        return false;
    }
    if (raw.rows < h) {
        why = QStringLiteral("Input needs at least %1 rows, got %2")
                  .arg(h).arg(raw.rows);
        return false;
    }
    /* Take the top h rows. This covers the exact case, a sensor that appends
     * a few telemetry rows, and a module that stacks picture over data. */
    out = (raw.rows == h) ? raw : raw(cv::Rect(0, 0, w, h)).clone();
    return true;
}

void SrWorker::run()
{
    if (!m_engine) {
        emit failed(QStringLiteral("Engine could not be created."));
        return;
    }

    while (true) {
        cv::Mat frame;
        int reqModel = -1, reqThreads = 0;
        bool doModel = false;

        {
            QMutexLocker lk(&m_mtx);
            while (!m_quit && !m_hasFrame && !m_hasModelReq)
                m_cv.wait(&m_mtx);
            if (m_quit) break;

            if (m_hasModelReq) {
                reqModel = m_reqModel;
                reqThreads = m_reqThreads;
                m_hasModelReq = false;
                doModel = true;
            }
            if (m_hasFrame) {
                frame = m_pending;
                m_hasFrame = false;
            }
        }

        if (doModel) {
            if (m_engine->selectModel(reqModel, reqThreads)) {
                m_lastWhy.clear();
                const QString summary =
                    QStringLiteral("%1  |  %2x%3 -> %4x%5  (x%6)")
                        .arg(modelName(reqModel))
                        .arg(m_engine->inputWidth()).arg(m_engine->inputHeight())
                        .arg(m_engine->inputWidth() * m_engine->scale())
                        .arg(m_engine->inputHeight() * m_engine->scale())
                        .arg(m_engine->scale());
                emit modelReady(reqModel, summary);
            } else {
                emit failed(QStringLiteral("Failed to load model: %1")
                                .arg(QString::fromUtf8(m_engine->lastError())));
            }
        }

        if (frame.empty() || m_engine->currentModel() < 0) continue;

        cv::Mat in;
        QString why;
        if (!prepare(frame, in, why)) {
            if (why != m_lastWhy) {      // report a given size problem once
                m_lastWhy = why;
                emit failed(why);
            }
            continue;
        }
        m_lastWhy.clear();

        cv::Mat out;
        if (!m_engine->process(in, out)) {
            emit failed(QString::fromUtf8(m_engine->lastError()));
            continue;
        }
        emit resultReady(in.clone(), out, m_engine->lastProcessMs());
    }
}
