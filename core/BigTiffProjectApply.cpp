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

#include "BigTiffProjectApply.h"

#include <tiffio.h>

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QTransform>
#include <QVector>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <utility>
#include <initializer_list>

#ifdef HASLAMA
#include "Config.h"
#include "../util/LaMaInpainting.h"
#endif

// ── TIFF pyramid level ────────────────────────────────────────────────────────

struct ProjLevel {
    uint32_t w = 0, h = 0;
    uint32_t tileW = 0, tileH = 0;
    int      dirIdx = 0;
    bool     tiled  = false;
};

static QVector<ProjLevel> scanProjLevels(TIFF* tif)
{
    QVector<ProjLevel> raw;
    do {
        ProjLevel lvl;
        lvl.dirIdx = (int)TIFFCurrentDirectory(tif);
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH,  &lvl.w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &lvl.h);
        lvl.tiled = TIFFIsTiled(tif) != 0;
        if (lvl.tiled) {
            TIFFGetField(tif, TIFFTAG_TILEWIDTH,  &lvl.tileW);
            TIFFGetField(tif, TIFFTAG_TILELENGTH, &lvl.tileH);
        } else {
            lvl.tileW = lvl.w;
            lvl.tileH = lvl.h;
        }
        if (lvl.w > 0 && lvl.h > 0) raw.append(lvl);
    } while (TIFFReadDirectory(tif));
    std::sort(raw.begin(), raw.end(),
              [](const ProjLevel& a, const ProjLevel& b){ return a.w > b.w; });
    return raw;
}

// ── operation type ────────────────────────────────────────────────────────────

enum class ProjOpType { Move, Transform, Perspective, CageWarp };

// Describes one LassoCut + subsequent layer operation.
//
// The forward mapping (20-µm canvas space) is:
//   MoveLayer:     canvas = dstPos + mirror(lx, ly)
//   TransformLayer: canvas = itemPos + QTransform(m11..m32).map(mirror(lx, ly))
//
// mirror() is applied before the position/transform when mirrorPlane != 0:
//   plane=1 (vertical):   ly  →  srcH - 1 - ly
//   plane=2 (horizontal): lx  →  srcW - 1 - lx
//
// Inverse: flip is self-inverse, so the same formula undoes it.
struct PasteOp {
    QImage      mask;        // binary mask at 20-µm, Format_Grayscale8
    QRect       srcRect20;   // source bounding box at 20-µm canvas coords

    ProjOpType  type = ProjOpType::Move;
    int         mirrorPlane = 0;  // 0=none, 1=vertical flip, 2=horizontal flip

    // ── MoveLayer ────────────────────────────────────────────────────────────
    QPoint      dstPos20;    // canvas position of local origin after move

    // ── TransformLayer (affine QTransform from JSON) ──────────────────────
    // Qt convention: (x',y') = (m11*x + m21*y + m31, m12*x + m22*y + m32)
    double m11=1, m12=0, m21=0, m22=1, m31=0, m32=0;
    double det = 1;          // m11*m22 - m12*m21, precomputed
    QPoint itemPos20;        // layer pos() in canvas (newPosition from JSON)
    QRectF dstBBox20;        // bounding box of transformed region at 20 µm

    // ── CageWarp (mesh deformation in 20-µm pixel space) ─────────────────
    QPointF          cwTopLeft20;    // canvas position of warped layer top-left (20 µm)
    QVector<QPointF> cwBefore20;     // cagepoints_before in layer-local 20-µm pixels
    QVector<QPointF> cwAfter20;      // cagepoints_after  in layer-local 20-µm pixels
    int cwRows = 0, cwCols = 0;

    // ── PerspectiveWarp (projective homography in 20-µm pixel space) ──────
    // Forward H maps source-local (lx,ly) → warped-local (wx,wy):
    //   wx = (pw_m11*lx + pw_m21*ly + pw_m31) / (pw_m13*lx + pw_m23*ly + pw_m33)
    //   wy = (pw_m12*lx + pw_m22*ly + pw_m32) / (pw_m13*lx + pw_m23*ly + pw_m33)
    QPointF pwNewPos20;                          // canvas top-left of warped image (20 µm)
    double pw_m11=1,pw_m12=0,pw_m13=0;
    double pw_m21=0,pw_m22=1,pw_m23=0;
    double pw_m31=0,pw_m32=0,pw_m33=1;

    // ── pre-affine (absorbed TransformLayer preceding CageWarp) ──────────
    // Forward chain when hasPreAffine: source → mirror → affine → cage → canvas
    // The affine is the totalTransform from the TransformLayer command.
    // Qt convention: display_x = m11*ox + m21*oy + m31, display_y = m12*ox + m22*oy + m32
    bool   hasPreAffine = false;
    double pa_m11=1, pa_m12=0, pa_m21=0, pa_m22=1, pa_m31=0, pa_m32=0, pa_det=1;
};

// ── inpainting patch types ────────────────────────────────────────────────────

struct InpaintPatch {
    QRect  boundsL0;   // bounds at finest BigTIFF resolution (level 0 pixels)
    QImage resultRGB;  // Format_RGB32 inpainted result at boundsL0 size
    QImage holeMask;   // Format_Grayscale8 mask (255 = inpainted pixel) at boundsL0 size
};

struct LevelInpaintPatch {
    QRect  bounds;     // bounds scaled to current pyramid level
    QImage result;     // Format_RGB32 scaled to bounds.size()
    QImage mask;       // Format_Grayscale8 scaled to bounds.size()
};

// ── parsing ───────────────────────────────────────────────────────────────────

