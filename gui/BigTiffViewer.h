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

#pragma once

#include <QColor>
#include <QHash>
#include <QRectF>
#include <QRgb>
#include <QSize>
#include <QVector>
#include <QWidget>

struct BigTiffLevel {
    uint32_t w = 0, h = 0;
    uint32_t tileW = 0, tileH = 0;
    int      dirIdx = 0;
    bool     tiled  = false;
};

class BigTiffGraphicsView;

class BigTiffViewer : public QWidget
{
    Q_OBJECT

public:
    explicit BigTiffViewer(QWidget* parent = nullptr);
    ~BigTiffViewer();

    bool    open(const QString& path);
    void    closeTiff();
    bool    isOpen() const;
    QString filePath() const { return m_filePath; }
    QSize   imageSize() const;
    void    setColorTable(const QVector<QRgb>& lut);
    void    setBrightness(int v);
    void    setContrast(int v);
    bool    saveTiff(const QString& outputPath);
    void    centerOn(const QPointF& scenePos);

signals:
    void closeRequested();
    void scaleChanged(double scale);
    void cursorPositionChanged(int x, int y);
    void cursorColorChanged(const QColor& color);
    void viewportChanged(QRectF visibleScene, QRectF fullScene);

private slots:
    void zoomIn();
    void zoomOut();
    void fitView();
    void saveAs();

private:
    bool openWebMode(const QString& url);
    void updateInfoLabel();

    BigTiffGraphicsView* m_view      = nullptr;
    QString              m_filePath;
    class QLabel*        m_infoLabel = nullptr;
};
