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

#include "BigTiffViewer.h"

#include "../core/BigTiffIO.h"

#include <tiffio.h>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QPixmap>
#include <QImage>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QTimer>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslError>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <functional>

#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>

// ── memory I/O callbacks for TIFFClientOpen ───────────────────────────────────
namespace {
struct MemIO { const char* data; qint64 size; qint64 pos; };
static tsize_t tiffMemRead(thandle_t h, tdata_t buf, tsize_t n) {
    auto* m = static_cast<MemIO*>(h);
    tsize_t avail = (tsize_t)qMax(qint64(0), m->size - m->pos);
    tsize_t cnt   = qMin(n, avail);
    if (cnt > 0) { std::memcpy(buf, m->data + m->pos, cnt); m->pos += cnt; }
    return cnt;
}
static tsize_t tiffMemWrite(thandle_t, tdata_t, tsize_t) { return -1; }
static toff_t  tiffMemSeek(thandle_t h, toff_t off, int whence) {
    auto* m = static_cast<MemIO*>(h);
    qint64 np;
    if (whence == SEEK_SET) np = (qint64)off;
    else if (whence == SEEK_CUR) np = m->pos + (qint64)off;
    else np = m->size + (qint64)off;  // SEEK_END
    m->pos = qBound(qint64(0), np, m->size);
    return (toff_t)m->pos;
}
static int     tiffMemClose(thandle_t) { return 0; }
static toff_t  tiffMemSize(thandle_t h) { return (toff_t)static_cast<MemIO*>(h)->size; }
static int     tiffMemMap(thandle_t, tdata_t*, toff_t*) { return 0; }
static void    tiffMemUnmap(thandle_t, tdata_t, toff_t) {}
} // namespace

// ── tile cache ───────────────────────────────────────────────────────────────
//  LRU eviction, keyed by level:tileCol:tileRow packed into quint64
static constexpr int MAX_CACHED_TILES = 128;

struct TileCache {
    // key = (level << 40) | (row << 20) | col
    static quint64 makeKey(int level, int col, int row)
    { return ((quint64)(uint8_t)level << 40) | ((quint64)(uint32_t)row << 20) | (quint64)(uint32_t)col; }

    const QPixmap* get(quint64 k) const {
        auto it = m_map.find(k);
        return it != m_map.end() ? &it->pixmap : nullptr;
    }

    void put(quint64 k, const QPixmap& pix) {
        if (m_map.contains(k)) return;
        if (m_map.size() >= MAX_CACHED_TILES) {
            // evict oldest
            quint64 oldest = m_order.takeFirst();
            m_map.remove(oldest);
        }
        m_map.insert(k, { pix });
        m_order.append(k);
    }

    void clear() { m_map.clear(); m_order.clear(); }

private:
    struct Entry { QPixmap pixmap; };
    QHash<quint64, Entry> m_map;
    QList<quint64>        m_order;
};

// ── BigTiffGraphicsView ───────────────────────────────────────────────────────
class BigTiffGraphicsView : public QGraphicsView
{
public:
    explicit BigTiffGraphicsView(QWidget* parent = nullptr)
        : QGraphicsView(parent)
    {
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setRenderHint(QPainter::SmoothPixmapTransform, false);
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
        setBackgroundBrush(QColor(0x28, 0x28, 0x28));
        setMouseTracking(true);
        viewport()->setMouseTracking(true);

        auto* scene = new QGraphicsScene(this);
        setScene(scene);

        // default identity LUT (gray → gray)
        m_baseLut.resize(256);
        m_lut.resize(256);
        for (int i = 0; i < 256; ++i)
            m_baseLut[i] = m_lut[i] = qRgb(i, i, i);
    }

    void setColorTable(const QVector<QRgb>& lut) {
        m_baseLut = lut;
        rebuildEffectiveLut();
    }

    void setBrightness(int v) { m_brightness = qBound(-100, v, 100); rebuildEffectiveLut(); }
    void setContrast(int v)   { m_contrast   = qBound(-100, v, 100); rebuildEffectiveLut(); }

    void rebuildEffectiveLut() {
        m_lut.resize(256);
        const float factor = 1.0f + m_contrast / 100.0f;
        for (int i = 0; i < 256; ++i) {
            int adj = qBound(0, qRound((i - 128) * factor + 128 + m_brightness), 255);
            m_lut[i] = m_baseLut[adj];
        }
        m_cache.clear();
        scene()->update();
    }

    ~BigTiffGraphicsView() { closeTiff(); }

