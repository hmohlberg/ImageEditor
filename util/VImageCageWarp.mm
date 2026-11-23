/*
* Copyright 2026 Forschungszentrum Jülich
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    https://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "VImageCageWarp.h"

#ifdef __APPLE__

#include "../layer/CageMesh.h"

#include <Accelerate/Accelerate.h>
#include <dispatch/dispatch.h>
#include <cmath>
#include <vector>

#include <QImage>
#include <QPolygonF>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

namespace VImageCageWarp {

static vImage_PerpsectiveTransform buildTransform(const QTransform& forward,
                                                 int sourceHeight, int ox, int oy, int height)
{
    // Qt uses the top-left origin. vImage uses the bottom-left origin of each buffer.
    const QTransform toQtSource(1, 0, 0, -1, 0, sourceHeight);
    const QTransform toVImageTile(1, 0, 0, -1, -ox, oy + height);
    const QTransform transform = toQtSource * forward * toVImageTile;
    return {
        float(transform.m11()), float(transform.m12()),
        float(transform.m21()), float(transform.m22()),
        float(transform.m31()), float(transform.m32()),
        float(transform.m13()), float(transform.m23()), float(transform.m33())
    };
}

struct QuadResult {
    QImage    image;
    QPolygonF clipPoly;
    int       ox = 0;
    int       oy = 0;
};

static QImage warpByCageVImage(const QImage& src, const CageMesh& cageMesh)
    API_AVAILABLE(macos(13.0));

static QImage warpByCageVImage(const QImage& src, const CageMesh& cageMesh)
{
    const int cols = cageMesh.cols();
    const int rows = cageMesh.rows();
    if (cols < 2 || rows < 2) return QImage();

    const QVector<QPointF>& pts    = cageMesh.points();
    const QRectF             dstBounds = QPolygonF(pts).boundingRect();
    const qreal              srcW  = src.width();
    const qreal              srcH  = src.height();
    const int                nQ    = (rows - 1) * (cols - 1);

    const vImage_Buffer srcVBuf = {
        const_cast<void*>(static_cast<const void*>(src.constBits())),
        static_cast<vImagePixelCount>(src.height()),
        static_cast<vImagePixelCount>(src.width()),
        static_cast<size_t>(src.bytesPerLine())
    };

    std::vector<QuadResult> qr(nQ);
    // Capture a raw pointer so dispatch_apply blocks can write to it without
    // triggering ObjC block copy semantics on the std::vector itself.
    QuadResult* qrPtr = qr.data();

    dispatch_apply(nQ,
        dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
        ^(size_t idx)
    {
        const int r = (int)(idx / (cols - 1));
        const int c = (int)(idx % (cols - 1));

        const QPolygonF srcQuad({
            QPointF( c      * srcW / (cols - 1),  r      * srcH / (rows - 1)),
            QPointF((c + 1) * srcW / (cols - 1),  r      * srcH / (rows - 1)),
            QPointF((c + 1) * srcW / (cols - 1), (r + 1) * srcH / (rows - 1)),
            QPointF( c      * srcW / (cols - 1), (r + 1) * srcH / (rows - 1))
        });
        const QPolygonF dstQuad({
            pts[ r      * cols + c    ] - dstBounds.topLeft(),
            pts[ r      * cols + c + 1] - dstBounds.topLeft(),
            pts[(r + 1) * cols + c + 1] - dstBounds.topLeft(),
            pts[(r + 1) * cols + c    ] - dstBounds.topLeft()
        });

        QTransform tFwd;
        if (!QTransform::quadToQuad(srcQuad, dstQuad, tFwd)) return;

        const QRectF qb = dstQuad.boundingRect();
        const int ox = qMax(0, (int)std::floor(qb.left()   - 1.0));
        const int oy = qMax(0, (int)std::floor(qb.top()    - 1.0));
        const int ex = qMin((int)srcW, (int)std::ceil(qb.right()  + 1.0));
        const int ey = qMin((int)srcH, (int)std::ceil(qb.bottom() + 1.0));
        const int bw = ex - ox;
        const int bh = ey - oy;
        if (bw <= 0 || bh <= 0) return;

        const vImage_PerpsectiveTransform vpt = buildTransform(tFwd, src.height(), ox, oy, bh);

        QImage temp(bw, bh, QImage::Format_ARGB32);
        temp.fill(Qt::transparent);

        vImage_Buffer dstVBuf = {
            temp.bits(),
            static_cast<vImagePixelCount>(bh),
            static_cast<vImagePixelCount>(bw),
            static_cast<size_t>(temp.bytesPerLine())
        };

        Pixel_8888 backColor = { 0, 0, 0, 0 };
        const vImage_Error err = vImagePerspectiveWarp_ARGB8888(
            &srcVBuf, &dstVBuf,
            nullptr,
            &vpt,
            kvImageInterpolationLinear,
            backColor,
            kvImageBackgroundColorFill);
        if (err != kvImageNoError) return;

        // Expand clip polygon 0.75 px outward to close seams between quads.
        QPointF centroid;
        for (const QPointF& p : dstQuad) centroid += p;
        centroid /= (qreal)dstQuad.size();
        QPolygonF expanded;
        expanded.reserve(dstQuad.size());
        for (const QPointF& p : dstQuad) {
            const QPointF dd  = p - centroid;
            const qreal   len = std::hypot(dd.x(), dd.y());
            expanded << (len > 0.0 ? centroid + dd * ((len + 0.75) / len) : p);
        }

        qrPtr[idx] = { std::move(temp), std::move(expanded), ox, oy };
    });

    // Composite quads sequentially with QPainter clip paths.
    QImage result(src.size(), QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, false);

    for (int i = 0; i < nQ; ++i) {
        const QuadResult& q = qr[i];
        if (q.image.isNull() || q.clipPoly.isEmpty()) continue;
        QPainterPath clip;
        clip.addPolygon(q.clipPoly);
        painter.save();
        painter.setClipPath(clip);
        painter.drawImage(QPointF(q.ox, q.oy), q.image);
        painter.restore();
    }
    painter.end();
    return result;
}

QImage warpByCage(const QImage& srcIn, const CageMesh& cageMesh)
{
    if (@available(macOS 13.0, *)) {
        const QImage src = (srcIn.format() == QImage::Format_ARGB32)
                           ? srcIn : srcIn.convertToFormat(QImage::Format_ARGB32);
        return warpByCageVImage(src, cageMesh);
    }
    return QImage();
}

} // namespace VImageCageWarp
#endif // __APPLE__
