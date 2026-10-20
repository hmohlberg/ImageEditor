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

class QUndoStack;

class DeleteLayerCommand : public AbstractCommand
{

 public:

    DeleteLayerCommand( LayerItem* layer, const QPointF& pos, const int idx=0,
                        QUndoStack* undoStack = nullptr, QUndoCommand* parent = nullptr );
    
    AbstractCommand* clone() const override { return new DeleteLayerCommand(m_layer, m_pos, m_layerId, m_undoStack); }

    
    QString type() const override { return "DeleteLayer"; }
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1055; }
    
    void undo() override;
    void redo() override;
    
    QJsonObject toJson() const override;
    static DeleteLayerCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers );

 private:

    void setRelatedCommandsDeleted( bool deleted );

    int m_layerId = 0;
    LayerItem* m_layer = nullptr;
    QUndoStack* m_undoStack = nullptr;

    QPointF m_pos;
    
};