    // ── open / close ─────────────────────────────────────────────────────────
    bool openWeb(const QString& url, const QVector<BigTiffLevel>& levels) {
        closeTiff();
        m_webMode    = true;
        m_fitted     = false;
        m_webBaseUrl = url;
        m_levels     = levels;
        scene()->setSceneRect(0, 0, m_levels[0].w, m_levels[0].h);
        return true;
    }

    bool openTiff(const QString& path) {
        closeTiff();
        m_srcPath = path;
        m_tiff = TIFFOpen(path.toLocal8Bit().constData(), "r");
        if (!m_tiff) return false;

        // scan all IFDs, collect level info
        QVector<BigTiffLevel> raw;
        do {
            BigTiffLevel lvl;
            lvl.dirIdx = (int)TIFFCurrentDirectory(m_tiff);
            TIFFGetField(m_tiff, TIFFTAG_IMAGEWIDTH,  &lvl.w);
            TIFFGetField(m_tiff, TIFFTAG_IMAGELENGTH, &lvl.h);
            lvl.tiled = TIFFIsTiled(m_tiff) != 0;
            if (lvl.tiled) {
                TIFFGetField(m_tiff, TIFFTAG_TILEWIDTH,  &lvl.tileW);
                TIFFGetField(m_tiff, TIFFTAG_TILELENGTH, &lvl.tileH);
            } else {
                lvl.tileW = lvl.w;
                lvl.tileH = lvl.h;
            }
            if (lvl.w > 0 && lvl.h > 0)
                raw.append(lvl);
        } while (TIFFReadDirectory(m_tiff));

        if (raw.isEmpty()) { closeTiff(); return false; }

        // sort finest → coarsest by decreasing width
        std::sort(raw.begin(), raw.end(), [](const BigTiffLevel& a, const BigTiffLevel& b){
            return a.w > b.w;
        });
        m_levels = raw;

        // set scene rect to full-res dimensions
        scene()->setSceneRect(0, 0, m_levels[0].w, m_levels[0].h);
        m_cache.clear();
        return true;
    }

    void closeTiff() {
        if (m_tiff) { TIFFClose(m_tiff); m_tiff = nullptr; }
        m_webMode  = false;
        m_fitted   = false;
        m_webBaseUrl.clear();
        m_pending.clear();
        m_levels.clear();
        m_cache.clear();
        m_srcPath.clear();
        scene()->setSceneRect(QRectF());
    }

    bool isOpen() const { return m_tiff != nullptr || m_webMode; }

    const QVector<BigTiffLevel>& levels() const { return m_levels; }
    bool isBigTiff() const { return m_webMode || (m_tiff && TIFFIsBigTIFF(m_tiff)); }

    // ── zoom helpers ─────────────────────────────────────────────────────────
    void zoomBy(qreal factor) { scale(factor, factor); }

    void fitAll() {
        if (m_levels.isEmpty()) return;
        m_fitted = true;
        fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
        if (onScaleChanged) onScaleChanged(transform().m11());
        emitViewport();
    }

    void emitViewport() {
        if (onViewportChanged)
            onViewportChanged(mapToScene(viewport()->rect()).boundingRect(), sceneRect());
    }

    // called by BigTiffViewer to receive live level/scale updates
    std::function<void(int dirIdx, qreal scale)>  onDebugInfo;
    std::function<void(double)>                   onScaleChanged;
    std::function<void(int, int)>                 onCursorPos;
    std::function<void(QColor)>                   onCursorColor;
    std::function<void(QRectF, QRectF)>           onViewportChanged;

protected:
    void scrollContentsBy(int dx, int dy) override {
        QGraphicsView::scrollContentsBy(dx, dy);
        emitViewport();
    }

