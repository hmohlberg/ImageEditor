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

#include "PerspectiveWarpCommand.h"

#include "../gui/MainWindow.h"
#include "../core/Config.h"
#include "../layer/LayerItem.h"

// -------------- Constructor --------------
PerspectiveWarpCommand::PerspectiveWarpCommand(
       LayerItem* layer, const QVector<QPointF>& before, const QVector<QPointF>& after, QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_before(before)
    , m_after(after)
{
    m_layerId = layer->id();
    m_name = QString("Perspective Warp Layer %1").arg(m_layerId);
    // m_baseTransform = m_layer->transform();
    // const QRectF r = layer->boundingRect();
    // m_startQuad = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
    setText(m_name);
    QByteArray perspectiveWarpSvg = "<svg viewBox='0 0 64 64' xmlns='http://www.w3.org/2000/svg'>"
      "<path d='M12 12h40v40H12z' fill='none' stroke='#ccc' stroke-dasharray='2,2' stroke-width='1'/>"
      "<path d='M10 20 L54 10 L48 54 L16 44 Z' fill='rgba(0, 122, 255, 0.2)' stroke='#007aff' stroke-width='3' stroke-linejoin='round'/>"
      "<circle cx='10' cy='20' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<circle cx='54' cy='10' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<circle cx='48' cy='54' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<circle cx='16' cy='44' r='4' fill='white' stroke='#007aff' stroke-width='2'/>"
      "<path d='M6 16 l4 4 m0-4 l-4 4' stroke='#007aff' stroke-width='1.5' stroke-linecap='round'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(perspectiveWarpSvg));
    printMessage();
}

// -------------- Methods --------------
void PerspectiveWarpCommand::printMessage( bool isUndo )
{
  if ( auto *ms = IMainSystem::instance() ) {
    if ( isUndo ) {
     IMainSystem::instance()->showMessage(QString("Undo perspective warp of layer %1").arg(m_layerId));
    } else {
     IMainSystem::instance()->showMessage(QString("Perspective warp of layer %1").arg(m_layerId));
    }
  }
}

bool PerspectiveWarpCommand::mergeWith( const QUndoCommand* other ) 
{
  qDebug() << "PerspectiveWarpCommand::mergeWith(): Processing...";
  {
    if ( other->id() != id() )
        return false;
qDebug() << "1";
    auto* cmd = static_cast<const PerspectiveWarpCommand*>(other);
    if ( cmd->m_layer != m_layer )
        return false;
qDebug() << "2";
    if ( cmd->m_deleted != m_deleted )
        return false;
qDebug() << "3";
    if ( cmd->m_startQuad.size() != 4 || m_startQuad.size() != 4 )
        return false;
qDebug() << "4";
    if ( cmd->m_startQuad != m_startQuad )
        return false;
qDebug() << "5";
    if ( cmd->m_baseTransform != m_baseTransform )
        return false;
qDebug() << "Got it";
    m_after = cmd->m_after;
    return true;
  }
}

void PerspectiveWarpCommand::apply( const QVector<QPointF>& quad )
{
  qCDebug(logEditor) << "PerspectiveWarpCommand::apply(): Processing...";
  {
    if ( !m_layer || quad.size() != 4 ) return;
    QRectF r = m_layer->boundingRect();
    QVector<QPointF> startQuad = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
    QTransform warp;
    if ( QTransform::quadToQuad(startQuad, quad, warp) ) {
      m_layer->setTransform(warp * m_baseTransform);
    }
  }
}

// -------------- Undo / redo --------------
void PerspectiveWarpCommand::undo()
{
    if ( !m_layer || m_deleted ) return;
    apply(m_before);
    printMessage(true);
}

void PerspectiveWarpCommand::redo()
{
    if ( !m_layer || m_deleted ) return;
    apply(m_after);
    printMessage();
}

// -------------- History --------------
QJsonObject PerspectiveWarpCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    
    obj["type"] = "PerspectiveWarp";
    obj["layerId"] = m_layer ? m_layer->id() : -1;

    QJsonArray b, a;
    for ( auto p : m_before ) b.append(QJsonArray{p.x(), p.y()});
    for ( auto p : m_after )  a.append(QJsonArray{p.x(), p.y()});

    obj["before"] = b;
    obj["after"]  = a;
    
    return obj;
}

PerspectiveWarpCommand* PerspectiveWarpCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent )
{
    // Layer
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == layerId ) {
            layer = l;
            break;
        }
    }
    if ( !layer ) {
        qWarning() << "PerspectiveWarpCommand::fromJson(): Layer not found:" << layerId;
        return nullptr;
    }
    
    // >>>
    QVector<QPointF> before, after;
    for( auto v : obj["before"].toArray() ) {
        auto arr = v.toArray();
        before.push_back(QPointF(arr[0].toDouble(),
                                arr[1].toDouble()));
    }
    for( auto v : obj["after"].toArray() ) {
        auto arr = v.toArray();
        after.push_back(QPointF(arr[0].toDouble(),
                               arr[1].toDouble()));
    }

    return new PerspectiveWarpCommand(layer, before, after);
}