static QVector<PasteOp> parseOps(const QJsonObject& project)
{
    struct LayerInfo { QImage mask; };
    QHash<int, LayerInfo> layerMasks;
    QHash<int, QRect>     cutRects;       // newLayerId → bounding rect from LassoCut
    QHash<int, int>       layerMirrors;   // newLayerId → net mirrorPlane (0=none)
    QHash<int, QJsonObject> pendingTransforms; // newLayerId → last TransformLayer cmd
    QVector<PasteOp>      ops;

    // Collect binary-masked layers
    for (const QJsonValue& lv : project["layers"].toArray()) {
        QJsonObject layer = lv.toObject();
        if (!layer["binaryMask"].toBool(false)) continue;
        int id = layer["id"].toInt(-1);
        if (id < 0) continue;

        QByteArray raw = QByteArray::fromBase64(
            layer["data"].toString().toLatin1());
        QImage img;
        if (!img.loadFromData(raw, "PNG")) continue;
        if (img.format() != QImage::Format_Grayscale8)
            img = img.convertToFormat(QImage::Format_Grayscale8);

        layerMasks[id] = { img };
    }

    // Walk undoStack and collect LassoCutCommand → operation pairs
    for (const QJsonValue& uv : project["undoStack"].toArray()) {
        QJsonObject cmd = uv.toObject();
        const QString type = cmd["type"].toString();

        if (type == "LassoCutCommand") {
            int newId = cmd["newLayerId"].toInt(-1);
            if (newId < 0) continue;
            QJsonObject r = cmd["rect"].toObject();
            cutRects[newId] = QRect(r["x"].toInt(), r["y"].toInt(),
                                    r["width"].toInt(), r["height"].toInt());
        }
        else if (type == "MirrorLayer") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId)) continue;
            int plane = cmd["mirrorPlane"].toInt(0);
            if (plane > 0) {
                // Toggle: two flips on the same plane cancel each other
                layerMirrors[layerId] = (layerMirrors.value(layerId, 0) == plane) ? 0 : plane;
            }
            // plane <= 0 means the undo of the mirror → clear it
            else {
                layerMirrors.remove(layerId);
            }
        }
        else if (type == "MoveLayer") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;
            pendingTransforms.remove(layerId);  // move finalises position; pre-affine no longer composable

            PasteOp op;
            op.mask        = layerMasks[layerId].mask;
            op.srcRect20   = cutRects[layerId];
            op.type        = ProjOpType::Move;
            op.mirrorPlane = layerMirrors.value(layerId, 0);
            op.dstPos20    = QPoint(cmd["toX"].toInt(), cmd["toY"].toInt());

            // Bounding box = destination rect
            op.dstBBox20 = QRectF(op.dstPos20.x(), op.dstPos20.y(),
                                  op.srcRect20.width(), op.srcRect20.height());
            ops.append(op);
        }
        else if (type == "TransformLayer") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;
            // Store as pending; create the PasteOp only if no CageWarp/Perspective follows,
            // or absorb it as pre-affine into the subsequent cage/perspective.
            pendingTransforms[layerId] = cmd;
        }
        else if (type == "CageWarp") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;

            const int rows = cmd["rows"].toInt(0);
            const int cols = cmd["columns"].toInt(0);
            if (rows < 2 || cols < 2) continue;

            QVector<QPointF> before, after;
            for (const QJsonValue& v : cmd["cagepoints_before"].toArray()) {
                QJsonObject po = v.toObject();
                before << QPointF(po["x"].toDouble(), po["y"].toDouble());
            }
            for (const QJsonValue& v : cmd["cagepoints_after"].toArray()) {
                QJsonObject po = v.toObject();
                after << QPointF(po["x"].toDouble(), po["y"].toDouble());
            }
            if (before.size() != rows * cols || after.size() != rows * cols) continue;

            QJsonObject tlObj = cmd["topLeft_after"].toObject();

            // Destination bounding box: bounding rect of all after points + topLeft
            QRectF afterBounds = QPolygonF(after).boundingRect();

            PasteOp op;
            op.mask        = layerMasks[layerId].mask;
            op.srcRect20   = cutRects[layerId];
            op.type        = ProjOpType::CageWarp;
            op.mirrorPlane = layerMirrors.value(layerId, 0);
            op.cwTopLeft20 = QPointF(tlObj["x"].toDouble(), tlObj["y"].toDouble());
            op.cwBefore20  = before;
            op.cwAfter20   = after;
            op.cwRows      = rows;
            op.cwCols      = cols;
            op.dstBBox20   = QRectF(op.cwTopLeft20, QSizeF(afterBounds.right(), afterBounds.bottom()));

            // Absorb a preceding TransformLayer as pre-affine (source→mirror→affine→cage chain)
            if (pendingTransforms.contains(layerId)) {
                const QJsonObject& tc = pendingTransforms[layerId];
                QJsonObject T = tc["newTransform"].toObject();
                op.hasPreAffine = true;
                op.pa_m11 = T["m11"].toDouble(1); op.pa_m12 = T["m12"].toDouble(0);
                op.pa_m21 = T["m21"].toDouble(0); op.pa_m22 = T["m22"].toDouble(1);
                op.pa_m31 = T["m31"].toDouble(0); op.pa_m32 = T["m32"].toDouble(0);
                op.pa_det = op.pa_m11 * op.pa_m22 - op.pa_m12 * op.pa_m21;
                pendingTransforms.remove(layerId);
            }
            ops.append(op);
        }
        else if (type == "PerspectiveWarp") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;
            pendingTransforms.remove(layerId);  // perspective replaces image; pre-affine not composable yet

            QVector<QPointF> before, after;
            for (const QJsonValue& v : cmd["before"].toArray()) {
                QJsonArray a = v.toArray();
                before << QPointF(a[0].toDouble(), a[1].toDouble());
            }
            for (const QJsonValue& v : cmd["after"].toArray()) {
                QJsonArray a = v.toArray();
                after << QPointF(a[0].toDouble(), a[1].toDouble());
            }
            if (before.size() != 4 || after.size() != 4) continue;

            // Reconstruct the warpTransform as in PerspectiveWarpCommand::rebuildWarp()
            QRectF targetBounds = QPolygonF(after).boundingRect();
            QVector<QPointF> shiftedAfter;
            for (const QPointF& p : after) shiftedAfter << (p - targetBounds.topLeft());

            QTransform H;
            if (!QTransform::quadToQuad(QPolygonF(before), QPolygonF(shiftedAfter), H)) continue;

            QJsonObject np = cmd["newPosition"].toObject();

            PasteOp op;
            op.mask        = layerMasks[layerId].mask;
            op.srcRect20   = cutRects[layerId];
            op.type        = ProjOpType::Perspective;
            op.mirrorPlane = layerMirrors.value(layerId, 0);
            op.pwNewPos20  = QPointF(np["x"].toDouble(), np["y"].toDouble());
            op.pw_m11 = H.m11(); op.pw_m12 = H.m12(); op.pw_m13 = H.m13();
            op.pw_m21 = H.m21(); op.pw_m22 = H.m22(); op.pw_m23 = H.m23();
            op.pw_m31 = H.dx();  op.pw_m32 = H.dy();  op.pw_m33 = H.m33();

            // Destination bounding box: warped image rectangle placed at newPos
            op.dstBBox20 = QRectF(op.pwNewPos20, QSizeF(targetBounds.width(), targetBounds.height()));
            ops.append(op);
        }
    }

    // Flush TransformLayer ops that were not consumed by a subsequent CageWarp/PerspectiveWarp
    for (auto it = pendingTransforms.begin(); it != pendingTransforms.end(); ++it) {
        int layerId = it.key();
        const QJsonObject& tc = it.value();
        if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;

        QJsonObject T = tc["newTransform"].toObject();
        PasteOp op;
        op.mask        = layerMasks[layerId].mask;
        op.srcRect20   = cutRects[layerId];
        op.type        = ProjOpType::Transform;
        op.mirrorPlane = layerMirrors.value(layerId, 0);
        op.m11 = T["m11"].toDouble(1); op.m12 = T["m12"].toDouble(0);
        op.m21 = T["m21"].toDouble(0); op.m22 = T["m22"].toDouble(1);
        op.m31 = T["m31"].toDouble(0); op.m32 = T["m32"].toDouble(0);
        op.det = op.m11 * op.m22 - op.m12 * op.m21;

        QJsonObject np = tc["newPosition"].toObject();
        op.itemPos20 = QPoint(np["x"].toInt(), np["y"].toInt());

        double iw = op.itemPos20.x(), ih = op.itemPos20.y();
        double w  = op.srcRect20.width(), h = op.srcRect20.height();
        auto mapCorner = [&](double lx, double ly) -> QPointF {
            return { iw + op.m11*lx + op.m21*ly + op.m31,
                     ih + op.m12*lx + op.m22*ly + op.m32 };
        };
        QPointF c[4] = { mapCorner(0,0), mapCorner(w,0),
                         mapCorner(0,h), mapCorner(w,h) };
        double bx0 = c[0].x(), bx1 = c[0].x(),
               by0 = c[0].y(), by1 = c[0].y();
        for (int k = 1; k < 4; ++k) {
            bx0 = std::min(bx0, c[k].x()); bx1 = std::max(bx1, c[k].x());
            by0 = std::min(by0, c[k].y()); by1 = std::max(by1, c[k].y());
        }
        op.dstBBox20 = QRectF(QPointF(bx0, by0), QPointF(bx1, by1));
        ops.append(op);
    }
    return ops;
}