    // ── tile-based background rendering ──────────────────────────────────────
    void drawBackground(QPainter* painter, const QRectF& exposed) override {
        QGraphicsView::drawBackground(painter, exposed);
        if ((!m_tiff && !m_webMode) || m_levels.isEmpty()) return;
        if (m_webMode && !m_fitted) return;  // wait for fitAll() before fetching tiles

        const qreal viewScale = transform().m11();
        const int level  = bestLevel(viewScale);
        const auto& lvl  = m_levels[level];
        const qreal D    = (qreal)m_levels[0].w / lvl.w;  // scene units per level pixel

        // Map exposed scene rect to level pixel space
        const qreal lx0 = exposed.left()   / D;
        const qreal ly0 = exposed.top()    / D;
        const qreal lx1 = exposed.right()  / D;
        const qreal ly1 = exposed.bottom() / D;

        {
            const int maxN     = (int)std::floor(
                std::log2((double)qMax(m_levels[0].w, m_levels[0].h) / 256.0));
            const int dirParam = 1 + maxN - (int)std::floor(
                std::log2((double)qMax(lvl.w, lvl.h) / 256.0));
            if (onDebugInfo) onDebugInfo(dirParam, viewScale);
        }

        if (lvl.tiled) {
            const int tileX0 = qMax(0, (int)(lx0 / lvl.tileW));
            const int tileX1 = qMin((int)((lvl.w - 1) / lvl.tileW), (int)(lx1 / lvl.tileW));
            const int tileY0 = qMax(0, (int)(ly0 / lvl.tileH));
            const int tileY1 = qMin((int)((lvl.h - 1) / lvl.tileH), (int)(ly1 / lvl.tileH));

            for (int ty = tileY0; ty <= tileY1; ++ty) {
                for (int tx = tileX0; tx <= tileX1; ++tx) {
                    QPixmap pix = fetchTile(level, tx, ty);
                    if (pix.isNull()) continue;

                    // Clip pixmap to actual image boundary at this level
                    uint32_t pxLeft  = tx * lvl.tileW;
                    uint32_t pyTop   = ty * lvl.tileH;
                    uint32_t validW  = qMin((uint32_t)lvl.tileW, lvl.w  - pxLeft);
                    uint32_t validH  = qMin((uint32_t)lvl.tileH, lvl.h  - pyTop);

                    QRectF dest(pxLeft * D, pyTop * D, validW * D, validH * D);
                    QRectF src(0, 0, validW, validH);
                    painter->drawPixmap(dest, pix, src);
                }
            }
        } else {
            // Stripped level: treat the whole IFD as one tile
            QPixmap pix = fetchTile(level, 0, 0);
            if (!pix.isNull())
                painter->drawPixmap(QRectF(0, 0, lvl.w * D, lvl.h * D), pix, QRectF(pix.rect()));
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        const qreal factor = e->angleDelta().y() > 0 ? 1.25 : 1.0 / 1.25;
        scale(factor, factor);
        // always call parent so AnchorUnderMouse works
        QGraphicsView::wheelEvent(e);
        if (onScaleChanged) onScaleChanged(transform().m11());
        emitViewport();
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        QGraphicsView::mouseMoveEvent(e);
        if (!isOpen() || m_levels.isEmpty()) return;

        const QPointF sp = mapToScene(e->pos());
        if (onCursorPos)
            onCursorPos((int)sp.x(), (int)sp.y());

        if (onCursorColor && m_levels[0].w > 0) {
            const int   level = bestLevel(transform().m11());
            const auto& lvl   = m_levels[level];
            const qreal D     = (qreal)m_levels[0].w / lvl.w;
            const int   lx    = (int)(sp.x() / D);
            const int   ly    = (int)(sp.y() / D);
            if (lx < 0 || ly < 0 || (uint32_t)lx >= lvl.w || (uint32_t)ly >= lvl.h) return;
            const int col = lx / (int)lvl.tileW;
            const int row = ly / (int)lvl.tileH;
            const quint64 key = TileCache::makeKey(level, col, row);
            if (const QPixmap* pix = m_cache.get(key)) {
                const int tx = lx - col * (int)lvl.tileW;
                const int ty = ly - row * (int)lvl.tileH;
                const QImage img = pix->toImage();
                if (tx < img.width() && ty < img.height())
                    onCursorColor(img.pixelColor(tx, ty));
            }
        }
    }

private:
    // ── level selection ───────────────────────────────────────────────────────
    int bestLevel(qreal viewScale) const {
        // viewScale = viewport pixels per full-res pixel
        // Use finest level where one level pixel >= 1 viewport pixel
        for (int i = 0; i < m_levels.size(); ++i) {
            qreal downscale = (qreal)m_levels[0].w / m_levels[i].w;
            if (downscale * viewScale >= 1.0)
                return i;
        }
        return m_levels.size() - 1;
    }

    // ── tile fetching ─────────────────────────────────────────────────────────
    QPixmap fetchTile(int level, int col, int row) {
        quint64 key = TileCache::makeKey(level, col, row);
        if (const QPixmap* cached = m_cache.get(key))
            return *cached;

        if (m_webMode) {
            if (m_pending.contains(key)) return {};
            m_pending.insert(key);
            const auto& lvl = m_levels[level];
            const int maxN    = (int)std::floor(
                std::log2((double)qMax(m_levels[0].w, m_levels[0].h) / 256.0));
            const int dirParam = 1 + maxN - (int)std::floor(
                std::log2((double)qMax(lvl.w, lvl.h) / 256.0));
            const QString tileUrl = m_webBaseUrl
                + "&directory=" + QString::number(dirParam)
                + "&x=" + QString::number(col)
                + "&y=" + QString::number(row);
            QUrl tileQUrl(tileUrl);
            QNetworkRequest req(tileQUrl);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
            req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
            QNetworkReply* reply = m_nam.get(req);
            QObject::connect(reply, &QNetworkReply::sslErrors,
                             reply, [reply](const QList<QSslError>&){ reply->ignoreSslErrors(); });
            QObject::connect(reply, &QNetworkReply::finished,
                             this, [this, key, level, col, row, reply]() {
                m_pending.remove(key);
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) return;
                QPixmap pix = loadTileFromBytes(level, reply->readAll());
                if (!pix.isNull()) {
                    m_cache.put(key, pix);
                    if (level < m_levels.size()) {
                        const auto& l = m_levels[level];
                        const qreal D = (qreal)m_levels[0].w / l.w;
                        scene()->update(QRectF((qreal)col * l.tileW * D,
                                               (qreal)row * l.tileH * D,
                                               l.tileW * D, l.tileH * D));
                    }
                }
            });
            return {};
        }

        QPixmap pix = loadTile(level, col, row);
        if (!pix.isNull())
            m_cache.put(key, pix);
        return pix;
    }

