#include "LaMaInpainting.h"

#include <QFileInfo>
#include <QRect>
#include <QStandardPaths>
#include "../core/Config.h"

#ifdef HASLAMA
#include <onnxruntime_cxx_api.h>
#include <algorithm>
#include <vector>
#endif

QString LaMaInpainting::defaultModelPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/lama.onnx";
}

bool LaMaInpainting::isAvailable()
{
#ifdef HASLAMA
    const QString configured = EditorStyle::instance().lamaModelPath();
    const QString path = configured.isEmpty() ? defaultModelPath() : configured;
    return QFileInfo::exists(path);
#else
    return false;
#endif
}

#ifdef HASLAMA

static auto fail(QString* errOut, const QString& msg) -> QImage
{
    if ( errOut ) *errOut = msg;
    return QImage();
}

// Hole bounding box (pixels where mask > 0), returns invalid QRect if none.
static QRect holeBounds(const QImage& mask8)
{
    int x0 = mask8.width(), y0 = mask8.height(), x1 = -1, y1 = -1;
    for ( int y = 0; y < mask8.height(); ++y ) {
        const uchar* row = mask8.constScanLine(y);
        for ( int x = 0; x < mask8.width(); ++x ) {
            if ( row[x] ) {
                x0 = std::min(x0, x); x1 = std::max(x1, x);
                y0 = std::min(y0, y); y1 = std::max(y1, y);
            }
        }
    }
    return x1 >= 0 ? QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1) : QRect();
}

// Pack QImage crop (Format_RGB32) + mask (Format_Grayscale8) into NCHW float tensors.
static void toNchw( const QImage& img, const QImage& msk,
                    int W, int H,
                    std::vector<float>& imgOut, std::vector<float>& mskOut )
{
    imgOut.assign(3 * H * W, 0.f);
    mskOut.assign(    H * W, 0.f);
    for ( int y = 0; y < img.height() && y < H; ++y ) {
        const QRgb*  ri = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        const uchar* rm = msk.constScanLine(y);
        for ( int x = 0; x < img.width() && x < W; ++x ) {
            imgOut[0*H*W + y*W + x] = float(qRed  (ri[x])) / 255.f;
            imgOut[1*H*W + y*W + x] = float(qGreen(ri[x])) / 255.f;
            imgOut[2*H*W + y*W + x] = float(qBlue (ri[x])) / 255.f;
            mskOut[       y*W + x] = rm[x] > 0 ? 1.f : 0.f;
        }
    }
}

