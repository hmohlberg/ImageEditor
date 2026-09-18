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

#include <QFile>
#include <QImage>
#include <QRect>
#include <QRectF>
#include <QPoint>
#include <QPointF>
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

enum class ProjOpType { Move, Transform };

// Describes one LassoCut + subsequent layer operation.
//
// The forward mapping (20-µm canvas space) is:
//   MoveLayer:     canvas = dstPos + (lx, ly)
//   TransformLayer: canvas = itemPos + QTransform(m11..m32).map((lx, ly))
//                         = (itemPos.x + m11*lx + m21*ly + m31,
//                            itemPos.y + m12*lx + m22*ly + m32)
//
// where (lx, ly) is the pixel's position within the local source rect,
// i.e. lx ∈ [0, srcRect.width()), ly ∈ [0, srcRect.height()).
struct PasteOp {
    QImage      mask;        // binary mask at 20-µm, Format_Grayscale8
    QRect       srcRect20;   // source bounding box at 20-µm canvas coords

    ProjOpType  type = ProjOpType::Move;

    // ── MoveLayer ────────────────────────────────────────────────────────────
    QPoint      dstPos20;    // canvas position of local origin after move

    // ── TransformLayer (QTransform from JSON) ─────────────────────────────
    // Row-major Qt convention: (x',y') = (m11*x + m21*y + m31, m12*x + m22*y + m32)
    double m11=1, m12=0, m21=0, m22=1, m31=0, m32=0;
    double det = 1;          // m11*m22 - m12*m21, precomputed
    QPoint itemPos20;        // layer pos() in canvas (newPosition from JSON)
    QRectF dstBBox20;        // bounding box of transformed region at 20 µm
};

// ── parsing ───────────────────────────────────────────────────────────────────