    QPixmap loadTileFromBytes(int /*level*/, const QByteArray& bytes) {
        if (bytes.isEmpty()) return {};
        QImage src;
        if (!src.loadFromData(bytes)) return {};
        // Apply LUT (gray → mapped color)
        QImage img = src.convertToFormat(QImage::Format_ARGB32);
        for (int r = 0; r < img.height(); ++r) {
            auto* dst = reinterpret_cast<uint32_t*>(img.scanLine(r));
            for (int c = 0; c < img.width(); ++c) {
                uint8_t gray = (uint8_t)qRed(dst[c]);
                QRgb    mapped = m_lut[gray];
                dst[c] = (0xFFu << 24)
                       | ((uint32_t)qRed(mapped)   << 16)
                       | ((uint32_t)qGreen(mapped) <<  8)
                       |  (uint32_t)qBlue(mapped);
            }
        }
        return QPixmap::fromImage(std::move(img));
    }

    QPixmap loadTile(int level, int col, int row) {
        if (!m_tiff) return {};
        const auto& lvl = m_levels[level];

        TIFFSetDirectory(m_tiff, (uint16_t)lvl.dirIdx);

        if (lvl.tiled) {
            // TIFFReadRGBATile returns bottom-up ABGR (libtiff convention)
            std::vector<uint32_t> raster((size_t)lvl.tileW * lvl.tileH);
            uint32_t x = (uint32_t)col * lvl.tileW;
            uint32_t y = (uint32_t)row * lvl.tileH;
            if (!TIFFReadRGBATile(m_tiff, x, y, raster.data()))
                return {};

            QImage img((int)lvl.tileW, (int)lvl.tileH, QImage::Format_ARGB32);
            for (uint32_t r = 0; r < lvl.tileH; ++r) {
                auto* dst = reinterpret_cast<uint32_t*>(img.scanLine((int)r));
                // libtiff rows are bottom-up → flip
                const uint32_t* src = raster.data() + (lvl.tileH - 1 - r) * lvl.tileW;
                for (uint32_t c = 0; c < lvl.tileW; ++c) {
                    uint32_t abgr = src[c];
                    uint8_t  gray = TIFFGetR(abgr);  // R=G=B for grayscale
                    QRgb     mapped = m_lut[gray];
                    dst[c] = (0xFFu << 24)
                           | ((uint32_t)qRed(mapped)   << 16)
                           | ((uint32_t)qGreen(mapped) <<  8)
                           |  (uint32_t)qBlue(mapped);
                }
            }
            return QPixmap::fromImage(std::move(img));
        } else {
            // Stripped: read whole level as RGBA (only for small coarse levels)
            std::vector<uint32_t> raster((size_t)lvl.w * lvl.h);
            if (!TIFFReadRGBAImageOriented(m_tiff, lvl.w, lvl.h, raster.data(),
                                           ORIENTATION_TOPLEFT, 0))
                return {};

            QImage img((int)lvl.w, (int)lvl.h, QImage::Format_ARGB32);
            for (uint32_t r = 0; r < lvl.h; ++r) {
                auto* dst = reinterpret_cast<uint32_t*>(img.scanLine((int)r));
                const uint32_t* src = raster.data() + r * lvl.w;
                for (uint32_t c = 0; c < lvl.w; ++c) {
                    uint32_t abgr = src[c];
                    uint8_t  gray = TIFFGetR(abgr);
                    QRgb     mapped = m_lut[gray];
                    dst[c] = (0xFFu << 24)
                           | ((uint32_t)qRed(mapped)   << 16)
                           | ((uint32_t)qGreen(mapped) <<  8)
                           |  (uint32_t)qBlue(mapped);
                }
            }
            return QPixmap::fromImage(std::move(img));
        }
    }

public:
    QString lastSaveError() const { return m_lastSaveError; }

