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

#include "DeleteLayerCommand.h"
#include "../core/Config.h"

#include <QPainter>
#include <QtMath>
#include <QDebug>

// -------------- Constructor --------------
DeleteLayerCommand::DeleteLayerCommand( LayerItem* layer, const QPointF& pos, const int idx, QUndoCommand* parent )
        : AbstractCommand(parent), m_layer(layer), m_pos(pos), m_layerId(idx)
{
    setText(QString("Delete Layer %1").arg(idx));
    QByteArray moveLayerSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<path d='M32 12 V52 M12 32 H52 M32 12 L26 18 M32 12 L38 18 "
      "M32 52 L26 46 M32 52 L38 46 M12 32 L18 26 M12 32 L18 38 "
      "M52 32 L46 26 M52 32 L46 38' "
      "fill='none' stroke='white' stroke-width='3' stroke-linecap='round' stroke-linejoin='round'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(moveLayerSvg));
}

// -------------- Undo / redo  -------------- 
void DeleteLayerCommand::undo() 
{ 
  // qDebug() << "DeleteLayerCommand::undo(): pos =" << m_pos;
  {
    if ( !m_layer ) return;
    m_layer->setPos(m_pos);
    m_layer->setVisible(true);
    m_layer->setInActive(false);
  }
}

void DeleteLayerCommand::redo()
{ 
  // qDebug() << "DeleteLayerCommand::redo(): pos= " << m_pos;
  {
    if ( m_silent || !m_layer ) return;
    m_layer->setPos(m_pos); 
    m_layer->setVisible(false);
    m_layer->setInActive(true);
  }
}

// -------------- history related methods  -------------- 
QJsonObject DeleteLayerCommand::toJson() const
{
   QJsonObject obj = AbstractCommand::toJson();
   obj["layerId"] = m_layerId;
   obj["posX"] = m_pos.x();
   obj["posY"] = m_pos.y();
   obj["type"] = type();
   return obj;
}

DeleteLayerCommand* DeleteLayerCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
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
      qWarning() << "DeleteLayerCommand::fromJson(): Layer " << layerId << " not found.";
      return nullptr;
    }
    return new DeleteLayerCommand(layer,
                  QPointF(obj["posX"].toDouble(),obj["posY"].toDouble()),obj["layerId"].toInt());
}