// ── cage warp geometry helpers ────────────────────────────────────────────────

// Cross product of (b-a) × (p-a). Positive = CCW, negative = CW.
static inline double cwCross(const QPointF& a, const QPointF& b, const QPointF& p)
{
    return (b.x()-a.x())*(p.y()-a.y()) - (b.y()-a.y())*(p.x()-a.x());
}

static bool cwPointInTri(const QPointF& p,
                          const QPointF& a, const QPointF& b, const QPointF& c)
{
    double d1 = cwCross(a, b, p);
    double d2 = cwCross(b, c, p);
    double d3 = cwCross(c, a, p);
    bool neg = (d1<0)||(d2<0)||(d3<0);
    bool pos = (d1>0)||(d2>0)||(d3>0);
    return !(neg && pos);
}

// Check whether p is inside the quad (TL, TR, BR, BL order).
static bool cwPointInQuad(const QPointF& p, const QPointF q[4])
{
    return cwPointInTri(p, q[0], q[1], q[2]) ||
           cwPointInTri(p, q[0], q[2], q[3]);
}

// Newton–Raphson bilinear inversion: given p in dst quad, map to src quad.
// Returns the mapped source point. Same algorithm as GeometryUtils::barycentric for quads.
static QPointF cwBilinearInverse(const QPointF& p,
                                  const QPointF dst[4], const QPointF src[4])
{
    QPointF Q(0, 0);  // initial guess (parametric coords in [-1,1]×[-1,1])
    QPointF v0 = dst[0]+dst[1]+dst[2]+dst[3];
    QPointF v1 = -dst[0]+dst[1]+dst[2]-dst[3];
    QPointF v2 = -dst[0]-dst[1]+dst[2]+dst[3];
    QPointF v3 = dst[0]-dst[1]+dst[2]-dst[3];
    for (int iter = 0; iter < 10; ++iter) {
        QPointF rhs = 4.0*p - v0 - Q.x()*v1 - Q.y()*v2 - Q.x()*Q.y()*v3;
        if (rhs.x()*rhs.x() + rhs.y()*rhs.y() <= 1e-10) break;
        QPointF A0 = v1 + v3*Q.y();
        QPointF A1 = v2 + v3*Q.x();
        double det = A0.x()*A1.y() - A1.x()*A0.y();
        if (std::abs(det) < 1e-12) break;
        Q += QPointF((A1.y()*rhs.x() - A1.x()*rhs.y()) / det,
                     (A0.x()*rhs.y() - A0.y()*rhs.x()) / det);
    }
    // Bilinear interpolation using parametric coords Q ∈ [-1,1]²
    double w[4] = { (1-Q.x())*(1-Q.y()), (1+Q.x())*(1-Q.y()),
                    (1+Q.x())*(1+Q.y()), (1-Q.x())*(1+Q.y()) };
    return 0.25 * (w[0]*src[0] + w[1]*src[1] + w[2]*src[2] + w[3]*src[3]);
}

// ── mask and inverse-map helpers ──────────────────────────────────────────────

static inline int toMaskCoord(double offset, double levelSize, int maskSize)
{
    int mc = (int)(offset * maskSize / levelSize);
    return qBound(0, mc, maskSize - 1);
}

static inline bool isMasked(const QImage& mask, int mx, int my)
{
    return mask.constBits()[my * mask.bytesPerLine() + mx] > 128;
}

// ── per-level operation (precomputed per pyramid level) ───────────────────────

struct LevelOp {
    QRect   srcL;        // erase zone in level-pixel space
    QRectF  dstBBoxL;    // destination bounding box in level-pixel space
    int     opIdx;
    bool    isTransform  = false;
    bool    isPerspective = false;
    int     mirrorPlane  = 0;  // 0=none, 1=vertical, 2=horizontal

    // ── affine TransformLayer (isTransform==true) ─────────────────────────
    double  itemPxL, itemPyL;  // itemPos scaled to level pixels
    double  m31L, m32L;        // m31/m32 scaled to level pixels
    double  m11, m12, m21, m22, det;

    // ── PerspectiveWarp (isPerspective==true) ─────────────────────────────
    // Inverse homography in level-k pixel space: maps warped-local → source-local
    double newPxL = 0, newPyL = 0;  // warped image origin in level-k pixels
    double ip11=1,ip12=0,ip13=0;
    double ip21=0,ip22=1,ip23=0;
    double ip31=0,ip32=0,ip33=1;

    // ── CageWarp (isCageWarp==true) ───────────────────────────────────────
    bool    isCageWarp = false;
    QPointF cwTopL;                  // topLeft_after in level-k pixels
    QVector<QPointF> cwBeforeL;      // before grid points in level-k pixels (layer-local)
    QVector<QPointF> cwAfterL;       // after  grid points in level-k pixels (warped-local)
    QVector<QRectF>  cwCellBBoxL;    // precomputed bbox per cell (row-major, (rows-1)*(cols-1))
    int cwRows = 0, cwCols = 0;

    // ── pre-affine for CageWarp (cwHasPreAffine==true) ────────────────────
    // After cage inverse gives (lx,ly) in affine-display local space, apply:
    //   dx = lx - cwPA_m31L; dy = ly - cwPA_m32L
    //   ox = (cwPA_m22*dx - cwPA_m21*dy) / cwPA_det
    //   oy = (cwPA_m11*dy - cwPA_m12*dx) / cwPA_det
    bool   cwHasPreAffine = false;
    double cwPA_m11=1, cwPA_m12=0, cwPA_m21=0, cwPA_m22=1;
    double cwPA_m31L=0, cwPA_m32L=0, cwPA_det=1;
};

