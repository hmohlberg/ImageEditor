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
*
*/

#include "HistogramWidget.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QSlider>
#include <QLabel>
#include <QPainter>
#include <QImage>
#include <cmath>

// ---- Canvas ----------------------------------------------------------------

HistogramWidget::Canvas::Canvas(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    bins.fill(0);
}

// ---- ColorBar --------------------------------------------------------------

HistogramWidget::ColorBar::ColorBar(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(14);
    for (int i = 0; i < 256; ++i)
        lut[i] = qRgb(i, i, i);
}

void HistogramWidget::ColorBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const double W = width();
    for (int i = 0; i < 256; ++i) {
        const int x1 = qRound(i       * W / 256.0);
        const int x2 = qRound((i + 1) * W / 256.0);
        p.fillRect(x1, 0, qMax(1, x2 - x1), height(), QColor(lut[i]));
    }
}

void HistogramWidget::Canvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1e, 0x1e, 0x1e));
    if (maxBin <= 0) return;

    const int    W      = width();
    const int    H      = height();
    const double logMax = std::log1p(static_cast<double>(maxBin));
    const double barW   = static_cast<double>(W) / 256.0;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(180, 180, 180, 210));

    for (int i = 0; i < 256; ++i) {
        if (bins[i] <= 0) continue;
        const double norm = std::log1p(static_cast<double>(bins[i])) / logMax;
        const int    h    = qMax(1, qRound(norm * H));
        const int    x    = qRound(i * barW);
        const int    x2   = qRound((i + 1.0) * barW);
        p.drawRect(x, H - h, qMax(1, x2 - x), h);
    }
}

// ---- HistogramWidget -------------------------------------------------------

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent)
{
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(4, 4, 4, 4);
    vl->setSpacing(4);

    m_canvas = new Canvas(this);
    vl->addWidget(m_canvas, 0);

    m_colorBar = new ColorBar(this);
    vl->addWidget(m_colorBar);

    auto* grid = new QGridLayout;
    grid->setSpacing(4);
    grid->setContentsMargins(0, 0, 0, 0);

    auto makeTitleLabel = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        l->setFixedWidth(65);
        return l;
    };
    auto makeValLabel = [this]() {
        auto* l = new QLabel("0", this);
        l->setFixedWidth(30);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };

    grid->addWidget(makeTitleLabel("Brightness"), 0, 0);
    m_brightnessSlider = new QSlider(Qt::Horizontal, this);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    grid->addWidget(m_brightnessSlider, 0, 1);
    m_brightnessVal = makeValLabel();
    grid->addWidget(m_brightnessVal, 0, 2);

    grid->addWidget(makeTitleLabel("Contrast"), 1, 0);
    m_contrastSlider = new QSlider(Qt::Horizontal, this);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);
    grid->addWidget(m_contrastSlider, 1, 1);
    m_contrastVal = makeValLabel();
    grid->addWidget(m_contrastVal, 1, 2);

    vl->addLayout(grid);

    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int v) {
        m_brightnessVal->setText(QString::number(v));
        emit brightnessChanged(v);
    });
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int v) {
        m_contrastVal->setText(QString::number(v));
        emit contrastChanged(v);
    });
}

void HistogramWidget::setImage(const QImage& image)
{
    m_canvas->bins.fill(0);
    m_canvas->maxBin = 1;

    if (!image.isNull()) {
        // Scale down large images before computing the histogram to keep UI responsive.
        QImage src = image;
        if (src.width() > 512 || src.height() > 512)
            src = src.scaled(512, 512, Qt::KeepAspectRatio, Qt::FastTransformation);
        if (src.format() != QImage::Format_RGB32)
            src = src.convertToFormat(QImage::Format_RGB32);

        const int W = src.width();
        const int H = src.height();
        for (int y = 0; y < H; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(src.constScanLine(y));
            for (int x = 0; x < W; ++x) {
                const QRgb px  = row[x];
                const int  lum = (qRed(px) * 77 + qGreen(px) * 150 + qBlue(px) * 29) >> 8;
                m_canvas->bins[lum]++;
            }
        }
        for (int v : m_canvas->bins)
            if (v > m_canvas->maxBin) m_canvas->maxBin = v;
    }

    m_canvas->update();
}

void HistogramWidget::setBrightness(int v)
{
    QSignalBlocker blk(m_brightnessSlider);
    m_brightnessSlider->setValue(v);
    m_brightnessVal->setText(QString::number(v));
}

void HistogramWidget::setContrast(int v)
{
    QSignalBlocker blk(m_contrastSlider);
    m_contrastSlider->setValue(v);
    m_contrastVal->setText(QString::number(v));
}

void HistogramWidget::setColorTable(const QVector<QRgb>& lut)
{
    const int n = qMin(256, lut.size());
    for (int i = 0; i < n; ++i)
        m_colorBar->lut[i] = lut[i];
    m_colorBar->update();
}
