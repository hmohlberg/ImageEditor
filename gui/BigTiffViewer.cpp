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
#include <QVector>
#include <QTimer>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>

#include <functional>

#include <vector>
#include <algorithm>
#include <cstring>

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

        auto* scene = new QGraphicsScene(this);
        setScene(scene);

        // default identity LUT (gray → gray)
        m_lut.resize(256);
        for (int i = 0; i < 256; ++i)
            m_lut[i] = qRgb(i, i, i);
    }

    void setColorTable(const QVector<QRgb>& lut) {
        m_lut = lut;
        m_cache.clear();          // cached tiles used the old LUT → invalidate
        scene()->update();        // trigger redraw
    }

    ~BigTiffGraphicsView() { closeTiff(); }

    // ── open / close ─────────────────────────────────────────────────────────
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
        m_levels.clear();
        m_cache.clear();
        m_srcPath.clear();
        scene()->setSceneRect(QRectF());
    }

    bool isOpen() const { return m_tiff != nullptr; }

    const QVector<BigTiffLevel>& levels() const { return m_levels; }
    bool isBigTiff() const { return m_tiff && TIFFIsBigTIFF(m_tiff); }

    // ── zoom helpers ─────────────────────────────────────────────────────────
    void zoomBy(qreal factor) { scale(factor, factor); }

    void fitAll() {
        if (m_levels.isEmpty()) return;
        fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    }

protected:
    // ── tile-based background rendering ──────────────────────────────────────
    void drawBackground(QPainter* painter, const QRectF& exposed) override {
        QGraphicsView::drawBackground(painter, exposed);
        if (!m_tiff || m_levels.isEmpty()) return;

        const int level  = bestLevel(transform().m11());
        const auto& lvl  = m_levels[level];
        const qreal D    = (qreal)m_levels[0].w / lvl.w;  // scene units per level pixel

        // Map exposed scene rect to level pixel space
        const qreal lx0 = exposed.left()   / D;
        const qreal ly0 = exposed.top()    / D;
        const qreal lx1 = exposed.right()  / D;
        const qreal ly1 = exposed.bottom() / D;

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

        QPixmap pix = loadTile(level, col, row);
        if (!pix.isNull())
            m_cache.put(key, pix);
        return pix;
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

    TIFF*                 m_tiff   = nullptr;
    QVector<BigTiffLevel> m_levels;
    TileCache             m_cache;
    QVector<QRgb>         m_lut;
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
    // suppress libtiff non-fatal tag-order warnings
    TIFFSetWarningHandler(nullptr);

    bool ok = m_view->openTiff(path);
    if (ok) {
        updateInfoLabel();
        // fitInView only works after the widget has been laid out and shown
        QTimer::singleShot(0, m_view, [this]{ m_view->fitAll(); });
    }
    return ok;
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

void BigTiffViewer::setColorTable(const QVector<QRgb>& lut)
{
    m_view->setColorTable(lut);
}

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
