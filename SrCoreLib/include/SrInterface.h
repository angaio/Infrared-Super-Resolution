/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : SrInterface.h
* Description : The only public interface of SrCoreLib.
*
* SrCoreLib upscales a fixed-size single-channel infrared frame by an integer
* factor. Everything about how that is done stays inside the library. A caller
* only ever sees the four verbs below.
*
* Usage (three steps):
*   1) auto sr = SrEngineFactory::createEngine();
*   2) sr->selectModel(0, 8);            // once, or whenever the user switches
*   3) sr->process(src, dst);            // per frame
*
* Input contract:
*   src must be CV_8UC1 or CV_8UC3 and exactly inputWidth() x inputHeight().
*   dst comes back with the same channel count, scaled by scale().
*
* Threading:
*   One instance is NOT re-entrant; it reuses internal buffers. Use one
*   instance per thread.
******************************************************************************/
#ifndef ___SrInterface_h___
#define ___SrInterface_h___

#include <memory>
#include <opencv2/core.hpp>

#ifdef _WIN32
#  ifdef __DLL_EXPORTS__
#    define SRAPI __declspec(dllexport)
#  else
#    define SRAPI __declspec(dllimport)
#  endif
#else
#  define SRAPI __attribute__((visibility("default")))
#endif

class SRAPI SrEngine
{
public:
    virtual ~SrEngine() {}

    /* ---- model selection ------------------------------------------------ */

    /* Number of models shipped inside the library. */
    virtual int modelCount() const = 0;

    /* Display name of a model, e.g. "Enhanced". Never a file name. */
    virtual const char *modelName(int index) const = 0;

    /* One-line hint suitable for a tooltip. */
    virtual const char *modelHint(int index) const = 0;

    /* Load a model. threads <= 0 lets the runtime decide.
     * Costs a few hundred milliseconds; call it off the render thread.
     * Returns false on failure, see lastError(). */
    virtual bool selectModel(int index, int threads) = 0;

    /* Index of the loaded model, or -1 when nothing is loaded yet. */
    virtual int currentModel() const = 0;

    /* ---- geometry ------------------------------------------------------- */

    virtual int inputWidth() const = 0;
    virtual int inputHeight() const = 0;
    virtual int scale() const = 0;

    /* ---- processing ----------------------------------------------------- */

    /* Upscale one frame. src: CV_8UC1/CV_8UC3 at inputWidth() x inputHeight().
     * dst: same channel count, size multiplied by scale(). */
    virtual bool process(const cv::Mat &src, cv::Mat &dst) = 0;

    /* Wall time of the last process() call, in milliseconds. */
    virtual double lastProcessMs() const = 0;

    /* Empty when the last call succeeded. */
    virtual const char *lastError() const = 0;
};

class SRAPI SrEngineFactory
{
public:
    /* Creates an engine. Returns nullptr only when the process is out of
     * memory; a missing model is reported later by selectModel(). */
    static std::shared_ptr<SrEngine> createEngine();

    /* Library version string, for the About box and bug reports. */
    static const char *version();
};

#endif