// Given a destination pixel (gx, gy) at this pyramid level, compute the
// source local position (lx, ly) within the source bounding rect, check mask,
// and return the global source coordinates (srcGx, srcGy). Returns false when
// the pixel is outside the masked region.
static bool sourcePixelFor(const LevelOp& lo, const PasteOp& op,
                            double gx, double gy,
                            double& srcGx, double& srcGy)
{
    double lx, ly;

    if (!lo.isTransform && !lo.isPerspective && !lo.isCageWarp) {
        // MoveLayer: pure translation
        lx = gx - lo.dstBBoxL.x();
        ly = gy - lo.dstBBoxL.y();
    } else if (lo.isTransform) {
        // Affine TransformLayer: inverse via Cramer's rule
        double dx = gx - lo.itemPxL - lo.m31L;
        double dy = gy - lo.itemPyL - lo.m32L;
        lx = (lo.m22 * dx - lo.m21 * dy) / lo.det;
        ly = (lo.m11 * dy - lo.m12 * dx) / lo.det;
    } else if (lo.isPerspective) {
        // PerspectiveWarp: apply inverse homography in level-k pixel space
        double wx = gx - lo.newPxL;
        double wy = gy - lo.newPyL;
        double w  = lo.ip13 * wx + lo.ip23 * wy + lo.ip33;
        if (std::abs(w) < 1e-10) return false;
        lx = (lo.ip11 * wx + lo.ip21 * wy + lo.ip31) / w;
        ly = (lo.ip12 * wx + lo.ip22 * wy + lo.ip32) / w;
    } else {
        // CageWarp: find containing cell via bbox pre-filter + pointInQuad,
        // then bilinear inverse to map warped-local → source-local.
        const QPointF wp(gx - lo.cwTopL.x(), gy - lo.cwTopL.y());
        const int rows = lo.cwRows, cols = lo.cwCols;
        bool found = false;
        for (int row = 0; row + 1 < rows && !found; ++row) {
            for (int col = 0; col + 1 < cols && !found; ++col) {
                const QRectF& bb = lo.cwCellBBoxL[(cols-1)*row + col];
                if (!bb.contains(wp)) continue;
                int i0 = row * cols + col;
                const QPointF* a = lo.cwAfterL.constData();
                QPointF dq[4] = { a[i0], a[i0+1], a[i0+cols+1], a[i0+cols] };
                if (!cwPointInQuad(wp, dq)) continue;
                const QPointF* b = lo.cwBeforeL.constData();
                QPointF sq[4] = { b[i0], b[i0+1], b[i0+cols+1], b[i0+cols] };
                QPointF sp = cwBilinearInverse(wp, dq, sq);
                lx = sp.x();
                ly = sp.y();
                found = true;
            }
        }
        if (!found) return false;

        // Apply pre-affine inverse: maps affine-display-local → original-image-local
        // (source → mirror → affine → cage: here we undo the affine step)
        if (lo.cwHasPreAffine) {
            double dx = lx - lo.cwPA_m31L;
            double dy = ly - lo.cwPA_m32L;
            lx = (lo.cwPA_m22 * dx - lo.cwPA_m21 * dy) / lo.cwPA_det;
            ly = (lo.cwPA_m11 * dy - lo.cwPA_m12 * dx) / lo.cwPA_det;
        }
    }

    // Bounds check in local space (must be within source rect dimensions)
    double lwL = lo.srcL.width(), lhL = lo.srcL.height();
    if (lx < 0 || ly < 0 || lx >= lwL || ly >= lhL) return false;

    // Apply inverse mirror (flip is self-inverse)
    if (lo.mirrorPlane == 1)       ly = lhL - 1.0 - ly;  // vertical flip
    else if (lo.mirrorPlane == 2)  lx = lwL - 1.0 - lx;  // horizontal flip

    // Mask check (nearest-neighbour interpolation into the small 20-µm mask)
    int mx = toMaskCoord(lx, lwL, op.mask.width());
    int my = toMaskCoord(ly, lhL, op.mask.height());
    if (!isMasked(op.mask, mx, my)) return false;

    srcGx = lo.srcL.x() + lx;
    srcGy = lo.srcL.y() + ly;
    return true;
}

// ── BigTIFF region reader (tiled levels only) ─────────────────────────────────

static QImage readBigTiffRegionRGB(
    TIFF* tif,
    const ProjLevel& lvl,
    const QRect& bounds,
    uint16_t spp)
{
    const int x0 = qMax(0, bounds.x());
    const int y0 = qMax(0, bounds.y());
    const int x1 = qMin((int)lvl.w, bounds.x() + bounds.width());
    const int y1 = qMin((int)lvl.h, bounds.y() + bounds.height());
    if (x1 <= x0 || y1 <= y0) return {};

    const int rw = x1 - x0, rh = y1 - y0;
    QImage result(rw, rh, QImage::Format_RGB32);
    result.fill(0);

    const uint32_t tW = lvl.tileW, tH = lvl.tileH;
    const tsize_t tBytes = (tsize_t)tW * tH * spp;
    std::vector<uint8_t> tileBuf(tBytes);

    const int startTx = (x0 / (int)tW) * (int)tW;
    const int startTy = (y0 / (int)tH) * (int)tH;

    for (int ty = startTy; ty < y1; ty += (int)tH) {
        for (int tx = startTx; tx < x1; tx += (int)tW) {
            ttile_t idx = TIFFComputeTile(tif, (uint32_t)tx, (uint32_t)ty, 0, 0);
            if (TIFFReadEncodedTile(tif, idx, tileBuf.data(), tBytes) < 0) continue;

            const int py0 = qMax(y0 - ty, 0);
            const int py1 = qMin((int)tH, y1 - ty);
            const int px0 = qMax(x0 - tx, 0);
            const int px1 = qMin((int)tW, x1 - tx);

            for (int py = py0; py < py1; ++py) {
                QRgb* dstRow = reinterpret_cast<QRgb*>(result.scanLine(ty + py - y0));
                for (int px = px0; px < px1; ++px) {
                    const uint8_t* src = &tileBuf[((size_t)py * tW + px) * spp];
                    if (spp == 1)
                        dstRow[tx + px - x0] = qRgb(src[0], src[0], src[0]);
                    else
                        dstRow[tx + px - x0] = qRgb(src[0], src[1], src[2]);
                }
            }
        }
    }
    return result;
}

// ── inpainting patch parser ───────────────────────────────────────────────────

