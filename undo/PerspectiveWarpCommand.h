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
#include <QImage>
#include <QTransform>

class LayerItem;

class PerspectiveWarpCommand : public AbstractCommand
{

  public:
  
    PerspectiveWarpCommand( LayerItem* layer, const QVector<QPointF>& before, const QVector<QPointF>& after, QUndoCommand* parent = nullptr );
                           
    AbstractCommand* clone() const override;

    void buildFromJson( const QPointF& position );
    void setAfterQuad( const QVector<QPointF>& after );
    void setAfterQuadFromScene( const QVector<QPointF>& sceneQuad );
    void resetWarp();
    QVector<QPointF> displayQuad() const;
    QString type() const override { return "PerspectiveWarp"; }
    bool mergeWith( const QUndoCommand* other ) override;
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1031; }
    
    void printMessage( bool isUndo = false );
    
    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;
    static PerspectiveWarpCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent = nullptr );

  private:

    void rebuildWarp();
    bool applyWarp();
  
    int m_layerId = -1;
    LayerItem* m_layer = nullptr;
    
    QString m_name;
    
    bool m_isInitialized;
    
    QImage m_origImage;
    QPointF m_origPosition;
    QTransform m_origTransform;
    QTransform m_origSceneTransform;
    
    QTransform m_warpTransform;
    QPointF m_newPosition;
    
    QVector<QPointF> m_startQuad;
    QVector<QPointF> m_beforeQuad;
    QVector<QPointF> m_afterQuad;
    
};
