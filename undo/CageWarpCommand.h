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
#include <QVector>
#include <QPointF>
#include <QRectF>

#include "AbstractCommand.h"

class LayerItem;

class CageWarpCommand : public AbstractCommand
{

  public:

    CageWarpCommand( LayerItem* layer, const QVector<QPointF>& before,
                    const QVector<QPointF>& after, const QRectF& rect, int rows, int columns, QUndoCommand* parent = nullptr );

    QString type() const override { return "LassoCut"; }
    AbstractCommand* clone() const override { return new CageWarpCommand(m_layer, m_before, m_after, m_rect, m_rows, m_columns); }
    
    void undo() override;
    void redo() override;
    
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1002; }
    
    QJsonObject toJson() const override;
    static CageWarpCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent = nullptr );
    
    void pushNewWarpStep( const QVector<QPointF>& points );
    void setNumberOfRowsAndColumns( int n ) {
      m_rows = n;
      m_columns = n;
    }
    
    void setImage( const QImage& image ) { m_image = image; }
    void save_image() {
      m_image.save("/tmp/imageeditor_backuppic.png");
    }

  private:

    int m_layerId = -1;
    
    int m_steps = 0;
    int m_rows = 0;
    int m_columns = 0;
    
    QString m_interpolation = "trlinear";
    QRectF m_rect;
    
    LayerItem* m_layer;
    
    QVector<QPointF> m_before;  // Startposition der Cage-Punkte
    QVector<QPointF> m_after;   // Endposition der Cage-Punkte
    
    QImage m_image;
    
};