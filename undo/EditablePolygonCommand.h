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

#include "../layer/LayerItem.h"
#include "../layer/EditablePolygon.h"
#include "../layer/EditablePolygonItem.h"

#include <QGraphicsScene>

class EditablePolygonCommand : public AbstractCommand
{

public:

    EditablePolygonCommand( LayerItem* layer, QGraphicsScene* scene, const QPolygonF& polygon, const QString& name, QUndoCommand* parent = nullptr );

    AbstractCommand* clone() const override { return new EditablePolygonCommand(m_layer, m_scene, m_polygon, m_name); }
    QString type() const override { return "EditablePolygon"; }
    QString name() const { return m_name; }
    EditablePolygon* model() const { return m_model; }
    
    void setVisible( bool isVisible );
    void setSelected( bool isSelected );
    void setColor( const QColor& color );
    bool isSelected() const;
    
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1000; }
    
    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;
    static EditablePolygonCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QGraphicsScene* scene = nullptr );

private:

    LayerItem* m_layer = nullptr;
    QGraphicsScene* m_scene = nullptr;

    // Data
    QString m_name;
    QPolygonF m_polygon;

    // Runtime objects
    EditablePolygon* m_model = nullptr;
    EditablePolygonItem* m_item = nullptr;
    
};
