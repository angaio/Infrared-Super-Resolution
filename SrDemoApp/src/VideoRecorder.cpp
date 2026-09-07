/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "VideoRecorder.h"

#include <opencv2/imgproc.hpp>

bool VideoRecorder::open(const QString &path, const cv::Mat &firstFrame, double fps,
                         Profile profile)
{
    close();
    m_err.clear();
    if (firstFrame.empty()) { m_err = QStringLiteral("no frame to size the recording"); return false; }
    if (fps < 1.0 || fps > 240.0) fps = 25.0;

    // Even dimensions keep H.264 / YUV420 happy; MJPEG accepts any size.
    m_size = cv::Size(firstFrame.cols & ~1, firstFrame.rows & ~1);
    m_channels = firstFrame.channels();
    const bool colour = (m_channels == 3);

    // Codec preference by profile. The last entry (MPEG-4) is the fallback the
    // FFmpeg backend can always open, so a recording is never lost.
    const int playback[] = {
        cv::VideoWriter::fourcc('a', 'v', 'c', '1'),   // H.264, small files
        cv::VideoWriter::fourcc('H', '2', '6', '4'),
        cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
    };
    const int archive[] = {
        cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),   // intra-frame, low loss
        cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
    };
    const int *list = (profile == Archive) ? archive : playback;
    const int count = (profile == Archive) ? int(sizeof(archive) / sizeof(int))
                                           : int(sizeof(playback) / sizeof(int));
    for (int i = 0; i < count; ++i) {
        if (m_writer.open(path.toLocal8Bit().constData(), list[i], fps, m_size, colour))
            break;
    }
    if (!m_writer.isOpened()) {
        m_err = QStringLiteral("cannot open the encoder for %1").arg(path);
        return false;
    }
    // Ask for the least compression the codec allows (best effort - honoured
    // only on some backends; MJPEG is already intra-frame low-loss regardless).
    if (profile == Archive)
        m_writer.set(cv::VIDEOWRITER_PROP_QUALITY, 100.0);
    m_path = path;
    m_frames = 0;
    return true;
}

bool VideoRecorder::writeFrame(const cv::Mat &frame)
{
    if (!m_writer.isOpened() || frame.empty()) return false;

    cv::Mat f = frame;
    // Guard against a mid-recording view/size change: coerce to the geometry the
    // file was opened with rather than corrupting the stream.
    if (f.channels() != m_channels) {
        if (m_channels == 3 && f.channels() == 1) cv::cvtColor(f, f, cv::COLOR_GRAY2BGR);
        else if (m_channels == 1 && f.channels() == 3) cv::cvtColor(f, f, cv::COLOR_BGR2GRAY);
    }
    if (f.size() != m_size)
        cv::resize(f, f, m_size, 0, 0, cv::INTER_AREA);

    m_writer.write(f);
    ++m_frames;
    return true;
}

void VideoRecorder::close()
{
    if (m_writer.isOpened())
        m_writer.release();
    m_size = cv::Size();
    m_channels = 0;
}
