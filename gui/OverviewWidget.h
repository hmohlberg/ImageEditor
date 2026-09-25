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
#include <QImage>
#include <QPointF>
#include <QRectF>

class OverviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OverviewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setVisibleRect(const QRectF& visibleScene, const QRectF& fullScene);
    void clear();

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override { return {80, 40}; }

signals:
    void centerRequested(QPointF scenePos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void applyProportionalHeight();
    QRect  drawRect() const;
    QPointF widgetToScene(const QPoint& p) const;

    QImage  m_sourceImage;
    QRectF  m_visibleScene;
    QRectF  m_fullScene;
    bool    m_hasContent  = false;
    bool    m_dragging    = false;
    QPoint  m_dragStart;
    QPointF m_dragSceneStart;
};
