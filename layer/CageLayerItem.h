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

#include "LayerItem.h"

class CageControlPointItem;

class CageLayerItem : public LayerItem
{

public:
    CageLayerItem( const QPixmap& pix, const QPolygonF& cagePolygon = QPolygonF() );

    void setCagePoint( int idx, const QPointF& pos );
    void beginCageEdit();
    void endCageEdit();
    
    bool isEditing() const { return m_editing; }
    
    void enableCage();
    void disableCage();
    bool cageEnabled() const;

protected:
    void paint( QPainter*, const QStyleOptionGraphicsItem*, QWidget* ) override;

private:
    QVector<QPointF> m_cage;
    QVector<CageControlPointItem*> m_points;
    QTransform m_startTransform;
    bool m_cageEnabled = false;
    bool m_editing = false;
    void createDefaultBoundingBox();
    
};