static QVector<PasteOp> parseOps(const QJsonObject& project)
{
    struct LayerInfo { QImage mask; };
    QHash<int, LayerInfo> layerMasks;
    QHash<int, QRect>     cutRects;    // newLayerId → bounding rect from LassoCut
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
        else if (type == "MoveLayer") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;

            PasteOp op;
            op.mask      = layerMasks[layerId].mask;
            op.srcRect20 = cutRects[layerId];
            op.type      = ProjOpType::Move;
            op.dstPos20  = QPoint(cmd["toX"].toInt(), cmd["toY"].toInt());

            // Bounding box = destination rect
            op.dstBBox20 = QRectF(op.dstPos20.x(), op.dstPos20.y(),
                                  op.srcRect20.width(), op.srcRect20.height());
            ops.append(op);
        }
        else if (type == "TransformLayer") {
            int layerId = cmd["layerId"].toInt(-1);
            if (!cutRects.contains(layerId) || !layerMasks.contains(layerId)) continue;

            QJsonObject T = cmd["newTransform"].toObject();
            PasteOp op;
            op.mask      = layerMasks[layerId].mask;
            op.srcRect20 = cutRects[layerId];
            op.type      = ProjOpType::Transform;
            op.m11 = T["m11"].toDouble(1); op.m12 = T["m12"].toDouble(0);
            op.m21 = T["m21"].toDouble(0); op.m22 = T["m22"].toDouble(1);
            op.m31 = T["m31"].toDouble(0); op.m32 = T["m32"].toDouble(0);
            op.det = op.m11 * op.m22 - op.m12 * op.m21;

            QJsonObject np = cmd["newPosition"].toObject();
            op.itemPos20 = QPoint(np["x"].toInt(), np["y"].toInt());

            // Compute destination bounding box by mapping all 4 corners
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
    }
    return ops;
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
    bool    isTransform;

    // Precomputed inverse-transform parameters at this level
    // (only used when isTransform == true):
    double  itemPxL, itemPyL;  // itemPos scaled to level pixels
    double  m31L, m32L;        // m31/m32 scaled to level pixels
    double  m11, m12, m21, m22, det;
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

    if (!lo.isTransform) {
        // MoveLayer: pure translation
        lx = gx - lo.dstBBoxL.x();
        ly = gy - lo.dstBBoxL.y();
    } else {
        // TransformLayer: inverse of the affine transform
        //   (gx, gy) = itemPos + QTransform.map(lx, ly)
        //            = (itemPxL + m11*lx + m21*ly + m31L,
        //               itemPyL + m12*lx + m22*ly + m32L)
        // Rearrange and solve via Cramer's rule (det = m11*m22 - m12*m21):
        double dx = gx - lo.itemPxL - lo.m31L;
        double dy = gy - lo.itemPyL - lo.m32L;
        lx = (lo.m22 * dx - lo.m21 * dy) / lo.det;
        ly = (lo.m11 * dy - lo.m12 * dx) / lo.det;
    }

    // Bounds check in local space (must be within source rect dimensions)
    double lwL = lo.srcL.width(), lhL = lo.srcL.height();
    if (lx < 0 || ly < 0 || lx >= lwL || ly >= lhL) return false;

    // Mask check (nearest-neighbour interpolation into the small 20-µm mask)
    int mx = toMaskCoord(lx, lwL, op.mask.width());
    int my = toMaskCoord(ly, lhL, op.mask.height());
    if (!isMasked(op.mask, mx, my)) return false;

    srcGx = lo.srcL.x() + lx;
    srcGy = lo.srcL.y() + ly;
    return true;
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

            bool needsErase = false, needsPaste = false;
            for (const auto& lo : levelOps) {
                if (tileRect.intersects(QRectF(lo.srcL))) needsErase = true;
                if (tileRect.intersects(lo.dstBBoxL))    needsPaste = true;
            }

            // Load input tile
            std::fill(tileBuf.begin(), tileBuf.end(), 0);
            ttile_t mainIdx = TIFFComputeTile(in, tileX, tileY, 0, 0);
            TIFFReadEncodedTile(in, mainIdx, tileBuf.data(), tileBytes);

            if (needsErase || needsPaste) {
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
    if (ops.isEmpty())
        return fail("No applicable operations found in project "
                    "(need LassoCutCommand + MoveLayer or TransformLayer).");

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

            lo.isTransform = (op.type == ProjOpType::Transform);

            if (!lo.isTransform) {
                // MoveLayer: destination = dstPos, same size as source
                lo.dstBBoxL = QRectF(op.dstPos20.x() * coordScale,
                                     op.dstPos20.y() * coordScale,
                                     lo.srcL.width(), lo.srcL.height());
            } else {
                // TransformLayer: scale the precomputed bounding box
                lo.dstBBoxL = QRectF(
                    op.dstBBox20.x()      * coordScale,
                    op.dstBBox20.y()      * coordScale,
                    op.dstBBox20.width()  * coordScale,
                    op.dstBBox20.height() * coordScale);

                // Precompute inverse-transform parameters at this level.
                // The forward transform at level k is:
                //   px = itemPxL + m11*lx + m21*ly + m31L
                //   py = itemPyL + m12*lx + m22*ly + m32L
                // where itemPxL = itemPos.x * coordScale, m31L = m31 * coordScale
                // (m11..m22 are dimensionless rotation coefficients — no scaling).
                lo.itemPxL = op.itemPos20.x() * coordScale;
                lo.itemPyL = op.itemPos20.y() * coordScale;
                lo.m31L    = op.m31 * coordScale;
                lo.m32L    = op.m32 * coordScale;
                lo.m11 = op.m11; lo.m12 = op.m12;
                lo.m21 = op.m21; lo.m22 = op.m22;
                lo.det = op.det;
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
            processTiledLevel(in, in2, out, lvl, levelOps, ops, spp,
                              done, totalTiles, cancelled, progress);
        else
            processStrippedLevel(in, out, lvl, levelOps, ops, spp,
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
