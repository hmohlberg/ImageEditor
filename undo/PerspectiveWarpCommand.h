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
#include <QVector>
#include <QPointF>

class LayerItem;

class PerspectiveWarpCommand : public AbstractCommand
{

  public:
  
    PerspectiveWarpCommand( LayerItem* layer, const QVector<QPointF>& before, const QVector<QPointF>& after, QUndoCommand* parent = nullptr );
                           
    AbstractCommand* clone() const override { return new PerspectiveWarpCommand(m_layer,m_before,m_after); }

    void setAfterQuad( const QVector<QPointF>& after ) { m_after = after; }
    QString type() const override { return "PerspectiveWarp"; }
    LayerItem* layer() const override { return nullptr; }
    int id() const override { return 1031; }
    
    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;
    static PerspectiveWarpCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent = nullptr );

  private:
  
    int m_layerId = -1;
    LayerItem* m_layer = nullptr;
    QString m_name;
    
    QVector<QPointF> m_before;
    QVector<QPointF> m_after;
    
};
