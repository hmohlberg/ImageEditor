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

#include "PerspectiveTransform.h"

#include <QPainter>

PerspectiveTransform::PerspectiveTransform( QObject* parent )
    : QObject(parent)
{
}

void PerspectiveTransform::lockPoint( int idx, bool locked )
{
    if ( locked )
        m_lockedPoints.insert(idx);
    else
        m_lockedPoints.remove(idx);
}

bool PerspectiveTransform::isPointLocked( int idx ) const
{
    return m_lockedPoints.contains(idx);
}

void PerspectiveTransform::setTargetPoint( int idx, const QPointF& p )
{
    if ( idx < 0 || idx >= m_dst.size() )
        return;
    if ( isPointLocked(idx) )
        return;
    m_dst[idx] = p;
    applyConstraints(idx);
    emit changed();
}

void PerspectiveTransform::applyConstraints( int idx )
{
    if ( m_constraints.testFlag(KeepInBounds) ) {
        QRectF bounds = QPolygonF(m_src).boundingRect();
        QPointF& pt = m_dst[idx];
        pt.setX(qBound(bounds.left(),  pt.x(), bounds.right()));
        pt.setY(qBound(bounds.top(),   pt.y(), bounds.bottom()));
    }
    if ( m_constraints.testFlag(OrthogonalEdges) ) {
        // 0--1
        // |  |
        // 3--2
        if ( idx == 0 || idx == 1 ) {
            qreal y = m_dst[idx].y();
            m_dst[0].setY(y);
            m_dst[1].setY(y);
        }
        if ( idx == 2 || idx == 3 ) {
            qreal y = m_dst[idx].y();
            m_dst[2].setY(y);
            m_dst[3].setY(y);
        }
        if ( idx == 0 || idx == 3 ) {
            qreal x = m_dst[idx].x();
            m_dst[0].setX(x);
            m_dst[3].setX(x);
        }
        if ( idx == 1 || idx == 2 ) {
            qreal x = m_dst[idx].x();
            m_dst[1].setX(x);
            m_dst[2].setX(x);
        }
    }
}

void PerspectiveTransform::setSourceQuad( const QVector<QPointF>& src )
{
    if ( src.size() != 4 ) return;
    m_src = src;
    emit changed();
}

void PerspectiveTransform::setTargetQuad( const QVector<QPointF>& dst )
{
    if ( dst.size() != 4 ) return;
    m_dst = dst;
    emit changed();
}

bool PerspectiveTransform::isValid() const
{
    return m_src.size() == 4 && m_dst.size() == 4;
}

QTransform PerspectiveTransform::transform() const
{
    QTransform t;
    if ( !isValid() )
        return t;
    QTransform::quadToQuad(m_src, m_dst, t);
    return t;
}

QImage PerspectiveTransform::apply( const QImage& srcImage ) const
{
    if ( !isValid() )
        return srcImage;
    QTransform t = transform();
    QRectF bounds = QPolygonF(m_dst).boundingRect();
    QImage result(bounds.size().toSize(),
                  QImage::Format_ARGB32);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setTransform(t.translate(-bounds.left(), -bounds.top()));
    p.drawImage(QPointF(0, 0), srcImage);
    p.end();
    return result;
}