static QVector<InpaintPatch> parseInpaintPatches(
    const QJsonObject& project,
    TIFF* tif,
    const QVector<ProjLevel>& levels,
    int scaleFactor,
    uint16_t spp)
{
    QVector<InpaintPatch> patches;
    if (levels.isEmpty()) return patches;
    const ProjLevel& finest = levels[0];
    if (!finest.tiled) return patches;   // stripped finest level is unusual; skip

    for (const QJsonValue& lv : project["layers"].toArray()) {
        QJsonObject layerObj = lv.toObject();
        if (layerObj.value("creator").toString() != "Inpainting") continue;
        if (!layerObj.contains("data")) continue;

        // Decode saved ARGB32 result PNG
        QByteArray raw = QByteArray::fromBase64(layerObj["data"].toString().toLatin1());
        QImage savedResult;
        if (!savedResult.loadFromData(raw, "PNG")) continue;
        if (savedResult.format() != QImage::Format_ARGB32)
            savedResult = savedResult.convertToFormat(QImage::Format_ARGB32);

        const int sw = savedResult.width(), sh = savedResult.height();

        // Extract hole mask from alpha channel (alpha > 0 → inpainted pixel)
        QImage savedMask(sw, sh, QImage::Format_Grayscale8);
        for (int y = 0; y < sh; ++y) {
            const QRgb* srcRow = reinterpret_cast<const QRgb*>(savedResult.constScanLine(y));
            uint8_t* dstRow = savedMask.scanLine(y);
            for (int x = 0; x < sw; ++x)
                dstRow[x] = (qAlpha(srcRow[x]) > 0) ? 255 : 0;
        }

        // Scale bounds from project (20µm) coords to BigTIFF full-res pixels
        const int px20 = layerObj.value("x").toInt(0);
        const int py20 = layerObj.value("y").toInt(0);
        QRect boundsL0(px20 * scaleFactor, py20 * scaleFactor,
                       sw * scaleFactor,   sh * scaleFactor);
        boundsL0 = boundsL0.intersected(QRect(0, 0, (int)finest.w, (int)finest.h));
        if (boundsL0.isEmpty()) continue;

        // Read BigTIFF source region at full resolution
        TIFFSetDirectory(tif, (uint16_t)finest.dirIdx);
        QImage srcRegion = readBigTiffRegionRGB(tif, finest, boundsL0, spp);
        if (srcRegion.isNull()) continue;

        // Scale hole mask to full BigTIFF resolution
        QImage scaledMask = savedMask.scaled(
            boundsL0.width(), boundsL0.height(),
            Qt::IgnoreAspectRatio, Qt::FastTransformation);
        if (scaledMask.format() != QImage::Format_Grayscale8)
            scaledMask = scaledMask.convertToFormat(QImage::Format_Grayscale8);

        QImage resultRGB;

#ifdef HASLAMA
        {
            QString errMsg;
            const QString modelPath = EditorStyle::instance().lamaModelPath().isEmpty()
                                      ? LaMaInpainting::defaultModelPath()
                                      : EditorStyle::instance().lamaModelPath();
            QImage inpainted = LaMaInpainting::run(srcRegion, scaledMask, modelPath, &errMsg);
            if (!inpainted.isNull()) {
                resultRGB = inpainted.convertToFormat(QImage::Format_RGB32);
                qInfo() << "LaMa inpainting applied at BigTIFF resolution:"
                        << boundsL0.width() << "x" << boundsL0.height();
            } else {
                qWarning() << "LaMa failed for inpainting patch:" << errMsg
                           << "- falling back to upscaled saved result.";
            }
        }
#endif

        if (resultRGB.isNull()) {
            // Fallback: upscale the saved result and composite over the source region
            resultRGB = srcRegion.copy();
            if (resultRGB.format() != QImage::Format_RGB32)
                resultRGB = resultRGB.convertToFormat(QImage::Format_RGB32);
            QImage scaledSaved = savedResult.scaled(
                boundsL0.width(), boundsL0.height(),
                Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                .convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < boundsL0.height(); ++y) {
                QRgb* dstRow = reinterpret_cast<QRgb*>(resultRGB.scanLine(y));
                const QRgb* srcSaved = reinterpret_cast<const QRgb*>(scaledSaved.constScanLine(y));
                for (int x = 0; x < boundsL0.width(); ++x) {
                    if (qAlpha(srcSaved[x]) > 0)
                        dstRow[x] = qRgb(qRed(srcSaved[x]), qGreen(srcSaved[x]), qBlue(srcSaved[x]));
                }
            }
        }

        InpaintPatch patch;
        patch.boundsL0  = boundsL0;
        patch.resultRGB = resultRGB;
        patch.holeMask  = scaledMask;
        patches.append(patch);
        qInfo() << "Inpainting patch ready: project bounds=("
                << px20 << "," << py20 << "+" << sw << "x" << sh
                << ") → BigTIFF bounds=" << boundsL0;
    }
    return patches;
}

// ── source-tile cache ─────────────────────────────────────────────────────────

struct SrcCache {
    static constexpr int kMax = 64;

    const uint8_t* fetch(TIFF* tif, ttile_t idx, tsize_t bytes) {
        for (auto& e : entries) if (e.first == idx) return e.second.data();
        if ((int)entries.size() >= kMax) entries.erase(entries.begin());
        entries.push_back({ idx, std::vector<uint8_t>(bytes, 0) });
        TIFFReadEncodedTile(tif, idx, entries.back().second.data(), bytes);
        return entries.back().second.data();
    }

    void clear() { entries.clear(); }

    std::vector<std::pair<ttile_t, std::vector<uint8_t>>> entries;
};

// ── tiled level processor ─────────────────────────────────────────────────────

