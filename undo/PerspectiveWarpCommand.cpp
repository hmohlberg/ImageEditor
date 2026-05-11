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

#include <QPainter>
#include <QPolygonF>
#include <QtMath>

// -------------- Constructor --------------
PerspectiveWarpCommand::PerspectiveWarpCommand(
       LayerItem* layer, const QVector<QPointF>& before, const QVector<QPointF>& after, QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_beforeQuad(before)
    , m_afterQuad(after)
{
  qCDebug(logEditor) << "PerspectiveWarpCommand::PerspectiveWarpCommand(): Processing...";
  {
    m_isInitialized = false;
    m_origPosition = m_layer->pos();
    m_origTransform = m_layer->transform();
    m_origSceneTransform = m_layer->sceneTransform();
    m_origImage = m_layer->image(0);
    const QRectF r = m_layer->boundingRect();
    m_startQuad = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
    setAfterQuad(after);
    m_layerId = m_layer->id();
    m_name = QString("Perspective Warp Layer %1").arg(m_layerId);
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
}

AbstractCommand* PerspectiveWarpCommand::clone() const
{
    auto* cmd = new PerspectiveWarpCommand(m_layer, m_beforeQuad, m_afterQuad);
    cmd->m_layerId = m_layerId;
    cmd->m_name = m_name;
    cmd->m_isInitialized = m_isInitialized;
    cmd->m_origImage = m_origImage;
    cmd->m_origPosition = m_origPosition;
    cmd->m_origTransform = m_origTransform;
    cmd->m_origSceneTransform = m_origSceneTransform;
    cmd->m_warpTransform = m_warpTransform;
    cmd->m_newPosition = m_newPosition;
    cmd->m_startQuad = m_startQuad;
    cmd->setText(text());
    return cmd;
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
  qCDebug(logEditor) << "PerspectiveWarpCommand::mergeWith(): Processing...";
  {
    if ( other->id() != id() )
        return false;
    auto* cmd = static_cast<const PerspectiveWarpCommand*>(other);
    if ( cmd->m_layer != m_layer )
        return false;
    if ( cmd->m_deleted != m_deleted )
        return false;
    if ( cmd->m_startQuad.size() != 4 || m_startQuad.size() != 4 )
        return false;
    if ( cmd->m_startQuad != m_startQuad )
        return false;
    if ( cmd->m_origTransform != m_origTransform )
        return false;
    m_afterQuad = cmd->m_afterQuad;
    rebuildWarp();
    return true;
  }
}

void PerspectiveWarpCommand::setAfterQuad( const QVector<QPointF>& after ) 
{ 
  qCDebug(logEditor) << "PerspectiveWarpCommand::setAfterQuad(): after =" << after;
  {
    m_afterQuad = after; 
    rebuildWarp();
  }
}

void PerspectiveWarpCommand::setAfterQuadFromScene( const QVector<QPointF>& sceneQuad )
{
  qCDebug(logEditor) << "PerspectiveWarpCommand::setAfterQuadFromScene(): sceneQuad =" << sceneQuad;
  {
    if ( sceneQuad.size() != 4 ) return;
    bool invertible = false;
    QTransform sceneToOriginal = m_origSceneTransform.inverted(&invertible);
    if ( !invertible ) {
      qWarning() << "PerspectiveWarpCommand::setAfterQuadFromScene(): Original scene transform is not invertible.";
      return;
    }
    QVector<QPointF> after;
    after.reserve(4);
    for ( const QPointF& point : sceneQuad ) {
      after.push_back(sceneToOriginal.map(point));
    }
    setAfterQuad(after);
  }
}

void PerspectiveWarpCommand::resetWarp()
{
  qCDebug(logEditor) << "PerspectiveWarpCommand::resetWarp(): Processing...";
  {
    m_afterQuad = m_beforeQuad;
    rebuildWarp();
    redo();
  }
}

QVector<QPointF> PerspectiveWarpCommand::displayQuad() const
{
    QVector<QPointF> shifted;
    if ( m_afterQuad.size() != 4 ) {
        return shifted;
    }
    const QPointF topLeft = QPolygonF(m_afterQuad).boundingRect().topLeft();
    shifted.reserve(4);
    for ( const QPointF& point : m_afterQuad ) {
        shifted.push_back(point - topLeft);
    }
    return shifted;
}

void PerspectiveWarpCommand::rebuildWarp()
{
    m_warpTransform = QTransform();
    if ( m_beforeQuad.size() != 4 || m_afterQuad.size() != 4 ) {
        return;
    }

    const QRectF targetBounds = QPolygonF(m_afterQuad).boundingRect();
    QVector<QPointF> shiftedAfter;
    shiftedAfter.reserve(4);
    for ( const QPointF& point : m_afterQuad ) {
        shiftedAfter.push_back(point - targetBounds.topLeft());
    }

    if ( !QTransform::quadToQuad(m_beforeQuad, shiftedAfter, m_warpTransform) ) {
        qWarning() << "PerspectiveWarpCommand::rebuildWarp(): Cannot build perspective transform.";
        m_warpTransform = QTransform();
        return;
    }

    m_newPosition = m_origSceneTransform.map(targetBounds.topLeft());
}

bool PerspectiveWarpCommand::applyWarp()
{
    if ( !m_layer || m_beforeQuad.size() != 4 || m_afterQuad.size() != 4 || m_origImage.isNull() ) {
        return false;
    }

    if ( m_afterQuad == m_beforeQuad ) {
        m_layer->resetImageState(m_origImage, m_origPosition, m_origTransform);
        m_layer->setOriginalImage(m_origImage,LayerItem::ImageType::Original);
        return true;
    }

    const QRectF targetBounds = QPolygonF(m_afterQuad).boundingRect();
    const QSize targetSize(qMax(1, qCeil(targetBounds.width())),
                           qMax(1, qCeil(targetBounds.height())));
    QImage warped(targetSize, QImage::Format_ARGB32_Premultiplied);
    warped.fill(Qt::transparent);

    QPainter painter(&warped);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setTransform(m_warpTransform);
    painter.drawImage(QPointF(0, 0), m_origImage);
    painter.end();

    m_layer->resetImageState(warped, m_newPosition, QTransform());
    m_layer->setOriginalImage(warped,LayerItem::ImageType::Original);
    return true;
}

// -------------- Undo / redo --------------
void PerspectiveWarpCommand::undo()
{
  qCDebug(logEditor) << "PerspectiveWarpCommand::undo(): position =" << m_origPosition;
  {
    if ( !m_layer ) return;
    m_layer->resetImageState(m_origImage,m_origPosition,m_origTransform);
    m_layer->setOriginalImage(m_origImage,LayerItem::ImageType::Original);
    printMessage(true);
  }
}

void PerspectiveWarpCommand::redo()
{
  qCDebug(logEditor) << "PerspectiveWarpCommand::redo(): isInitialized =" << m_isInitialized << ", position =" << m_newPosition;
  {
    if ( !m_layer || m_deleted ) return;
    if ( applyWarp() ) {
      printMessage();
    }
  }
}

// -------------- History --------------
void PerspectiveWarpCommand::buildFromJson( const QPointF& position )
{
  m_isInitialized = false;
  rebuildWarp();
  m_newPosition = position;
}

QJsonObject PerspectiveWarpCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    
    obj["type"] = "PerspectiveWarp";
    obj["layerId"] = m_layer ? m_layer->id() : -1;
    
    QJsonObject newPositionObj;
    newPositionObj["x"] = m_newPosition.x();
    newPositionObj["y"] = m_newPosition.y();
    obj["newPosition"] = newPositionObj;

    QJsonArray b, a;
    for ( auto p : m_beforeQuad ) b.append(QJsonArray{p.x(), p.y()});
    for ( auto p : m_afterQuad )  a.append(QJsonArray{p.x(), p.y()});

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
    QPointF newPosition;
    const bool hasNewPosition = obj.contains("newPosition");
    if ( hasNewPosition ) {
        QJsonObject newPositionObj = obj["newPosition"].toObject();
        newPosition = QPointF(newPositionObj["x"].toDouble(), newPositionObj["y"].toDouble());
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

    PerspectiveWarpCommand* perspectiveWarpCommand = new PerspectiveWarpCommand(layer,before,after);
    perspectiveWarpCommand->buildFromJson(hasNewPosition ? newPosition : perspectiveWarpCommand->m_newPosition);
    return perspectiveWarpCommand;
}