    // ── save BigTIFF pyramid ──────────────────────────────────────────────────
    // progress(pct 0-100) → return false to cancel
    bool saveTiff(const QString& outputPath,
                  std::function<bool(int)> progress = {})
    {
        m_lastSaveError.clear();
        if (m_webMode) {
            m_lastSaveError = tr("Save is not supported for web-based BigTIFF.");
            return false;
        }
        if (!m_tiff || m_levels.isEmpty()) return false;

        // Check whether current LUT is identity (gray → gray)
        bool applyLut = false;
        for (int i = 0; i < 256 && !applyLut; ++i)
            if (m_lut[i] != qRgb(i, i, i)) applyLut = true;

        // Fast path: identity LUT → delegate to the Qt-Widgets-free copy function
        // so the same code path is used by the GUI Save As button and batch mode.
        if (!applyLut) {
            // Close our read handle first; bigTiffCopyPyramid opens the file itself.
            const QString srcPath = m_srcPath;
            bool ok = bigTiffCopyPyramid(srcPath, outputPath, progress, &m_lastSaveError);
            return ok;
        }

        // Slow path: non-identity LUT — apply per tile and write RGB output.
        TIFF* out = TIFFOpen(outputPath.toLocal8Bit().constData(), "w8");
        if (!out) {
            m_lastSaveError = tr("Could not create output file.");
            return false;
        }
        if (!TIFFIsBigTIFF(out)) {
            TIFFClose(out);
            QFile::remove(outputPath);
            m_lastSaveError = tr(
                "The libtiff library loaded at runtime does not support BigTIFF "
                "write mode (\"w8\"). Please ensure the correct libtiff ≥ 4.0 "
                "is linked (check rpath / LD_LIBRARY_PATH).");
            return false;
        }

        int totalTiles = 0;
        for (const auto& lvl : m_levels) {
            int tw = lvl.tiled ? (int)lvl.tileW : 256;
            int th = lvl.tiled ? (int)lvl.tileH : 256;
            totalTiles += ((int(lvl.w) + tw - 1) / tw) * ((int(lvl.h) + th - 1) / th);
        }
        int done = 0;
        bool cancelled = false;

        auto reportProgress = [&](int pct) {
            if (progress && !progress(pct)) cancelled = true;
        };

        for (int li = 0; li < m_levels.size() && !cancelled; ++li) {
            const auto& lvl = m_levels[li];
            TIFFSetDirectory(m_tiff, (uint16_t)lvl.dirIdx);

            uint16_t srcBps = 8, srcSpp = 1;
            uint16_t srcPhoto = PHOTOMETRIC_MINISBLACK;
            TIFFGetField(m_tiff, TIFFTAG_BITSPERSAMPLE,   &srcBps);
            TIFFGetField(m_tiff, TIFFTAG_SAMPLESPERPIXEL, &srcSpp);
            TIFFGetField(m_tiff, TIFFTAG_PHOTOMETRIC,     &srcPhoto);

            uint32_t outTW  = lvl.tiled ? lvl.tileW : 256;
            uint32_t outTH  = lvl.tiled ? lvl.tileH : 256;
            // LUT maps gray → RGB
            uint16_t outSpp   = 3;
            uint16_t outPhoto = PHOTOMETRIC_RGB;

            TIFFSetField(out, TIFFTAG_IMAGEWIDTH,      lvl.w);
            TIFFSetField(out, TIFFTAG_IMAGELENGTH,     lvl.h);
            TIFFSetField(out, TIFFTAG_TILEWIDTH,       outTW);
            TIFFSetField(out, TIFFTAG_TILELENGTH,      outTH);
            TIFFSetField(out, TIFFTAG_BITSPERSAMPLE,   (uint16_t)8);
            TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, outSpp);
            TIFFSetField(out, TIFFTAG_PHOTOMETRIC,     outPhoto);
            TIFFSetField(out, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
            TIFFSetField(out, TIFFTAG_COMPRESSION,     COMPRESSION_DEFLATE);
            TIFFSetField(out, TIFFTAG_PREDICTOR,       PREDICTOR_HORIZONTAL);
            if (li > 0)
                TIFFSetField(out, TIFFTAG_SUBFILETYPE, (uint32_t)FILETYPE_REDUCEDIMAGE);

            int tilesX = (int(lvl.w) + int(outTW) - 1) / int(outTW);
            int tilesY = (int(lvl.h) + int(outTH) - 1) / int(outTH);

            std::vector<uint32_t> abgrBuf((size_t)outTW * outTH);
            std::vector<uint8_t>  outBuf((size_t)outTW * outTH * outSpp, 0);

            if (lvl.tiled) {
                for (int ty = 0; ty < tilesY && !cancelled; ++ty) {
                    for (int tx = 0; tx < tilesX && !cancelled; ++tx) {
                        uint32_t x = (uint32_t)tx * outTW;
                        uint32_t y = (uint32_t)ty * outTH;
                        uint32_t validW = qMin(outTW, lvl.w - x);
                        uint32_t validH = qMin(outTH, lvl.h - y);
                        TIFFReadRGBATile(m_tiff, x, y, abgrBuf.data());
                        std::fill(outBuf.begin(), outBuf.end(), 0);
                        for (uint32_t row = 0; row < validH; ++row) {
                            uint32_t srcRow = outTH - 1 - row; // RGBA tile is bottom-up
                            for (uint32_t col = 0; col < validW; ++col) {
                                uint32_t abgr = abgrBuf[(size_t)srcRow * outTW + col];
                                uint8_t  gray = TIFFGetR(abgr);
                                QRgb m = m_lut[gray];
                                size_t idx = ((size_t)row * outTW + col) * 3;
                                outBuf[idx]   = (uint8_t)qRed(m);
                                outBuf[idx+1] = (uint8_t)qGreen(m);
                                outBuf[idx+2] = (uint8_t)qBlue(m);
                            }
                        }
                        TIFFWriteTile(out, outBuf.data(), x, y, 0, 0);
                        reportProgress(++done * 100 / totalTiles);
                    }
                    QApplication::processEvents();
                }
            } else {
                // Strip-based level
                std::vector<uint32_t> raster((size_t)lvl.w * lvl.h);
                TIFFReadRGBAImageOriented(m_tiff, lvl.w, lvl.h,
                                          raster.data(), ORIENTATION_TOPLEFT, 0);
                for (int ty = 0; ty < tilesY && !cancelled; ++ty) {
                    for (int tx = 0; tx < tilesX && !cancelled; ++tx) {
                        uint32_t x0 = (uint32_t)tx * outTW;
                        uint32_t y0 = (uint32_t)ty * outTH;
                        uint32_t tw = qMin(outTW, lvl.w - x0);
                        uint32_t th = qMin(outTH, lvl.h - y0);
                        std::fill(outBuf.begin(), outBuf.end(), 0);
                        for (uint32_t r = 0; r < th; ++r) {
                            for (uint32_t c = 0; c < tw; ++c) {
                                uint32_t abgr = raster[(y0 + r) * lvl.w + (x0 + c)];
                                uint8_t  gray = TIFFGetR(abgr);
                                QRgb m = m_lut[gray];
                                size_t idx = ((size_t)r * outTW + c) * 3;
                                outBuf[idx]   = (uint8_t)qRed(m);
                                outBuf[idx+1] = (uint8_t)qGreen(m);
                                outBuf[idx+2] = (uint8_t)qBlue(m);
                            }
                        }
                        TIFFWriteTile(out, outBuf.data(), x0, y0, 0, 0);
                        reportProgress(++done * 100 / totalTiles);
                    }
                    QApplication::processEvents();
                }
            }

            TIFFWriteDirectory(out);
        }

        TIFFClose(out);
        if (cancelled) {
            QFile::remove(outputPath);
            return false;
        }
        return true;
    }

