/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : VideoRecorder.h
* Description : Records the frames the user is watching into an .mp4.
*
* Interface follows IrMp4Rec (early004ArithCollection) - open / writeFrame /
* close - but the backend is OpenCV's VideoWriter, which this app already links
* (cv::VideoCapture is used for playback). That buys colour recording for free,
* so the Compare view's side-by-side image records as-is; IrMp4Rec's encoder is
* grayscale-only and could not.
*
* The geometry is fixed by the first frame: whatever the current view produces
* (single grayscale/colour panel, or the stitched Before|After pair) sets the
* recording size. Recording is view-driven - starting in Compare records the
* comparison, After records the result, Before records the input.
******************************************************************************/
#ifndef ___VideoRecorder_h___
#define ___VideoRecorder_h___

#include <QString>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class VideoRecorder
{
public:
    // Playback: inter-frame H.264 (small files) for demo clips of the view.
    // Archive : intra-frame MJPEG (every frame kept independently, far less
    //           compression) for the original source, to minimise picture loss.
    // Both write a standard .mp4 and fall back to MPEG-4 if the first codec of
    // the profile is unavailable on the machine.
    enum Profile { Playback, Archive };

    VideoRecorder() = default;
    ~VideoRecorder() { close(); }

    /* Begin an .mp4. Size and channel count are taken from the first frame, so
     * pass a representative one. fps <= 0 falls back to 25. */
    bool open(const QString &path, const cv::Mat &firstFrame, double fps,
              Profile profile = Playback);

    /* Append one frame. It must match the size/channels of the first frame;
     * mismatches are resized/converted so a mid-recording view change cannot
     * corrupt the file. */
    bool writeFrame(const cv::Mat &frame);

    void close();
    bool    isOpen()     const { return m_writer.isOpened(); }
    long    frameCount() const { return m_frames; }
    QString path()       const { return m_path; }
    QString lastError()  const { return m_err; }

private:
    cv::VideoWriter m_writer;
    cv::Size        m_size;
    int             m_channels = 0;
    long            m_frames = 0;
    QString         m_path, m_err;
};

#endif
