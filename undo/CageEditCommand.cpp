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

#include "CageEditCommand.h"
CageEditCommand::CageEditCommand(LayerItem* layer, const LayerItem::CageState& before,
                                 const LayerItem::CageState& after, const QString& text,
                                 QUndoCommand* parent)
    : QUndoCommand(text, parent), m_layer(layer), m_before(before), m_after(after)
{}

void CageEditCommand::undo()
{
    if ( m_layer )
        m_layer->restoreCageState(m_before);
}

void CageEditCommand::redo()
{
    if ( m_firstRedo ) { m_firstRedo = false; return; }
    if ( m_layer )
        m_layer->restoreCageState(m_after);
}
