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

#include <QGraphicsPixmapItem>
#include <QImage>
#include "EditablePolygon.h"

class PolygonMaskLayerItem : public QGraphicsPixmapItem
{
public:
    PolygonMaskLayerItem( EditablePolygon* poly, const QImage& baseImage );

    void updateMask();               // wendet Polygon auf baseImage an
    void setOpacity( float opacity );  // Layer über original legen

    QImage maskImage() const { return m_mask; }
    EditablePolygon* polygon() const { return m_poly; }

private:
    EditablePolygon* m_poly;
    QImage m_baseImage; // Ausgangsbild
    QImage m_mask;      // Maskenbild, transparent außerhalb
    float m_opacity = 1.0f;
};
