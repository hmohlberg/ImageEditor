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

#include "AbstractCommand.h"
#include "../layer/EditablePolygon.h"

class LayerItem;

class PolygonMovePointCommand : public AbstractCommand
{

  public:
  
    PolygonMovePointCommand( EditablePolygon* poly, int idx, const QPointF& oldPos, const QPointF& newPos, QUndoCommand* parent = nullptr );
                            
    AbstractCommand* clone() const override { return new PolygonMovePointCommand(m_poly,m_idx,m_oldPos,m_newPos); }

    QString type() const override { return "MovePolygonPoint"; }
    LayerItem* layer() const override { return nullptr; }
    int id() const override { return 1009; }
    
    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;
    static PolygonMovePointCommand* fromJson( const QJsonObject& obj, EditablePolygon* poly );

  private:

    EditablePolygon* m_poly;
    int m_idx;
    QPointF m_oldPos;
    QPointF m_newPos;

};