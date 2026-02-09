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

#include "PolygonMaskLayerItem.h"
#include <QPainter>
#include <QBrush>

PolygonMaskLayerItem::PolygonMaskLayerItem(EditablePolygon* poly,
                                           const QImage& baseImage)
    : m_poly(poly)
    , m_baseImage(baseImage)
{
    setZValue(100); // über dem Original
    updateMask();

    connect(m_poly, &EditablePolygon::changed,
            this, &PolygonMaskLayerItem::updateMask);
}

void PolygonMaskLayerItem::setOpacity( float opacity )
{
    m_opacity = qBound(0.0f, opacity, 1.0f);
    setOpacity(m_opacity);
}

void PolygonMaskLayerItem::updateMask()
{
    if (m_baseImage.isNull() || !m_poly) return;

    QImage img(m_baseImage.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QBrush(m_baseImage));
    p.setPen(Qt::NoPen);

    // Polygon in Bild-Koordinaten
    QPolygonF polyScene = m_poly->polygon();
    p.drawPolygon(polyScene);

    p.end();

    m_mask = img;
    setPixmap(QPixmap::fromImage(m_mask));
    setOpacity(m_opacity);
}

QJsonObject toJson() const
{
    QJsonObject obj;
    obj["type"] = "PolygonMaskLayer";
    obj["polygon"] = m_poly->toJson();
    obj["opacity"] = m_opacity;
    return obj;
}

static PolygonMaskLayerItem* fromJson( const QJsonObject& obj, const QImage& baseImage )
{
    EditablePolygon* poly = EditablePolygon::fromJson(obj["polygon"].toObject());
    PolygonMaskLayerItem* item = new PolygonMaskLayerItem(poly, baseImage);
    item->setOpacity(obj["opacity"].toDouble(1.0));
    return item;
}