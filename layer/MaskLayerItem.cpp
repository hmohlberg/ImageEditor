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

#include "MaskLayerItem.h"

#include <QPainter>
#include <iostream>

MaskLayerItem::MaskLayerItem( MaskLayer* layer ) : m_layer(layer)
{
    setZValue(1000); // immer über Bild
    m_labelColors = {
        QColor(0,0,0,0),        // Label 0 = transparent
        QColor(255,0,0,255),
        QColor(0,255,0,255),
        QColor(0,0,255,255),
        QColor(255,255,0,255),
        QColor(255,0,255,255),
        QColor(0,255,255,255),
        QColor(128,128,128,255),
        QColor(255,128,0,255),
        QColor(128,0,255,255),
        QColor(0,128,255,255)
    };
}

void MaskLayerItem::setOpacityFactor( qreal o ) {
    qreal clamped = qBound(0.0, o, 1.0);
    if ( !qFuzzyCompare(clamped, m_opacityFactor) ) {
        m_opacityFactor = clamped;
        m_dirty = true;
        m_fullDirty = true;
        update();
    }
}

void MaskLayerItem::maskUpdated(const QRect& dirtyRect)
{
    m_dirty = true;
    if ( dirtyRect.isEmpty() ) {
        m_fullDirty = true;
        m_dirtyRect = QRect();
        update();
    } else {
        m_dirtyRect = m_dirtyRect.isEmpty() ? dirtyRect : m_dirtyRect.united(dirtyRect);
        update(QRectF(m_dirtyRect));
    }
}

void MaskLayerItem::setLabelColors( const QVector<QColor>& colors ) {
    m_labelColors = colors;
    m_dirty = true;
    m_fullDirty = true;
    update();
}

QRectF MaskLayerItem::boundingRect() const {
    return QRectF(0, 0, m_layer->width(), m_layer->height());
}

void MaskLayerItem::paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget* ) {
    if ( !m_layer ) return;
    if ( !m_dirty && !m_cachedImage.isNull() ) {
        p->drawImage(0, 0, m_cachedImage);
        return;
    }
    const QImage& mask = m_layer->image();
    if ( m_fullDirty || m_cachedImage.isNull() || m_cachedImage.size() != mask.size() ) {
        m_cachedImage = QImage(mask.size(), QImage::Format_ARGB32);
        m_cachedImage.fill(Qt::transparent);
        const int nc = m_labelColors.size();
        for (int y = 0; y < mask.height(); ++y) {
            const uchar* src = mask.constScanLine(y);
            QRgb*        dst = reinterpret_cast<QRgb*>(m_cachedImage.scanLine(y));
            for (int x = 0; x < mask.width(); ++x) {
                const int label = src[x];
                if ( label == 0 || label >= nc ) continue;
                const QColor& c = m_labelColors.at(label);
                dst[x] = qRgba(c.red(), c.green(), c.blue(), int(255 * m_opacityFactor));
            }
        }
        m_fullDirty = false;
    } else if ( !m_dirtyRect.isEmpty() ) {
        const QRect  dr = m_dirtyRect.intersected(mask.rect());
        const int    nc = m_labelColors.size();
        for (int y = dr.top(); y <= dr.bottom(); ++y) {
            const uchar* src = mask.constScanLine(y) + dr.left();
            QRgb*        dst = reinterpret_cast<QRgb*>(m_cachedImage.scanLine(y)) + dr.left();
            for (int x = 0; x < dr.width(); ++x) {
                const int label = src[x];
                dst[x] = (label == 0 || label >= nc)
                    ? 0
                    : qRgba(m_labelColors.at(label).red(), m_labelColors.at(label).green(),
                             m_labelColors.at(label).blue(), int(255 * m_opacityFactor));
            }
        }
    }
    m_dirtyRect = QRect();
    m_dirty = false;
    p->drawImage(0, 0, m_cachedImage);
}