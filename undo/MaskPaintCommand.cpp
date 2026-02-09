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

#include "MaskPaintCommand.h"

MaskPaintCommand::MaskPaintCommand(
    MaskLayer* layer,
    QVector<PixelChange>&& changes,
    QUndoCommand* parent
)
    : QUndoCommand(parent)
    , m_layer(layer)
    , m_changes(std::move(changes))
{
    setText(QString("Mask paint (%1 px)").arg(m_changes.size()));
}

void MaskPaintCommand::undo()
{
    if ( !m_layer ) return;
    for (const auto& c : m_changes)
        m_layer->setPixel(c.x, c.y, c.before);

    m_layer->emitChanged();
}

void MaskPaintCommand::redo()
{
    if ( !m_layer ) return;
    for (const auto& c : m_changes)
        m_layer->setPixel(c.x, c.y, c.after);
    m_layer->emitChanged();
}
