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
        update();
    }
}

void MaskLayerItem::maskUpdated()
{
    m_dirty = true;
    update();
}

void MaskLayerItem::setLabelColors( const QVector<QColor>& colors ) {
    m_labelColors = colors;
    update();
}

QRectF MaskLayerItem::boundingRect() const {
    return QRectF(0, 0, m_layer->width(), m_layer->height());
}

// to slow
void MaskLayerItem::paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget* ) {
    if ( !m_layer )
        return;
    if ( m_dirty || m_cachedImage.isNull() ) {
        // Nur neu berechnen wenn dirty oder Cache leer
        const QImage& mask = m_layer->image();
        m_cachedImage = QImage(mask.size(), QImage::Format_ARGB32);
        m_cachedImage.fill(Qt::transparent);
        for (int y = 0; y < mask.height(); ++y) {
            const uchar* line = mask.constScanLine(y);
            QRgb* out = reinterpret_cast<QRgb*>(m_cachedImage.scanLine(y));
            for (int x = 0; x < mask.width(); ++x) {
                int label = line[x];
                if (label == 0) continue;
                QColor c = m_labelColors[label];
                out[x] = QColor(c.red(), c.green(), c.blue(), int(255 * m_opacityFactor)).rgba();
            }
        }
        m_dirty = false;
    }
    p->drawImage(0, 0, m_cachedImage); // nur Cache zeichnen
}