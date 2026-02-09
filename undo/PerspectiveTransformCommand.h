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
#include <QVector>

#include "AbstractCommand.h"
#include "../layer/LayerItem.h"

// -----------------  ----------------- 
class PerspectiveTransformCommand : public AbstractCommand
{

public:

    PerspectiveTransformCommand( LayerItem* layer,
                                const QVector<QPointF>& before,
                                const QVector<QPointF>& after );

    LayerItem* layer() const override { return m_layer; }
    QString type() const override { return "PerspectiveTransform"; }
    
    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;
    static PerspectiveTransformCommand* fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers );

private:

    LayerItem* m_layer;
    QVector<QPointF> m_before;
    QVector<QPointF> m_after;
};