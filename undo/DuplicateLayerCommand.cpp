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

#include "DuplicateLayerCommand.h"
#include "AbstractCommand.h"
#include "../core/Config.h"

DuplicateLayerCommand::DuplicateLayerCommand( LayerItem* duplicate, const int idx, QUndoCommand* parent )
    : AbstractCommand(parent), m_layer(duplicate), m_layerId(idx)
{
    setText(QString("Duplicate Layer %1").arg(idx));
    QByteArray svg =
      "<svg viewBox='0 0 64 64'>"
      "<rect x='8'  y='16' width='36' height='36' rx='3'"
      "  fill='none' stroke='white' stroke-width='3'/>"
      "<rect x='20' y='8'  width='36' height='36' rx='3'"
      "  fill='#444' stroke='white' stroke-width='3'/>"
      "<line x1='38' y1='17' x2='38' y2='35' stroke='white' stroke-width='2.5' stroke-linecap='round'/>"
      "<line x1='29' y1='26' x2='47' y2='26' stroke='white' stroke-width='2.5' stroke-linecap='round'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(svg));
}

void DuplicateLayerCommand::undo()
{
    if ( !m_layer ) return;
    m_layer->setVisible(false);
    m_layer->setInActive(true);
}

void DuplicateLayerCommand::redo()
{
    if ( m_silent ) {
        m_silent = false;
        return;
    }
    if ( !m_layer ) return;
    m_layer->setVisible(true);
    m_layer->setInActive(false);
}

QJsonObject DuplicateLayerCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"]    = type();
    obj["layerId"] = m_layerId;
    return obj;
}

DuplicateLayerCommand* DuplicateLayerCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
{
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = AbstractCommand::getLayerItem(layers, layerId);
    if ( !layer ) {
        qWarning() << "DuplicateLayerCommand::fromJson(): Layer not found:" << layerId;
        return nullptr;
    }
    auto* cmd = new DuplicateLayerCommand(layer, layerId);
    cmd->setSilent(true);
    return cmd;
}