static bool processTiledLevel(
    TIFF* in, TIFF* in2, TIFF* out,
    const ProjLevel& lvl,
    const QVector<LevelOp>& levelOps,
    const QVector<PasteOp>& ops,
    const QVector<LevelInpaintPatch>& patches,
    uint16_t spp,
    int& done, int totalTiles,
    bool& cancelled,
    std::function<bool(int)>& progress)
{
    uint32_t outTW = lvl.tileW, outTH = lvl.tileH;
    int tilesX = (int(lvl.w) + int(outTW) - 1) / int(outTW);
    int tilesY = (int(lvl.h) + int(outTH) - 1) / int(outTH);
    tsize_t tileBytes = (tsize_t)outTW * outTH * spp;

    std::vector<uint8_t> tileBuf(tileBytes, 0);
    SrcCache srcCache;

    for (int ty = 0; ty < tilesY && !cancelled; ++ty) {
        for (int tx = 0; tx < tilesX && !cancelled; ++tx) {
            uint32_t tileX = (uint32_t)tx * outTW;
            uint32_t tileY = (uint32_t)ty * outTH;
            QRectF tileRect((double)tileX, (double)tileY,
                            (double)outTW, (double)outTH);

            bool needsErase = false, needsPaste = false, needsPatch = false;
            for (const auto& lo : levelOps) {
                if (tileRect.intersects(QRectF(lo.srcL))) needsErase = true;
                if (tileRect.intersects(lo.dstBBoxL))    needsPaste = true;
            }
            for (const auto& p : patches)
                if (tileRect.intersects(QRectF(p.bounds))) needsPatch = true;

            // Load input tile
            std::fill(tileBuf.begin(), tileBuf.end(), 0);
            ttile_t mainIdx = TIFFComputeTile(in, tileX, tileY, 0, 0);
            TIFFReadEncodedTile(in, mainIdx, tileBuf.data(), tileBytes);

            if (needsErase || needsPaste || needsPatch) {
                for (uint32_t py = 0; py < outTH; ++py) {
                    uint32_t gy = tileY + py;
                    if (gy >= lvl.h) break;

                    for (uint32_t px = 0; px < outTW; ++px) {
                        uint32_t gx = tileX + px;
                        if (gx >= lvl.w) break;

                        size_t off = ((size_t)py * outTW + px) * spp;

                        // Phase 1: erase masked pixels in source zone
                        for (const auto& lo : levelOps) {
                            if (!lo.srcL.contains((int)gx, (int)gy)) continue;
                            const auto& op = ops[lo.opIdx];
                            int mx = toMaskCoord(gx - lo.srcL.x(), lo.srcL.width(),  op.mask.width());
                            int my = toMaskCoord(gy - lo.srcL.y(), lo.srcL.height(), op.mask.height());
                            if (isMasked(op.mask, mx, my))
                                std::memset(&tileBuf[off], 0, spp);
                        }

                        // Phase 2: paste pixels into destination zone
                        for (const auto& lo : levelOps) {
                            if (!lo.dstBBoxL.contains((double)gx, (double)gy)) continue;

                            const auto& op = ops[lo.opIdx];
                            double srcGxD, srcGyD;
                            if (!sourcePixelFor(lo, op, (double)gx, (double)gy,
                                                srcGxD, srcGyD)) continue;

                            int iSrcGx = (int)srcGxD, iSrcGy = (int)srcGyD;
                            if (iSrcGx < 0 || iSrcGy < 0 ||
                                iSrcGx >= (int)lvl.w || iSrcGy >= (int)lvl.h) continue;

                            uint32_t stx = ((uint32_t)iSrcGx / outTW) * outTW;
                            uint32_t sty = ((uint32_t)iSrcGy / outTH) * outTH;
                            ttile_t srcIdx = TIFFComputeTile(in2, stx, sty, 0, 0);
                            const uint8_t* srcTile = srcCache.fetch(in2, srcIdx, tileBytes);

                            size_t sox = (uint32_t)iSrcGx - stx;
                            size_t soy = (uint32_t)iSrcGy - sty;
                            std::memcpy(&tileBuf[off],
                                        srcTile + (soy * outTW + sox) * spp, spp);
                        }

                        // Phase 3: apply inpainting patches
                        if (needsPatch) {
                            for (const auto& p : patches) {
                                if (!p.bounds.contains((int)gx, (int)gy)) continue;
                                const int lx = (int)gx - p.bounds.x();
                                const int ly = (int)gy - p.bounds.y();
                                const int pw = p.bounds.width(), ph = p.bounds.height();
                                const int mx = qBound(0, lx * p.mask.width()  / pw, p.mask.width()  - 1);
                                const int my = qBound(0, ly * p.mask.height() / ph, p.mask.height() - 1);
                                if (p.mask.constScanLine(my)[mx] == 0) continue;
                                const int rx = qBound(0, lx * p.result.width()  / pw, p.result.width()  - 1);
                                const int ry = qBound(0, ly * p.result.height() / ph, p.result.height() - 1);
                                const QRgb pixel = *reinterpret_cast<const QRgb*>(
                                    p.result.constScanLine(ry) + rx * sizeof(QRgb));
                                if (spp == 1) {
                                    tileBuf[off] = (uint8_t)qGray(pixel);
                                } else {
                                    tileBuf[off]   = (uint8_t)qRed(pixel);
                                    if (spp >= 2) tileBuf[off+1] = (uint8_t)qGreen(pixel);
                                    if (spp >= 3) tileBuf[off+2] = (uint8_t)qBlue(pixel);
                                }
                                break;
                            }
                        }
                    }
                }
            }

            TIFFWriteTile(out, tileBuf.data(), tileX, tileY, 0, 0);
            if (progress && !progress(++done * 100 / totalTiles)) cancelled = true;
        }
    }
    return !cancelled;
}

// ── stripped level processor (small coarse levels) ────────────────────────────

static bool processStrippedLevel(
    TIFF* in, TIFF* out,
    const ProjLevel& lvl,
    const QVector<LevelOp>& levelOps,
    const QVector<PasteOp>& ops,
    const QVector<LevelInpaintPatch>& patches,
    uint16_t spp,
    int& done, int totalTiles,
    bool& cancelled,
    std::function<bool(int)>& progress)
{
    std::vector<uint32_t> raster((size_t)lvl.w * lvl.h);
    TIFFReadRGBAImageOriented(in, lvl.w, lvl.h, raster.data(), ORIENTATION_TOPLEFT, 0);

    uint32_t outTW = 256, outTH = 256;
    int tilesX = (int(lvl.w) + int(outTW) - 1) / int(outTW);
    int tilesY = (int(lvl.h) + int(outTH) - 1) / int(outTH);
    std::vector<uint8_t> outBuf((size_t)outTW * outTH * spp, 0);

    for (int ty = 0; ty < tilesY && !cancelled; ++ty) {
        for (int tx = 0; tx < tilesX && !cancelled; ++tx) {
            uint32_t x0 = (uint32_t)tx * outTW;
            uint32_t y0 = (uint32_t)ty * outTH;
            uint32_t tw = std::min(outTW, lvl.w  - x0);
            uint32_t th = std::min(outTH, lvl.h - y0);
            std::fill(outBuf.begin(), outBuf.end(), 0);

            for (uint32_t r = 0; r < th; ++r) {
                for (uint32_t c = 0; c < tw; ++c) {
                    uint32_t gx = x0 + c, gy = y0 + r;
                    uint32_t abgr = raster[(size_t)gy * lvl.w + gx];
                    size_t   off  = ((size_t)r * outTW + c) * spp;

                    // Copy source channels from raster
                    if (spp == 1) {
                        outBuf[off] = TIFFGetR(abgr);
                    } else {
                        outBuf[off]   = TIFFGetR(abgr);
                        outBuf[off+1] = TIFFGetG(abgr);
                        if (spp >= 3) outBuf[off+2] = TIFFGetB(abgr);
                        if (spp == 4) outBuf[off+3] = TIFFGetA(abgr);
                    }

                    // Phase 1: erase
                    for (const auto& lo : levelOps) {
                        if (!lo.srcL.contains((int)gx, (int)gy)) continue;
                        const auto& op = ops[lo.opIdx];
                        int mx = toMaskCoord(gx - lo.srcL.x(), lo.srcL.width(),  op.mask.width());
                        int my = toMaskCoord(gy - lo.srcL.y(), lo.srcL.height(), op.mask.height());
                        if (isMasked(op.mask, mx, my))
                            std::memset(&outBuf[off], 0, spp);
                    }

                    // Phase 2: paste (source from raster)
                    for (const auto& lo : levelOps) {
                        if (!lo.dstBBoxL.contains((double)gx, (double)gy)) continue;
                        const auto& op = ops[lo.opIdx];
                        double srcGxD, srcGyD;
                        if (!sourcePixelFor(lo, op, (double)gx, (double)gy,
                                            srcGxD, srcGyD)) continue;

                        int iSrcGx = (int)srcGxD, iSrcGy = (int)srcGyD;
                        if (iSrcGx < 0 || iSrcGy < 0 ||
                            iSrcGx >= (int)lvl.w || iSrcGy >= (int)lvl.h) continue;

                        uint32_t srcAbgr = raster[(size_t)iSrcGy * lvl.w + iSrcGx];
                        if (spp == 1) {
                            outBuf[off] = TIFFGetR(srcAbgr);
                        } else {
                            outBuf[off]   = TIFFGetR(srcAbgr);
                            outBuf[off+1] = TIFFGetG(srcAbgr);
                            if (spp >= 3) outBuf[off+2] = TIFFGetB(srcAbgr);
                            if (spp == 4) outBuf[off+3] = TIFFGetA(srcAbgr);
                        }
                    }

                    // Phase 3: apply inpainting patches
                    for (const auto& p : patches) {
                        if (!p.bounds.contains((int)gx, (int)gy)) continue;
                        const int lx = (int)gx - p.bounds.x();
                        const int ly = (int)gy - p.bounds.y();
                        const int pw = p.bounds.width(), ph = p.bounds.height();
                        const int mx = qBound(0, lx * p.mask.width()  / pw, p.mask.width()  - 1);
                        const int my = qBound(0, ly * p.mask.height() / ph, p.mask.height() - 1);
                        if (p.mask.constScanLine(my)[mx] == 0) continue;
                        const int rx = qBound(0, lx * p.result.width()  / pw, p.result.width()  - 1);
                        const int ry = qBound(0, ly * p.result.height() / ph, p.result.height() - 1);
                        const QRgb pixel = *reinterpret_cast<const QRgb*>(
                            p.result.constScanLine(ry) + rx * sizeof(QRgb));
                        if (spp == 1) {
                            outBuf[off] = (uint8_t)qGray(pixel);
                        } else {
                            outBuf[off]   = (uint8_t)qRed(pixel);
                            if (spp >= 2) outBuf[off+1] = (uint8_t)qGreen(pixel);
                            if (spp >= 3) outBuf[off+2] = (uint8_t)qBlue(pixel);
                        }
                        break;
                    }
                }
            }

            TIFFWriteTile(out, outBuf.data(), x0, y0, 0, 0);
            if (progress && !progress(++done * 100 / totalTiles)) cancelled = true;
        }
    }
    return !cancelled;
}

