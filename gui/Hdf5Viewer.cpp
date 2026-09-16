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

#include "Hdf5Viewer.h"

#include <hdf5.h>

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QPixmap>
#include <QImage>
#include <QVBoxLayout>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QScreen>
#include <QGuiApplication>

#include <vector>
#include <algorithm>
#include <cstring>

// ── tile cache ────────────────────────────────────────────────────────────────
// HDF5 chunks are large (2048×2048), keep fewer cached tiles
static constexpr int MAX_HDF5_CACHED_TILES = 16;

struct Hdf5TileCache {
    static quint64 makeKey(int level, int col, int row)
    { return ((quint64)(uint8_t)level << 40) | ((quint64)(uint32_t)row << 20) | (quint64)(uint32_t)col; }

    const QPixmap* get(quint64 k) const {
        auto it = m_map.find(k);
        return it != m_map.end() ? &it->pixmap : nullptr;
    }
    void put(quint64 k, const QPixmap& pix) {
        if (m_map.contains(k)) return;
        if (m_map.size() >= MAX_HDF5_CACHED_TILES) {
            m_map.remove(m_order.takeFirst());
        }
        m_map.insert(k, {pix});
        m_order.append(k);
    }
    void clear() { m_map.clear(); m_order.clear(); }

private:
    struct Entry { QPixmap pixmap; };
    QHash<quint64, Entry> m_map;
    QList<quint64>        m_order;
};

// ── Hdf5GraphicsView ──────────────────────────────────────────────────────────
class Hdf5GraphicsView : public QGraphicsView
{
public:
    explicit Hdf5GraphicsView(QWidget* parent = nullptr)
        : QGraphicsView(parent)
    {
        setDragMode(QGraphicsView::ScrollHandDrag);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setRenderHint(QPainter::SmoothPixmapTransform, false);
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
        setBackgroundBrush(QColor(0x28, 0x28, 0x28));
        setScene(new QGraphicsScene(this));

        // identity LUT
        m_lut.resize(256);
        for (int i = 0; i < 256; ++i) m_lut[i] = qRgb(i, i, i);
        m_lutIsIdentity = true;

        // suppress HDF5 error output
        H5Eset_auto2(H5E_DEFAULT, nullptr, nullptr);
    }

    ~Hdf5GraphicsView() { closeFile(); }

    // ── open / close ─────────────────────────────────────────────────────────
    bool openFile(const QString& path) {
        closeFile();
        m_fileId = H5Fopen(path.toLocal8Bit().constData(), H5F_ACC_RDONLY, H5P_DEFAULT);
        if (m_fileId < 0) return false;

        QVector<Hdf5Level> raw;

        // Scan /pyramid/00, /pyramid/01, ...
        for (int i = 0; i <= 99; ++i) {
            QString dpath = QString("/pyramid/%1").arg(i, 2, 10, QChar('0'));
            hid_t did = H5Dopen2(m_fileId, dpath.toLocal8Bit().constData(), H5P_DEFAULT);
            if (did < 0) break;

            Hdf5Level lvl;
            lvl.datasetPath = dpath;
            if (!readLevelMeta(did, lvl)) { H5Dclose(did); break; }
            H5Dclose(did);
            if (lvl.rows > 0 && lvl.cols > 0)
                raw.append(lvl);
        }

        // Fallback: /Image directly
        if (raw.isEmpty()) {
            hid_t did = H5Dopen2(m_fileId, "/Image", H5P_DEFAULT);
            if (did >= 0) {
                Hdf5Level lvl;
                lvl.datasetPath = "/Image";
                if (readLevelMeta(did, lvl) && lvl.rows > 0 && lvl.cols > 0)
                    raw.append(lvl);
                H5Dclose(did);
            }
        }

        if (raw.isEmpty()) { H5Fclose(m_fileId); m_fileId = -1; return false; }

        // sort finest → coarsest
        std::sort(raw.begin(), raw.end(), [](const Hdf5Level& a, const Hdf5Level& b){
            return a.cols > b.cols;
        });
        m_levels = raw;

        scene()->setSceneRect(0, 0, m_levels[0].cols, m_levels[0].rows);
        m_cache.clear();
        return true;
    }

