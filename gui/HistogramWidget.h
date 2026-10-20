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

#pragma once

#include <QWidget>
#include <QVector>
#include <QRgb>
#include <array>

class QSlider;
class QLabel;

class HistogramWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistogramWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setBrightness(int v);
    void setContrast(int v);
    void setColorTable(const QVector<QRgb>& lut);

signals:
    void brightnessChanged(int);
    void contrastChanged(int);

private:
    class Canvas : public QWidget {
    public:
        explicit Canvas(QWidget* parent = nullptr);
        std::array<int, 256> bins{};
        int maxBin = 1;
    protected:
        void paintEvent(QPaintEvent*) override;
        QSize sizeHint()        const override { return {200, 120}; }
        QSize minimumSizeHint() const override { return {60,   60}; }
    };

    class ColorBar : public QWidget {
    public:
        explicit ColorBar(QWidget* parent = nullptr);
        std::array<QRgb, 256> lut{};
    protected:
        void paintEvent(QPaintEvent*) override;
        QSize sizeHint()        const override { return {200, 14}; }
        QSize minimumSizeHint() const override { return {60,  14}; }
    };

    Canvas*   m_canvas           = nullptr;
    ColorBar* m_colorBar         = nullptr;
    QSlider*  m_brightnessSlider = nullptr;
    QSlider*  m_contrastSlider   = nullptr;
    QLabel*   m_brightnessVal    = nullptr;
    QLabel*   m_contrastVal      = nullptr;
};
