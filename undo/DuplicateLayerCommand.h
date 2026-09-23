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

class DuplicateLayerCommand : public AbstractCommand
{

 public:

    DuplicateLayerCommand( LayerItem* duplicate, const int idx, QUndoCommand* parent = nullptr );

    AbstractCommand* clone() const override { return new DuplicateLayerCommand(m_layer, m_layerId); }

    QString type() const override { return "DuplicateLayer"; }
    LayerItem* layer() const override { return m_layer; }
    int id() const override { return 1056; }

    void undo() override;
    void redo() override;

    QJsonObject toJson() const override;

 private:

    int m_layerId = 0;
    LayerItem* m_layer = nullptr;

};