    TIFF*                 m_tiff       = nullptr;
    bool                  m_webMode    = false;
    bool                  m_fitted     = false;
    QString               m_webBaseUrl;
    QNetworkAccessManager m_nam;
    QSet<quint64>         m_pending;
    QVector<BigTiffLevel> m_levels;
    TileCache             m_cache;
    QVector<QRgb>         m_baseLut;
    QVector<QRgb>         m_lut;
    int                   m_brightness = 0;
    int                   m_contrast   = 0;
    QString               m_lastSaveError;
    QString               m_srcPath;
};

// ── BigTiffViewer ─────────────────────────────────────────────────────────────
BigTiffViewer::BigTiffViewer(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // toolbar
    auto* toolbar = new QToolBar(this);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setObjectName("bigTiffToolbar");
    toolbar->setStyleSheet("#bigTiffToolbar { background-color: #484848; border-bottom: 1px solid #2a2a2a; }");

    auto makePushBtn = [&](const QString& label) {
        auto* btn = new QPushButton(label, toolbar);
        btn->setFixedHeight(26);
        btn->setFocusPolicy(Qt::NoFocus);
        toolbar->addWidget(btn);
        return btn;
    };

    auto* closeBtn   = makePushBtn(tr("Close"));
    toolbar->addSeparator();
    auto* saveAsBtn  = makePushBtn(tr("Save As…"));
    toolbar->addSeparator();
    auto* zoomInBtn  = makePushBtn(tr("+"));
    auto* zoomOutBtn = makePushBtn(tr("−"));
    auto* fitBtn     = makePushBtn(tr("Fit"));
    toolbar->addSeparator();

    m_infoLabel = new QLabel(toolbar);
    m_infoLabel->setStyleSheet("background-color: transparent; color: #ccc; padding: 0 6px;");
    toolbar->addWidget(m_infoLabel);

    // view
    m_view = new BigTiffGraphicsView(this);

    layout->addWidget(toolbar);
    layout->addWidget(m_view);

    connect(closeBtn,   &QPushButton::clicked, this, &BigTiffViewer::closeRequested);
    connect(saveAsBtn,  &QPushButton::clicked, this, &BigTiffViewer::saveAs);
    connect(zoomInBtn,  &QPushButton::clicked, this, &BigTiffViewer::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &BigTiffViewer::zoomOut);
    connect(fitBtn,     &QPushButton::clicked, this, &BigTiffViewer::fitView);
}

