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
#include <QTransform>

#include "AbstractCommand.h"
#include "../layer/LayerItem.h"
#include "../core/Config.h"

class TransformLayerCommand : public AbstractCommand
{

 public:

    enum LayerTransformType { None, Scale, Rotate };

    TransformLayerCommand(
        LayerItem* layer,
        const QPointF& oldPos, 
        const QPointF& newPos,
        const QTransform& oldTransform,
        const QTransform& newTransform,
        const QString& name="Transform Layer",
        const LayerTransformType& trafoType = LayerTransformType::Rotate,
        QUndoCommand* parent = nullptr
    );
    TransformLayerCommand( LayerItem* layer, const QTransform& oldTransform,
        const QTransform& newTransform, QUndoCommand* parent = nullptr );
    
    AbstractCommand* clone() const override { return new TransformLayerCommand(m_layer, m_oldPos, m_newPos, m_oldTransform, m_newTransform, m_name); }
    
    void setRotationAngle( double rotation ) { m_rotationAngle = rotation; }
    double rotationAngle() const { return m_rotationAngle; }
    void setTransform( const QTransform& transform ) { m_newTransform = transform; } 
    QString type() const override { return "TransformLayer"; }
    LayerItem* layer() const override { return m_layer; }
    LayerTransformType trafoType() const { return m_trafoType; } 
    int id() const override { return 1234; }

    void undo() override;
    void redo() override;
    bool mergeWith( const QUndoCommand *other ) override;
    
    QJsonObject toJson() const override;
    static TransformLayerCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent = nullptr );

 private:

    int m_layerId = -1;
    double m_rotationAngle = 0.0;
    LayerItem* m_layer = nullptr;
    
    LayerTransformType m_trafoType = None;
    
    QString m_name;
    QPointF m_oldPos;
    QPointF m_newPos;
    
    QTransform m_oldTransform;
    QTransform m_newTransform;
    
};