// ── public API ────────────────────────────────────────────────────────────────

bool bigTiffApplyProject(
    const QString& inputPath,
    const QString& outputPath,
    const QJsonObject& project,
    int scaleFactor,
    std::function<bool(int)> progress,
    QString* errorOut)
{
    auto fail = [&](const QString& msg) -> bool {
        if (errorOut) *errorOut = msg;
        return false;
    };

    QVector<PasteOp> ops = parseOps(project);

    TIFFSetWarningHandler(nullptr);

    TIFF* in  = TIFFOpen(inputPath.toLocal8Bit().constData(), "r");
    if (!in) return fail("Cannot open input: " + inputPath);

    TIFF* in2 = TIFFOpen(inputPath.toLocal8Bit().constData(), "r");
    if (!in2) {
        TIFFClose(in);
        return fail("Cannot open input (second handle): " + inputPath);
    }

    QVector<ProjLevel> levels = scanProjLevels(in);
    if (levels.isEmpty()) {
        TIFFClose(in); TIFFClose(in2);
        return fail("No valid IFDs found in input.");
    }

    // Determine spp from finest level for patch parsing
    TIFFSetDirectory(in, (uint16_t)levels[0].dirIdx);
    uint16_t spp0 = 1;
    TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &spp0);

    // Parse inpainting patches (reads BigTIFF regions + runs LaMa at full res)
    QVector<InpaintPatch> rawPatches = parseInpaintPatches(project, in, levels, scaleFactor, spp0);

    if (ops.isEmpty() && rawPatches.isEmpty())
        return fail("No applicable operations found in project "
                    "(need LassoCutCommand + MoveLayer/TransformLayer, or Inpainting layers).");

    TIFF* out = TIFFOpen(outputPath.toLocal8Bit().constData(), "w8");
    if (!out) {
        TIFFClose(in); TIFFClose(in2);
        return fail("Cannot create output: " + outputPath);
    }
    if (!TIFFIsBigTIFF(out)) {
        TIFFClose(out); TIFFClose(in); TIFFClose(in2);
        QFile::remove(outputPath);
        return fail("libtiff does not support BigTIFF write mode (\"w8\"). "
                    "Check rpath / LD_LIBRARY_PATH.");
    }

    // Count total tiles for progress
    int totalTiles = 0;
    for (const auto& lvl : levels) {
        uint32_t tw = lvl.tiled ? lvl.tileW : 256u;
        uint32_t th = lvl.tiled ? lvl.tileH : 256u;
        totalTiles += ((int(lvl.w) + int(tw) - 1) / int(tw))
                    * ((int(lvl.h) + int(th) - 1) / int(th));
    }
    int  done      = 0;
    bool cancelled = false;

    for (int li = 0; li < levels.size() && !cancelled; ++li) {
        const auto& lvl = levels[li];
        TIFFSetDirectory(in,  (uint16_t)lvl.dirIdx);
        TIFFSetDirectory(in2, (uint16_t)lvl.dirIdx);

        // Scale from 20-µm project space → pixels at this pyramid level.
        // Full-res pixel  = 20µm-coord × scaleFactor
        // Level-k pixel   = full-res   × (lvl.w / levels[0].w)
        double coordScale = (double)scaleFactor * lvl.w / levels[0].w;

        // Build per-level op list with precomputed inverse-transform parameters
        QVector<LevelOp> levelOps;
        levelOps.reserve(ops.size());
        for (int oi = 0; oi < ops.size(); ++oi) {
            const auto& op = ops[oi];
            LevelOp lo;
            lo.opIdx = oi;

            // Erase zone (same for all op types)
            lo.srcL = QRect(
                (int)std::round(op.srcRect20.x()      * coordScale),
                (int)std::round(op.srcRect20.y()      * coordScale),
                qMax(1, (int)std::round(op.srcRect20.width()  * coordScale)),
                qMax(1, (int)std::round(op.srcRect20.height() * coordScale)));

            lo.isTransform   = (op.type == ProjOpType::Transform);
            lo.isPerspective = (op.type == ProjOpType::Perspective);
            lo.isCageWarp    = (op.type == ProjOpType::CageWarp);
            lo.mirrorPlane   = op.mirrorPlane;

            if (!lo.isTransform && !lo.isPerspective && !lo.isCageWarp) {
                // MoveLayer: destination = dstPos, same size as source
                lo.dstBBoxL = QRectF(op.dstPos20.x() * coordScale,
                                     op.dstPos20.y() * coordScale,
                                     lo.srcL.width(), lo.srcL.height());
            } else if (lo.isTransform) {
                // Affine TransformLayer: scale the precomputed bounding box
                lo.dstBBoxL = QRectF(
                    op.dstBBox20.x()      * coordScale,
                    op.dstBBox20.y()      * coordScale,
                    op.dstBBox20.width()  * coordScale,
                    op.dstBBox20.height() * coordScale);

                // (m11..m22 are dimensionless; m31/m32 and itemPos scale by coordScale)
                lo.itemPxL = op.itemPos20.x() * coordScale;
                lo.itemPyL = op.itemPos20.y() * coordScale;
                lo.m31L    = op.m31 * coordScale;
                lo.m32L    = op.m32 * coordScale;
                lo.m11 = op.m11; lo.m12 = op.m12;
                lo.m21 = op.m21; lo.m22 = op.m22;
                lo.det = op.det;
            } else if (lo.isPerspective) {
                // PerspectiveWarp: scale homography from 20-µm to level-k pixels.
                // For a homography H, scaling inputs/outputs by s transforms:
                //   m31,m32 *= s  (translation terms)
                //   m13,m23 /= s  (homogeneous terms)
                //   m11,m12,m21,m22 unchanged (dimensionless)
                double s = coordScale;
                QTransform H(op.pw_m11,      op.pw_m12,      op.pw_m13 / s,
                             op.pw_m21,      op.pw_m22,      op.pw_m23 / s,
                             op.pw_m31 * s,  op.pw_m32 * s,  op.pw_m33);
                bool ok = false;
                QTransform Hinv = H.inverted(&ok);
                if (!ok) continue;

                lo.dstBBoxL = QRectF(
                    op.dstBBox20.x()      * coordScale,
                    op.dstBBox20.y()      * coordScale,
                    op.dstBBox20.width()  * coordScale,
                    op.dstBBox20.height() * coordScale);
                lo.newPxL = op.pwNewPos20.x() * coordScale;
                lo.newPyL = op.pwNewPos20.y() * coordScale;
                lo.ip11 = Hinv.m11(); lo.ip12 = Hinv.m12(); lo.ip13 = Hinv.m13();
                lo.ip21 = Hinv.m21(); lo.ip22 = Hinv.m22(); lo.ip23 = Hinv.m23();
                lo.ip31 = Hinv.dx();  lo.ip32 = Hinv.dy();  lo.ip33 = Hinv.m33();
            } else if (lo.isCageWarp) {
                lo.dstBBoxL = QRectF(
                    op.dstBBox20.x()      * coordScale,
                    op.dstBBox20.y()      * coordScale,
                    op.dstBBox20.width()  * coordScale,
                    op.dstBBox20.height() * coordScale);
                lo.cwTopL = QPointF(op.cwTopLeft20.x() * coordScale,
                                    op.cwTopLeft20.y() * coordScale);
                lo.cwRows = op.cwRows;
                lo.cwCols = op.cwCols;

                lo.cwBeforeL.resize(op.cwBefore20.size());
                for (int i = 0; i < op.cwBefore20.size(); ++i)
                    lo.cwBeforeL[i] = op.cwBefore20[i] * coordScale;

                lo.cwAfterL.resize(op.cwAfter20.size());
                for (int i = 0; i < op.cwAfter20.size(); ++i)
                    lo.cwAfterL[i] = op.cwAfter20[i] * coordScale;

                // Precompute per-cell bounding boxes for fast rejection
                const int nCells = (lo.cwRows - 1) * (lo.cwCols - 1);
                lo.cwCellBBoxL.resize(nCells);
                for (int row = 0; row + 1 < lo.cwRows; ++row) {
                    for (int col = 0; col + 1 < lo.cwCols; ++col) {
                        const QPointF* q = lo.cwAfterL.constData();
                        int i0 = row * lo.cwCols + col;
                        int i1 = i0+1, i2 = i0+lo.cwCols, i3 = i0+lo.cwCols+1;
                        double xMin = std::min({q[i0].x(), q[i1].x(), q[i2].x(), q[i3].x()});
                        double xMax = std::max({q[i0].x(), q[i1].x(), q[i2].x(), q[i3].x()});
                        double yMin = std::min({q[i0].y(), q[i1].y(), q[i2].y(), q[i3].y()});
                        double yMax = std::max({q[i0].y(), q[i1].y(), q[i2].y(), q[i3].y()});
                        lo.cwCellBBoxL[(lo.cwCols - 1) * row + col] = QRectF(xMin, yMin, xMax - xMin, yMax - yMin);
                    }
                }

                // Pre-affine inverse parameters (m11/m22 dimensionless; m31/m32 scale by coordScale)
                if (op.hasPreAffine) {
                    lo.cwHasPreAffine = true;
                    lo.cwPA_m11 = op.pa_m11; lo.cwPA_m12 = op.pa_m12;
                    lo.cwPA_m21 = op.pa_m21; lo.cwPA_m22 = op.pa_m22;
                    lo.cwPA_m31L = op.pa_m31 * coordScale;
                    lo.cwPA_m32L = op.pa_m32 * coordScale;
                    lo.cwPA_det  = op.pa_det;
                }
            }

            levelOps.append(lo);
        }

        // Read source metadata
        uint16_t bps = 8, spp = 1;
        uint16_t photo = PHOTOMETRIC_MINISBLACK;
        TIFFGetField(in, TIFFTAG_BITSPERSAMPLE,   &bps);
        TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &spp);
        TIFFGetField(in, TIFFTAG_PHOTOMETRIC,     &photo);

        uint32_t outTW = lvl.tiled ? lvl.tileW : 256u;
        uint32_t outTH = lvl.tiled ? lvl.tileH : 256u;

        // Scale inpainting patches to current pyramid level
        const double levelScale = (double)lvl.w / levels[0].w;
        QVector<LevelInpaintPatch> levelPatches;
        levelPatches.reserve(rawPatches.size());
        for (const auto& p : rawPatches) {
            LevelInpaintPatch lp;
            lp.bounds = QRect(
                (int)std::round(p.boundsL0.x()      * levelScale),
                (int)std::round(p.boundsL0.y()      * levelScale),
                qMax(1, (int)std::round(p.boundsL0.width()  * levelScale)),
                qMax(1, (int)std::round(p.boundsL0.height() * levelScale)));
            lp.result = (lp.bounds.size() == p.resultRGB.size())
                        ? p.resultRGB
                        : p.resultRGB.scaled(lp.bounds.size(),
                                             Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation)
                                     .convertToFormat(QImage::Format_RGB32);
            lp.mask = (lp.bounds.size() == p.holeMask.size())
                      ? p.holeMask
                      : p.holeMask.scaled(lp.bounds.size(),
                                          Qt::IgnoreAspectRatio,
                                          Qt::FastTransformation)
                                  .convertToFormat(QImage::Format_Grayscale8);
            levelPatches.append(lp);
        }

        TIFFSetField(out, TIFFTAG_IMAGEWIDTH,      lvl.w);
        TIFFSetField(out, TIFFTAG_IMAGELENGTH,     lvl.h);
        TIFFSetField(out, TIFFTAG_TILEWIDTH,       outTW);
        TIFFSetField(out, TIFFTAG_TILELENGTH,      outTH);
        TIFFSetField(out, TIFFTAG_BITSPERSAMPLE,   bps);
        TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, spp);
        TIFFSetField(out, TIFFTAG_PHOTOMETRIC,     photo);
        TIFFSetField(out, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
        TIFFSetField(out, TIFFTAG_COMPRESSION,     COMPRESSION_DEFLATE);
        TIFFSetField(out, TIFFTAG_PREDICTOR,       PREDICTOR_HORIZONTAL);
        if (li > 0)
            TIFFSetField(out, TIFFTAG_SUBFILETYPE, (uint32_t)FILETYPE_REDUCEDIMAGE);

        if (lvl.tiled)
            processTiledLevel(in, in2, out, lvl, levelOps, ops, levelPatches, spp,
                              done, totalTiles, cancelled, progress);
        else
            processStrippedLevel(in, out, lvl, levelOps, ops, levelPatches, spp,
                                 done, totalTiles, cancelled, progress);

        TIFFWriteDirectory(out);
    }

    TIFFClose(out);
    TIFFClose(in);
    TIFFClose(in2);

    if (cancelled) {
        QFile::remove(outputPath);
        return fail("Operation cancelled.");
    }
    return true;
}
