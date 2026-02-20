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

#include "MoveLayerCommand.h"
#include "../core/Config.h"

#include <QPainter>
#include <QtMath>
#include <QDebug>

// -------------- Constructor --------------
MoveLayerCommand::MoveLayerCommand( LayerItem* layer, const QPointF& oldPos, const QPointF& newPos, const int idx, QUndoCommand* parent )
        : AbstractCommand(parent), m_layer(layer), m_oldPos(oldPos), m_newPos(newPos), m_layerId(idx)
{
    setText(QString("Move Layer %1").arg(idx));
    QByteArray moveLayerSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<path d='M32 12 V52 M12 32 H52 M32 12 L26 18 M32 12 L38 18 "
      "M32 52 L26 46 M32 52 L38 46 M12 32 L18 26 M12 32 L18 38 "
      "M52 32 L46 26 M52 32 L46 38' "
      "fill='none' stroke='white' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(moveLayerSvg));
}

// -------------- Merge  -------------- 
bool MoveLayerCommand::mergeWith( const QUndoCommand* other )
{
   if ( other->id() != id() )
        return false;
    auto* cmd = static_cast<const MoveLayerCommand*>(other);
    if ( cmd->m_layer != m_layer )
        return false;
    m_newPos = cmd->m_newPos;
    return true;
}

// -------------- Undo / redo  -------------- 
void MoveLayerCommand::undo() 
{ 
  qDebug() << "MoveLayerCommand::undo(): old_pos =" << m_oldPos;
  {
    if ( m_layer ) m_layer->setPos(m_oldPos);
    else qWarning() << "MoveLayerCommand::undo(): Invalid layer";
  }
}

void MoveLayerCommand::redo()
{ 
  qDebug() << "MoveLayerCommand::redo(): old_pos= " << m_oldPos << " -> new_pos =" << m_newPos;
  {
    if ( m_silent ) return;
    if ( m_layer ) m_layer->setPos(m_newPos); 
    else qWarning() << "MoveLayerCommand::redo(): Invalid layer";
  }
}

// -------------- history related methods  -------------- 
QJsonObject MoveLayerCommand::toJson() const
{
   QJsonObject obj = AbstractCommand::toJson();
   obj["layerId"] = m_layerId;
   obj["fromX"] = m_oldPos.x();
   obj["fromY"] = m_oldPos.y();
   obj["toX"] = m_newPos.x();
   obj["toY"] = m_newPos.y();
   obj["type"] = type();
   return obj;
}

MoveLayerCommand* MoveLayerCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
{
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == layerId ) {
            layer = l;
            break;
        }
    }
    if ( !layer ) {
      qWarning() << "MoveLayerCommand::fromJson(): Layer " << layerId << " not found.";
      return nullptr;
    }
    return new MoveLayerCommand(
        layer,
        QPointF(obj["fromX"].toDouble(), obj["fromY"].toDouble()),
        QPointF(obj["toX"].toDouble(),   obj["toY"].toDouble()),
        obj["layerId"].toInt()
    );
}