BigTiffViewer::~BigTiffViewer()
{
    m_view->closeTiff();
}

bool BigTiffViewer::open(const QString& path)
{
    m_filePath = path;
    TIFFSetWarningHandler(nullptr);

    bool ok;
    if (path.startsWith("http://") || path.startsWith("https://"))
        ok = openWebMode(path);
    else
        ok = m_view->openTiff(path);

    if (ok) {
        updateInfoLabel();
        m_view->onDebugInfo = [this](int dirIdx, qreal scale) {
            const auto& levels = m_view->levels();
            if (levels.isEmpty()) return;
            const auto& full = levels.first();
            m_infoLabel->setText(
                QString("%1 × %2 px  |  %3 level%4%5  |  dir=%6  zoom=×%7")
                    .arg(full.w).arg(full.h)
                    .arg(levels.size())
                    .arg(levels.size() != 1 ? "s" : "")
                    .arg(m_view->isBigTiff() ? "  |  BigTIFF" : "")
                    .arg(dirIdx)
                    .arg(scale, 0, 'f', 4));
        };
        m_view->onScaleChanged      = [this](double s) { emit scaleChanged(s); };
        m_view->onCursorPos         = [this](int x, int y) { emit cursorPositionChanged(x, y); };
        m_view->onCursorColor       = [this](QColor c) { emit cursorColorChanged(c); };
        m_view->onViewportChanged   = [this](QRectF vis, QRectF full) { emit viewportChanged(vis, full); };
        QTimer::singleShot(0, m_view, [this]{ m_view->fitAll(); });
    }
    return ok;
}

