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

#include "CageMoveCommand.h"
#include "../layer/LayerItem.h"

CageMoveCommand::CageMoveCommand(
    LayerItem* layer,
    const QVector<QPointF>& before,
    const QVector<QPointF>& after)
    : m_layer(layer), m_before(before), m_after(after)
{
    setText("Cage Deformation");
}

void CageMoveCommand::undo()
{
    m_layer->setCagePoints(m_before);
    m_layer->applyCageWarp();
}

void CageMoveCommand::redo()
{
    m_layer->setCagePoints(m_after);
    m_layer->applyCageWarp();
}
