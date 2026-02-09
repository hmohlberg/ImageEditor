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
#include <QImage>
#include <QVector>
#include <QRgb>

#include "AbstractCommand.h"

class LayerItem;

class InvertLayerCommand : public AbstractCommand
{
public:
    explicit InvertLayerCommand( LayerItem* layer, const QVector<QRgb>& lut, int idx = -1,
                                     QUndoCommand* parent = nullptr );

    QString type() const override { return "InvertLayer"; }
    
    void undo() override;
    void redo() override;
    
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1003; }
    
    QJsonObject toJson() const override;
    static InvertLayerCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers );

private:
    int m_layerId;
    LayerItem* m_layer;
    QImage     m_backup;
    QVector<QRgb> m_lut;
};