bool BigTiffViewer::openWebMode(const QString& url)
{
    QNetworkAccessManager nam;
    QUrl metaQUrl(url);
    QNetworkRequest req(metaQUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    QNetworkReply* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::sslErrors,
                     reply, [reply](const QList<QSslError>&){ reply->ignoreSslErrors(); });
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return false;
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    const QJsonObject root = QJsonDocument::fromJson(data).object();
    const QJsonObject ts   = root.value("tissuescope").toObject();
    const QJsonArray  dirs = ts.value("Directories").toArray();
    if (dirs.isEmpty()) return false;

    QVector<BigTiffLevel> levels;
    for (const QJsonValue& v : dirs) {
        const QJsonObject d = v.toObject();
        BigTiffLevel lvl;
        lvl.dirIdx = d.value("PageNumber").toString().toInt();
        lvl.w      = d.value("ImageWidth").toString().toUInt();
        lvl.h      = d.value("ImageLength").toString().toUInt();
        lvl.tileW  = d.value("TileWidth").toString().toUInt();
        lvl.tileH  = d.value("TileLength").toString().toUInt();
        lvl.tiled  = (lvl.tileW > 0 && lvl.tileH > 0);
        if (!lvl.tiled) { lvl.tileW = lvl.w; lvl.tileH = lvl.h; }
        if (lvl.w > 0 && lvl.h > 0) levels.append(lvl);
    }
    if (levels.isEmpty()) return false;

    std::sort(levels.begin(), levels.end(), [](const BigTiffLevel& a, const BigTiffLevel& b){
        return a.w > b.w;
    });

    // Replace TIFF level dimensions with the server's actual pixel dimensions at each
    // directory. The server serves ceil(finest / 2^(dir-1)) pixels at directory dir,
    // which may differ from the stored TIFF level dimensions.
    {
        const uint32_t fw    = levels[0].w;
        const uint32_t fh    = levels[0].h;
        const int      maxN  = (int)std::floor(std::log2((double)qMax(fw, fh) / 256.0));
        for (auto& lvl : levels) {
            const int rawDir  = (int)std::floor(std::log2((double)qMax(lvl.w, lvl.h) / 256.0));
            const int dir     = 1 + maxN - rawDir;
            const int scale   = 1 << dir;                      // 2^dir
            lvl.w     = (fw + (uint32_t)scale - 1) / (uint32_t)scale; // ceil(fw / scale)
            lvl.h     = (fh + (uint32_t)scale - 1) / (uint32_t)scale;
            lvl.tileW = 256;
            lvl.tileH = 256;
            lvl.tiled = true;
        }
    }

    return m_view->openWeb(url, levels);
}

void BigTiffViewer::closeTiff()
{
    m_view->closeTiff();
    m_filePath.clear();
    m_infoLabel->clear();
}

bool BigTiffViewer::isOpen() const
{
    return m_view->isOpen();
}

void BigTiffViewer::zoomIn()  { m_view->zoomBy(1.25); }
void BigTiffViewer::zoomOut() { m_view->zoomBy(1.0 / 1.25); }
void BigTiffViewer::fitView() { m_view->fitAll(); }

void BigTiffViewer::setColorTable(const QVector<QRgb>& lut) { m_view->setColorTable(lut); }
void BigTiffViewer::setBrightness(int v) { m_view->setBrightness(v); }
void BigTiffViewer::setContrast(int v)   { m_view->setContrast(v); }
void BigTiffViewer::centerOn(const QPointF& scenePos) { m_view->centerOn(scenePos); m_view->emitViewport(); }

QSize BigTiffViewer::imageSize() const
{
    const auto& lvls = m_view->levels();
    if (lvls.isEmpty()) return {};
    return QSize((int)lvls.first().w, (int)lvls.first().h);
}

void BigTiffViewer::updateInfoLabel()
{
    const auto& levels = m_view->levels();
    if (levels.isEmpty()) { m_infoLabel->clear(); return; }
    const auto& full = levels.first();
    QString info = QString("%1 × %2 px  |  %3 level%4%5")
        .arg(full.w).arg(full.h)
        .arg(levels.size())
        .arg(levels.size() != 1 ? "s" : "")
        .arg(m_view->isBigTiff() ? "  |  BigTIFF" : "");
    m_infoLabel->setText(info);
}

bool BigTiffViewer::saveTiff(const QString& outputPath)
{
    QProgressDialog progress(tr("Saving BigTIFF pyramid…"), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    progress.setValue(0);

    bool ok = m_view->saveTiff(outputPath, [&](int pct) -> bool {
        progress.setValue(pct);
        return !progress.wasCanceled();
    });

    progress.setValue(100);
    return ok;
}

void BigTiffViewer::saveAs()
{
    if (!m_view->isOpen()) return;

    const QString suggestedDir = QFileInfo(m_filePath).absolutePath();
    QString path = QFileDialog::getSaveFileName(
        this,
        tr("Save BigTIFF As…"),
        suggestedDir,
        tr("TIFF Files (*.tif *.tiff);;All Files (*)"));
    if (path.isEmpty()) return;

    if (!path.endsWith(".tif", Qt::CaseInsensitive) &&
        !path.endsWith(".tiff", Qt::CaseInsensitive))
        path += ".tif";

    if (!saveTiff(path)) {
        const QString detail = m_view->lastSaveError();
        const QString msg = detail.isEmpty()
            ? tr("Could not write BigTIFF to:\n%1").arg(path)
            : tr("Could not write BigTIFF to:\n%1\n\n%2").arg(path, detail);
        QMessageBox::warning(this, tr("Save Failed"), msg);
    }
}