    void closeFile() {
        if (m_fileId >= 0) { H5Fclose(m_fileId); m_fileId = -1; }
        m_levels.clear();
        m_cache.clear();
        scene()->setSceneRect(QRectF());
    }

    bool isOpen() const { return m_fileId >= 0; }

    const QVector<Hdf5Level>& levels() const { return m_levels; }

    void setColorTable(const QVector<QRgb>& lut) {
        m_lut = lut;
        // check identity: Original maps i → qRgb(i,i,i)
        m_lutIsIdentity = true;
        for (int i = 0; i < 256; ++i) {
            if (m_lut[i] != qRgb(i, i, i)) { m_lutIsIdentity = false; break; }
        }
        m_cache.clear();
        scene()->update();
    }

    void zoomBy(qreal f) { scale(f, f); }
    void fitAll() {
        if (m_levels.isEmpty()) return;
        fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    }

protected:
    // ── tile-based background rendering ──────────────────────────────────────
    void drawBackground(QPainter* painter, const QRectF& exposed) override {
        QGraphicsView::drawBackground(painter, exposed);
        if (m_fileId < 0 || m_levels.isEmpty()) return;

        const int level = bestLevel(transform().m11());
        const auto& lvl = m_levels[level];
        const qreal D   = (qreal)m_levels[0].cols / (qreal)lvl.cols;

        // exposed in level-pixel space
        const qreal lx0 = exposed.left()   / D;
        const qreal ly0 = exposed.top()    / D;
        const qreal lx1 = exposed.right()  / D;
        const qreal ly1 = exposed.bottom() / D;

        const int64_t tileX0 = qMax((int64_t)0, (int64_t)(lx0 / lvl.chunkCols));
        const int64_t tileX1 = qMin((lvl.cols-1) / lvl.chunkCols, (int64_t)(lx1 / lvl.chunkCols));
        const int64_t tileY0 = qMax((int64_t)0, (int64_t)(ly0 / lvl.chunkRows));
        const int64_t tileY1 = qMin((lvl.rows-1) / lvl.chunkRows, (int64_t)(ly1 / lvl.chunkRows));

        for (int64_t ty = tileY0; ty <= tileY1; ++ty) {
            for (int64_t tx = tileX0; tx <= tileX1; ++tx) {
                QPixmap pix = fetchTile(level, (int)tx, (int)ty);
                if (pix.isNull()) continue;

                const int64_t colLeft = tx * lvl.chunkCols;
                const int64_t rowTop  = ty * lvl.chunkRows;
                const int64_t validW  = qMin(lvl.chunkCols, lvl.cols - colLeft);
                const int64_t validH  = qMin(lvl.chunkRows, lvl.rows - rowTop);

                QRectF dest(colLeft*D, rowTop*D, validW*D, validH*D);
                painter->drawPixmap(dest, pix, QRectF(0, 0, validW, validH));
            }
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        scale(e->angleDelta().y() > 0 ? 1.25 : 1.0/1.25, e->angleDelta().y() > 0 ? 1.25 : 1.0/1.25);
        QGraphicsView::wheelEvent(e);
    }

private:
    // ── level metadata ────────────────────────────────────────────────────────
    bool readLevelMeta(hid_t did, Hdf5Level& lvl) {
        hid_t sid = H5Dget_space(did);
        if (sid < 0) return false;

        int ndims = H5Sget_simple_extent_ndims(sid);
        hsize_t dims[3] = {};
        H5Sget_simple_extent_dims(sid, dims, nullptr);
        H5Sclose(sid);

        if (ndims < 2) return false;
        lvl.rows     = (int64_t)dims[0];
        lvl.cols     = (int64_t)dims[1];
        lvl.channels = (ndims >= 3 && dims[2] > 0) ? (int)dims[2] : 1;

        // chunk dimensions
        hid_t pid = H5Dget_create_plist(did);
        if (pid >= 0) {
            if (H5Pget_layout(pid) == H5D_CHUNKED) {
                hsize_t cdims[3] = {};
                H5Pget_chunk(pid, ndims, cdims);
                lvl.chunkRows = (int64_t)cdims[0];
                lvl.chunkCols = (int64_t)cdims[1];
            } else {
                // contiguous storage: treat whole dataset as one tile
                lvl.chunkRows = lvl.rows;
                lvl.chunkCols = lvl.cols;
            }
            H5Pclose(pid);
        }
        return true;
    }

    // ── level selection ───────────────────────────────────────────────────────
    int bestLevel(qreal viewScale) const {
        for (int i = 0; i < m_levels.size(); ++i) {
            qreal downscale = (qreal)m_levels[0].cols / (qreal)m_levels[i].cols;
            if (downscale * viewScale >= 1.0)
                return i;
        }
        return m_levels.size() - 1;
    }

    // ── tile fetching ─────────────────────────────────────────────────────────
    QPixmap fetchTile(int level, int col, int row) {
        quint64 key = Hdf5TileCache::makeKey(level, col, row);
        if (const QPixmap* cached = m_cache.get(key)) return *cached;
        QPixmap pix = loadTile(level, col, row);
        if (!pix.isNull()) m_cache.put(key, pix);
        return pix;
    }

    QPixmap loadTile(int level, int tileCol, int tileRow) {
        if (m_fileId < 0) return {};
        const auto& lvl = m_levels[level];

        const int64_t rowStart = (int64_t)tileRow * lvl.chunkRows;
        const int64_t colStart = (int64_t)tileCol * lvl.chunkCols;
        const int64_t nRows    = qMin(lvl.chunkRows, lvl.rows - rowStart);
        const int64_t nCols    = qMin(lvl.chunkCols, lvl.cols - colStart);
        if (nRows <= 0 || nCols <= 0) return {};

        hid_t did = H5Dopen2(m_fileId, lvl.datasetPath.toLocal8Bit().constData(), H5P_DEFAULT);
        if (did < 0) return {};

        hid_t fspace = H5Dget_space(did);
        hsize_t offset[3] = {(hsize_t)rowStart, (hsize_t)colStart, 0};
        hsize_t count[3]  = {(hsize_t)nRows,    (hsize_t)nCols,    (hsize_t)lvl.channels};
        H5Sselect_hyperslab(fspace, H5S_SELECT_SET, offset, nullptr, count, nullptr);

        hsize_t memdims[3] = {(hsize_t)nRows, (hsize_t)nCols, (hsize_t)lvl.channels};
        hid_t mspace = H5Screate_simple(3, memdims, nullptr);

        std::vector<uint8_t> buf((size_t)nRows * nCols * lvl.channels);
        herr_t err = H5Dread(did, H5T_NATIVE_UCHAR, mspace, fspace, H5P_DEFAULT, buf.data());

        H5Sclose(mspace);
        H5Sclose(fspace);
        H5Dclose(did);
        if (err < 0) return {};

        QImage img((int)nCols, (int)nRows, QImage::Format_RGB32);
        const int ch = lvl.channels;

        for (int r = 0; r < (int)nRows; ++r) {
            auto* dst = reinterpret_cast<uint32_t*>(img.scanLine(r));
            const uint8_t* src = buf.data() + (size_t)r * nCols * ch;
            for (int c = 0; c < (int)nCols; ++c) {
                if (ch >= 3) {
                    const uint8_t R = src[c*ch],  G = src[c*ch+1], B = src[c*ch+2];
                    if (m_lutIsIdentity) {
                        dst[c] = 0xFF000000u | ((uint32_t)R<<16) | ((uint32_t)G<<8) | B;
                    } else {
                        const uint8_t gray = (uint8_t)(0.299f*R + 0.587f*G + 0.114f*B + 0.5f);
                        const QRgb mapped  = m_lut[gray];
                        dst[c] = 0xFF000000u | ((uint32_t)qRed(mapped)<<16)
                               | ((uint32_t)qGreen(mapped)<<8) | (uint32_t)qBlue(mapped);
                    }
                } else {
                    const uint8_t gray = src[c];
                    const QRgb mapped  = m_lut[gray];
                    dst[c] = 0xFF000000u | ((uint32_t)qRed(mapped)<<16)
                           | ((uint32_t)qGreen(mapped)<<8) | (uint32_t)qBlue(mapped);
                }
            }
        }
        return QPixmap::fromImage(std::move(img));
    }

    hid_t              m_fileId = -1;
    QVector<Hdf5Level> m_levels;
    Hdf5TileCache      m_cache;
    QVector<QRgb>      m_lut;
    bool               m_lutIsIdentity = true;
};

// ── Hdf5Viewer ────────────────────────────────────────────────────────────────
Hdf5Viewer::Hdf5Viewer(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* toolbar = new QToolBar(this);
    toolbar->setObjectName("hdf5Toolbar");
    toolbar->setStyleSheet("#hdf5Toolbar { background-color: #484848; border-bottom: 1px solid #2a2a2a; }");

    auto makeBtn = [&](const QString& label) {
        auto* btn = new QPushButton(label, toolbar);
        btn->setFixedHeight(26);
        btn->setFocusPolicy(Qt::NoFocus);
        toolbar->addWidget(btn);
        return btn;
    };

    auto* closeBtn   = makeBtn(tr("Close"));
    toolbar->addSeparator();
    auto* zoomInBtn  = makeBtn(tr("+"));
    auto* zoomOutBtn = makeBtn(tr("−"));
    auto* fitBtn     = makeBtn(tr("Fit"));
    toolbar->addSeparator();

    m_infoLabel = new QLabel(toolbar);
    m_infoLabel->setStyleSheet("background-color: transparent; color: #ccc; padding: 0 6px;");
    toolbar->addWidget(m_infoLabel);

    m_view = new Hdf5GraphicsView(this);
    layout->addWidget(toolbar);
    layout->addWidget(m_view);

    connect(closeBtn,   &QPushButton::clicked, this, &Hdf5Viewer::closeRequested);
    connect(zoomInBtn,  &QPushButton::clicked, this, &Hdf5Viewer::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &Hdf5Viewer::zoomOut);
    connect(fitBtn,     &QPushButton::clicked, this, &Hdf5Viewer::fitView);
}

Hdf5Viewer::~Hdf5Viewer() { m_view->closeFile(); }

bool Hdf5Viewer::open(const QString& path)
{
    m_filePath = path;
    if (!m_view->openFile(path)) return false;
    updateInfoLabel();
    QTimer::singleShot(0, m_view, [this]{ m_view->fitAll(); });
    return true;
}

void Hdf5Viewer::closeFile()
{
    m_view->closeFile();
    m_filePath.clear();
    m_infoLabel->clear();
}

bool Hdf5Viewer::isOpen() const { return m_view->isOpen(); }

QSize Hdf5Viewer::imageSize() const
{
    const auto& lvls = m_view->levels();
    if (lvls.isEmpty()) return {};
    return QSize((int)lvls.first().cols, (int)lvls.first().rows);
}

void Hdf5Viewer::setColorTable(const QVector<QRgb>& lut)
{
    m_view->setColorTable(lut);
}

void Hdf5Viewer::zoomIn()  { m_view->zoomBy(1.25); }
void Hdf5Viewer::zoomOut() { m_view->zoomBy(1.0/1.25); }
void Hdf5Viewer::fitView() { m_view->fitAll(); }

void Hdf5Viewer::updateInfoLabel()
{
    const auto& lvls = m_view->levels();
    if (lvls.isEmpty()) { m_infoLabel->clear(); return; }
    const auto& full = lvls.first();
    m_infoLabel->setText(QString("%1 × %2 px  |  %3 ch  |  %4 levels  |  chunk %5×%6")
        .arg(full.cols).arg(full.rows)
        .arg(full.channels)
        .arg(lvls.size())
        .arg(full.chunkCols).arg(full.chunkRows));
}
