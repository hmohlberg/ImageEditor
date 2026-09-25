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

#include "OverviewWidget.h"

#include <QPainter>
#include <QPen>
#include <QMouseEvent>
#include <QResizeEvent>

OverviewWidget::OverviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMinimumHeight(40);
}

QSize OverviewWidget::sizeHint() const
{
    constexpr int kPreferredW = 200;
    if ( m_sourceImage.isNull() || m_sourceImage.width() == 0 )
        return {kPreferredW, 120};
    return {kPreferredW, qMax(40, m_sourceImage.height() * kPreferredW / m_sourceImage.width())};
}

void OverviewWidget::applyProportionalHeight()
{
    if ( m_sourceImage.isNull() || m_sourceImage.width() == 0 || width() == 0 )
        return;
    const int h = qMax(40, m_sourceImage.height() * width() / m_sourceImage.width());
    setFixedHeight(h);
}

void OverviewWidget::setImage(const QImage& image)
{
    if ( image.isNull() ) { clear(); return; }
    constexpr int kMaxDim = 1024;
    if ( image.width() > kMaxDim || image.height() > kMaxDim )
        m_sourceImage = image.scaled(kMaxDim, kMaxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else
        m_sourceImage = image;
    m_hasContent = true;
    applyProportionalHeight();
    update();
}

void OverviewWidget::setVisibleRect(const QRectF& visibleScene, const QRectF& fullScene)
{
    m_visibleScene = visibleScene;
    m_fullScene    = fullScene;
    update();
}

void OverviewWidget::clear()
{
    m_sourceImage  = QImage();
    m_visibleScene = QRectF();
    m_fullScene    = QRectF();
    m_hasContent   = false;
    setMinimumHeight(40);
    setMaximumHeight(QWIDGETSIZE_MAX);
    update();
}

void OverviewWidget::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if ( e->size().width() != e->oldSize().width() )
        applyProportionalHeight();
}

QRect OverviewWidget::drawRect() const
{
    if ( m_sourceImage.isNull() || m_sourceImage.width() == 0 ) return rect();
    const int w = width();
    const int h = qMax(1, m_sourceImage.height() * w / qMax(1, m_sourceImage.width()));
    return QRect(0, (height() - h) / 2, w, h);
}

QPointF OverviewWidget::widgetToScene(const QPoint& p) const
{
    if ( m_fullScene.isEmpty() ) return {};
    const QRect dr = drawRect();
    if ( dr.width() == 0 || dr.height() == 0 ) return {};
    const qreal sx = m_fullScene.width()  / dr.width();
    const qreal sy = m_fullScene.height() / dr.height();
    return QPointF(
        m_fullScene.left() + (p.x() - dr.left()) * sx,
        m_fullScene.top()  + (p.y() - dr.top())  * sy
    );
}

void OverviewWidget::mousePressEvent(QMouseEvent* e)
{
    if ( !m_hasContent || m_fullScene.isEmpty() ) return;
    if ( e->button() == Qt::LeftButton ) {
        m_dragging      = true;
        m_dragStart     = e->pos();
        m_dragSceneStart = m_visibleScene.center();
        // If click is outside the red rect, jump immediately to that point.
        const QPointF sp = widgetToScene(e->pos());
        const QRect dr = drawRect();
        const qreal sx = qreal(dr.width())  / m_fullScene.width();
        const qreal sy = qreal(dr.height()) / m_fullScene.height();
        QRectF redRect(
            dr.left() + (m_visibleScene.left() - m_fullScene.left()) * sx,
            dr.top()  + (m_visibleScene.top()  - m_fullScene.top())  * sy,
            m_visibleScene.width()  * sx,
            m_visibleScene.height() * sy
        );
        if ( !redRect.contains(e->pos()) ) {
            m_dragSceneStart = sp;
            emit centerRequested(sp);
        }
    }
}

void OverviewWidget::mouseMoveEvent(QMouseEvent* e)
{
    if ( !m_dragging || m_fullScene.isEmpty() ) return;
    const QRect dr = drawRect();
    if ( dr.width() == 0 || dr.height() == 0 ) return;
    const qreal sx = m_fullScene.width()  / dr.width();
    const qreal sy = m_fullScene.height() / dr.height();
    const QPointF delta(
        (e->pos().x() - m_dragStart.x()) * sx,
        (e->pos().y() - m_dragStart.y()) * sy
    );
    emit centerRequested(m_dragSceneStart + delta);
}

void OverviewWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if ( e->button() == Qt::LeftButton )
        m_dragging = false;
}

void OverviewWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    if ( !m_hasContent || m_fullScene.isEmpty() ) return;
    if ( e->button() == Qt::LeftButton )
        emit centerRequested(widgetToScene(e->pos()));
}

void OverviewWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(40, 40, 40));

    if ( !m_hasContent || m_sourceImage.isNull() ) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, tr("No image loaded"));
        return;
    }

    const QRect dr = drawRect();
    const int w = dr.width();
    const int h = dr.height();

    p.drawImage(dr,
                m_sourceImage.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    if ( !m_fullScene.isEmpty() && m_visibleScene.isValid() && h > 0 ) {
        const qreal sx = qreal(w) / m_fullScene.width();
        const qreal sy = qreal(h) / m_fullScene.height();
        const QRectF redRect(
            dr.left() + (m_visibleScene.left() - m_fullScene.left()) * sx,
            dr.top()  + (m_visibleScene.top()  - m_fullScene.top())  * sy,
            m_visibleScene.width()  * sx,
            m_visibleScene.height() * sy
        );
        p.setPen(QPen(Qt::red, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(redRect);
    }
}
