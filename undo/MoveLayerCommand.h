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

#include <QUndoCommand>
#include <QPointF>

#include "AbstractCommand.h"
#include "../layer/LayerItem.h"

class MoveLayerCommand : public AbstractCommand
{

public:

    MoveLayerCommand( LayerItem* layer, const QPointF& oldPos, const QPointF& newPos, const int idx=0,
                      LayerItem::OperationMode reportedMode = LayerItem::OperationMode::Translate,
                      QUndoCommand* parent = nullptr );
    
    AbstractCommand* clone() const override { return new MoveLayerCommand(m_layer,m_oldPos,m_newPos,m_layerId,m_reportedMode); }
    
    QString type() const override { return "MoveLayer"; }
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1005; }
    bool mergeWith( const QUndoCommand* other ) override;
    void printMessage( bool isUndo=false );
    
    void undo() override;
    void redo() override;

    void shiftPositions( const QPointF& d ) { m_oldPos += d; m_newPos += d; }

    QJsonObject toJson() const override;
    static MoveLayerCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers );

private:

    int m_layerId;
    LayerItem* m_layer;

    QPointF m_oldPos;
    QPointF m_newPos;
    LayerItem::OperationMode m_reportedMode = LayerItem::OperationMode::Translate;
    
};