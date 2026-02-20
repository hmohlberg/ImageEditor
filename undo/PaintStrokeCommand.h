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
#include <QColor>

#include "AbstractCommand.h"
#include "../layer/LayerItem.h"

class PaintStrokeCommand : public AbstractCommand
{

public:

    PaintStrokeCommand( LayerItem* layer, const QPoint& pos, const QColor& color, int radius, qreal hardness, QUndoCommand* parent = nullptr );
    PaintStrokeCommand( LayerItem* layer, const QVector<QPoint>& strokePoints, const QColor& color, int radius, float hardness, QUndoCommand* parent = nullptr );

    QString type() const override { return "PaintStroke"; }
    AbstractCommand* clone() const override { return new PaintStrokeCommand(m_layer, m_pos, m_color, m_radius, m_hardness); }
    
    void undo() override;
    void redo() override;
    
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1004; }
    
    QJsonObject toJson() const override;
    static PaintStrokeCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent = nullptr );
    static PaintStrokeCommand* fromJson( const QJsonObject& obj, LayerItem* layer );
    
private:

	void paint( QImage& img );

private:

    int m_layerId;
    LayerItem* m_layer = nullptr;
    
    QVector<QPoint>   m_points;
    QPoint     m_pos;
    int        m_radius = 1;
    qreal      m_hardness = 1.0;
    QColor     m_color;
    
    QRect      m_dirtyRect;
    QRect      m_backupRect;
    QImage     m_backup;
    
};