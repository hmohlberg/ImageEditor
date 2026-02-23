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

#include "CageOverlayItem.h"
#include "LayerItem.h"
#include "CageMesh.h"
#include <QPainter>

CageOverlayItem::CageOverlayItem( LayerItem* layer ) : m_layer(layer)
{
    setZValue(10000);          // immer über dem Bild
    setAcceptedMouseButtons(Qt::NoButton);
}

QRectF CageOverlayItem::boundingRect() const
{
    return m_layer->boundingRect();
}

void CageOverlayItem::paint( QPainter* p, const QStyleOptionGraphicsItem*, QWidget* )
{
    const CageMesh& mesh = m_layer->cageMesh();
    if ( mesh.pointCount() == 0 )
        return;
    QPen pen(QColor(0, 255, 0)); // cage color
    pen.setWidth(0); // cosmetic
    p->setPen(pen);
    int cols = mesh.cols();
    int rows = mesh.rows();
    const auto& pts = mesh.points();
    // horizontale Linien
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x + 1 < cols; ++x) {
            int i1 = y * cols + x;
            int i2 = i1 + 1;
            p->drawLine(pts[i1], pts[i2]);
        }
    }
    // vertikale Linien
    for (int x = 0; x < cols; ++x) {
        for (int y = 0; y + 1 < rows; ++y) {
            int i1 = y * cols + x;
            int i2 = i1 + cols;
            p->drawLine(pts[i1], pts[i2]);
        }
    }
}
