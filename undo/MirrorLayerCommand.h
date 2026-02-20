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

class MirrorLayerCommand : public AbstractCommand
{

  public:

    MirrorLayerCommand( LayerItem* layer, const int idx, int mirrorPlane, QUndoCommand* parent = nullptr );
    
    AbstractCommand* clone() const override { return new MirrorLayerCommand(m_layer,m_layerId,m_mirrorPlane); }
    
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1006; }
    
    QString type() const override { return "MirrorLayer"; }
    void undo() override { if ( m_layer ) m_layer->setMirror(m_mirrorPlane); }
    void redo() override { if ( m_layer && !m_silent ) m_layer->setMirror(m_mirrorPlane); }
    bool mergeWith( const QUndoCommand* other ) override;
    
    QJsonObject toJson() const override;
    static MirrorLayerCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers );

  private:

    int m_layerId;
    int m_mirrorPlane;
    LayerItem* m_layer;
    
};