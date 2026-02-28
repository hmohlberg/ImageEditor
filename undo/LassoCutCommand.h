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

#include <QDebug>
#include <QGraphicsScene>
#include <QUndoCommand>
#include <QImage>
#include <QPainter>
#include <iostream>

#include "AbstractCommand.h"
#include "../layer/LayerItem.h"
#include "../layer/Layer.h"

class ImageView;

class LassoCutCommand : public AbstractCommand
{

  public:
  
    LassoCutCommand( LayerItem* originalLayer, LayerItem* newLayer, const QRect& bounds, 
                         const QImage& originalBackup, const int index, const QString& name, QUndoCommand* parent=nullptr );
    
    QString type() const override { return "LassoCut"; }
    AbstractCommand* clone() const override { return new LassoCutCommand(m_originalLayer, m_newLayer, m_bounds, m_backup, m_newLayerId, m_name); }
    
    void undo() override;
    void redo() override;
    
    LayerItem* layer() const override { return m_newLayer; }
    
    int id() const override { return 1001; }
    
    QJsonObject toJson() const override;
    static LassoCutCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent = nullptr );
    
    const QRect& rect() const { return m_bounds; }
    int layerId() const { return m_newLayerId; }
    void setController( QUndoCommand *undoCommand ) { m_controller = undoCommand; };
    
    void save_backup() {
      m_backup.save("/tmp/imageeditor_backuppic.png");
    }

  private:

    int m_originalLayerId = -1;
    int m_newLayerId = -1;
    
    QUndoCommand* m_controller = nullptr;
    LayerItem* m_originalLayer = nullptr;
    LayerItem* m_newLayer = nullptr;
    
    QString m_name;
    QRect m_bounds;
    QImage m_backup;
    
};