QImage LaMaInpainting::run( const QImage& src,
                             const QImage& holeMask,
                             const QString& modelPath,
                             QString* errorMsg )
{
    const QString mpath = modelPath.isEmpty() ? defaultModelPath() : modelPath;
    if ( !QFileInfo::exists(mpath) )
        return fail(errorMsg, QString("Model not found: %1\n"
                                     "Place lama.onnx at that path.").arg(mpath));

    QImage msk8 = holeMask.convertToFormat(QImage::Format_Grayscale8);
    QRect  hole = holeBounds(msk8);
    if ( !hole.isValid() )
        return fail(errorMsg, "Hole mask is empty.");

    // ── 1. Create ONNX session and query expected input size ─────────────────
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "lama");
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(4);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        Ort::Session session(env, mpath.toUtf8().constData(), opts);
        Ort::AllocatorWithDefaultOptions allocator;

        const size_t nIn  = session.GetInputCount();
        const size_t nOut = session.GetOutputCount();
        if ( nIn < 1 || nOut < 1 )
            return fail(errorMsg, "Unexpected model I/O count");

        // Input/output names
        std::vector<std::string> inNames, outNames;
        for ( size_t i = 0; i < nIn;  ++i )
            inNames .push_back(session.GetInputNameAllocated(i, allocator).get());
        for ( size_t i = 0; i < nOut; ++i )
            outNames.push_back(session.GetOutputNameAllocated(i, allocator).get());

        std::vector<const char*> inPtrs, outPtrs;
        for ( auto& s : inNames  ) inPtrs .push_back(s.c_str());
        for ( auto& s : outNames ) outPtrs.push_back(s.c_str());

        // Check whether model has fixed spatial dims (e.g. 1024×1024)
        auto shapeInfo = session.GetInputTypeInfo(0)
                                .GetTensorTypeAndShapeInfo().GetShape();
        const int64_t fixH = (shapeInfo.size() >= 4 && shapeInfo[2] > 0) ? shapeInfo[2] : 0;
        const int64_t fixW = (shapeInfo.size() >= 4 && shapeInfo[3] > 0) ? shapeInfo[3] : 0;

        // ── 2. Prepare crop region ────────────────────────────────────────────
        const int imgW = src.width(), imgH = src.height();
        QImage rgb = src.convertToFormat(QImage::Format_RGB32);

        QRect ctx;
        int W, H;
        bool didScale = false;

        if ( fixW > 0 && fixH > 0 ) {
            W = (int)fixW;  H = (int)fixH;

            if ( hole.width() <= W && hole.height() <= H ) {
                // Hole fits: center a native-resolution W×H crop — no scaling, sharp result
                int cx = hole.center().x(), cy = hole.center().y();
                int x0 = std::clamp(cx - W/2, 0, std::max(0, imgW - W));
                int y0 = std::clamp(cy - H/2, 0, std::max(0, imgH - H));
                ctx = QRect(x0, y0, std::min(W, imgW - x0), std::min(H, imgH - y0));
            } else {
                // Hole larger than model dims → scale the tight hole region
                const int margin = 32;
                ctx = QRect(
                    std::max(0,        hole.left()   - margin),
                    std::max(0,        hole.top()    - margin),
                    std::min(imgW - 1, hole.right()  + margin),
                    std::min(imgH - 1, hole.bottom() + margin)
                ).normalized();
                didScale = true;
            }
        } else {
            // Dynamic dims: tight crop + context, pad to multiple of 32
            const int margin = std::max(64, std::max(hole.width(), hole.height()) / 4);
            ctx = QRect(
                std::max(0,        hole.left()   - margin),
                std::max(0,        hole.top()    - margin),
                std::min(imgW - 1, hole.right()  + margin),
                std::min(imgH - 1, hole.bottom() + margin)
            ).normalized();
            W = ((ctx.width()  + 31) / 32) * 32;
            H = ((ctx.height() + 31) / 32) * 32;
        }

        QImage crop  = rgb .copy(ctx);
        QImage mcrop = msk8.copy(ctx);

        if ( didScale ) {
            crop  = crop .scaled(W, H, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            mcrop = mcrop.scaled(W, H, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        }

        // ── 3. Build float tensors ────────────────────────────────────────────
        std::vector<float> imgData, maskData;
        toNchw(crop, mcrop, W, H, imgData, maskData);

        auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::array<int64_t,4> imgShape  { 1, 3, H, W };
        std::array<int64_t,4> maskShape { 1, 1, H, W };

        std::vector<Ort::Value> inputs;
        inputs.push_back(Ort::Value::CreateTensor<float>(
            memInfo, imgData.data(),  imgData.size(),  imgShape.data(),  4));
        // Some models use only 1 input (image+mask stacked); try with 2 first.
        if ( nIn >= 2 )
            inputs.push_back(Ort::Value::CreateTensor<float>(
                memInfo, maskData.data(), maskData.size(), maskShape.data(), 4));

        // ── 4. Run inference ──────────────────────────────────────────────────
        auto outputs = session.Run(
            Ort::RunOptions{},
            inPtrs.data(), inputs.data(), inputs.size(),
            outPtrs.data(), outPtrs.size());

        // ── 5. Decode output → QImage ─────────────────────────────────────────
        const float* out = outputs[0].GetTensorData<float>();

        // Determine output layout (NCHW vs NHWC) from actual tensor shape
        auto outShape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
        // outShape: {1,3,H,W} = NCHW  or  {1,H,W,3} = NHWC
        const bool nhwc = (outShape.size() == 4 && outShape[3] == 3);
        const int oH = nhwc ? (int)outShape[1] : (int)outShape[2];
        const int oW = nhwc ? (int)outShape[2] : (int)outShape[3];

        // Auto-detect value range: some models output [0,255], others [0,1]
        float maxVal = 0.f;
        for ( int i = 0, n = 3 * oH * oW; i < n; ++i )
            maxVal = std::max(maxVal, out[i]);
        const float scale = maxVal > 1.5f ? (1.f / 255.f) : 1.f;

        QImage inpCrop(oW, oH, QImage::Format_RGB32);
        for ( int y = 0; y < oH; ++y ) {
            QRgb* dstRow = reinterpret_cast<QRgb*>(inpCrop.scanLine(y));
            for ( int x = 0; x < oW; ++x ) {
                int r, g, b;
                if ( nhwc ) {
                    const float* px = out + (y * oW + x) * 3;
                    r = std::clamp(int(px[0] * scale * 255.f + .5f), 0, 255);
                    g = std::clamp(int(px[1] * scale * 255.f + .5f), 0, 255);
                    b = std::clamp(int(px[2] * scale * 255.f + .5f), 0, 255);
                } else {
                    r = std::clamp(int(out[0*oH*oW + y*oW + x] * scale * 255.f + .5f), 0, 255);
                    g = std::clamp(int(out[1*oH*oW + y*oW + x] * scale * 255.f + .5f), 0, 255);
                    b = std::clamp(int(out[2*oH*oW + y*oW + x] * scale * 255.f + .5f), 0, 255);
                }
                dstRow[x] = qRgb(r, g, b);
            }
        }
        // Scale result back to ctx size when we resized for a fixed-dim model
        if ( oW != ctx.width() || oH != ctx.height() )
            inpCrop = inpCrop.scaled(ctx.width(), ctx.height(),
                                     Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        // ── 6. Paste only hole pixels back into a full copy of src ───────────
        QImage result = rgb.copy();
        for ( int y = 0; y < ctx.height(); ++y ) {
            const uchar* mrow = msk8.constScanLine(ctx.top() + y) + ctx.left();
            const QRgb*  srow = reinterpret_cast<const QRgb*>(inpCrop.constScanLine(y));
            QRgb*        drow = reinterpret_cast<QRgb*>(result.scanLine(ctx.top() + y))
                                + ctx.left();
            for ( int x = 0; x < ctx.width(); ++x )
                if ( mrow[x] ) drow[x] = srow[x];
        }
        return result;

    } catch ( const Ort::Exception& ex ) {
        return fail(errorMsg, QString("ONNX Runtime error: %1").arg(ex.what()));
    } catch ( const std::exception& ex ) {
        return fail(errorMsg, QString("Exception: %1").arg(ex.what()));
    }
}

#else

QImage LaMaInpainting::run( const QImage&, const QImage&,
                             const QString&, QString* errorMsg )
{
    if ( errorMsg )
        *errorMsg = "LaMa support not compiled in (onnxruntime not found at build time).";
    return QImage();
}

#endif
