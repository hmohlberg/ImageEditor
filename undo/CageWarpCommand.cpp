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

#include <cstdio>
#include <iostream>
#include "CageWarpCommand.h"

#include "../gui/MainWindow.h"
#include "../layer/LayerItem.h"
#include "../core/Config.h"

// ---------------------- Constructor ----------------------
CageWarpCommand::CageWarpCommand( LayerItem* layer,
           const QVector<QPointF>& before, const QVector<QPointF>& after, const QRectF& rect, const QPointF& newPos,
           int rows, int columns, QUndoCommand* parent )
    : AbstractCommand(parent),
      m_layer(layer),
      m_before(before),
      m_after(after),
      m_rect(rect),
      m_newPos(newPos),
      m_rows(rows),
      m_columns(columns)
{
  qCDebug(logEditor) << "CageWarpCommand::CageWarpCommand(): Processing...";
  {
    m_layerId = m_layer->id();
    captureInitialState();
    layer->setCageWarpCommand(this);
    setText(QString("Cage Warp %1").arg(m_layerId));
    QByteArray warpLayerSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<path d='M12 12 C25 18 39 18 52 12 M12 32 C25 38 39 38 52 32 M12 52 C25 58 39 58 52 52 "
      "M12 12 C18 25 18 39 12 52 M32 12 C38 25 38 39 32 52 M52 12 C58 25 58 39 52 52' "
      "fill='none' stroke='white' stroke-width='2.5' stroke-linecap='round'/>"
      "<circle cx='32' cy='32' r='3' fill='#007acc'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(warpLayerSvg));
    printMessage();
  }
}

// ---------------------- Private methods ----------------------
void CageWarpCommand::captureInitialState()
{
    if ( !m_layer || m_initializedState ) return;
    m_oldPos = m_layer->pos();
    m_oldSceneRect = m_layer->sceneBoundingRect();
    m_transform = m_layer->totalTransform();
    m_originalImage = m_layer->originalImage();
    m_initializedState = true;
}

void CageWarpCommand::restoreOldSceneRect()
{
    if ( !m_layer ) return;
    const QRectF currentSceneRect = m_layer->sceneBoundingRect();
    const QPointF delta = m_oldSceneRect.topLeft() - currentSceneRect.topLeft();
    const QPointF pos = m_layer->pos() + delta;
    m_layer->setPos(pos);
}

void CageWarpCommand::printMessage( bool isUndo )
{
  if ( auto *ms = IMainSystem::instance() ) {
    if ( isUndo ) {
     ms->showMessage(QString("Undo cage warp of layer %1").arg(m_layerId));
    } else {
     ms->showMessage(QString("Cage warp of layer %1").arg(m_layerId));
    }
  }
}

// ---------------------- Methods ----------------------
void CageWarpCommand::pushNewWarpStep( const QPointF& pos, const QVector<QPointF>& points )
{
  qCDebug(logEditor) << "CageWarpCommand::pushNewWarpStep(): pos =" << pos;
  {
    m_newPos = pos;
    m_after = points;
    m_steps += 1;
  }
}

// ---------------------- Undo/Redo ----------------------
void CageWarpCommand::undo()
{
  qCDebug(logEditor) << "CageWarpCommand::undo(): m_beforepoints =" << m_before.size();
  {
    if ( !m_layer ) return;
    m_layer->setCagePoints(m_before);
    m_layer->setCageVisible(LayerItem::OperationMode::CageWarp,false,true);
    m_layer->setTotalTransform(m_transform);
    m_layer->setOriginalImage(m_originalImage,LayerItem::ImageType::Original);
    m_layer->setImageTransform(QTransform());
    m_layer->setPos(m_oldPos);
    restoreOldSceneRect();
    printMessage(true);
  }
}

void CageWarpCommand::redo()
{
  qCDebug(logEditor) << "CageWarpCommand::redo(): step =" << m_steps << ", rows =" << m_rows << ", columns =" << m_columns << ", points =" << m_after.size() << ", rect =" << m_rect;
  {
    if ( m_silent || !m_layer ) return;
    captureInitialState();
    m_transform = m_layer->totalTransform();
    m_originalImage = m_layer->originalImage();
    m_layer->initCage(m_after,m_rect,m_rows,m_columns);
    m_layer->setCageVisible(LayerItem::OperationMode::CageWarp,true);
    m_warpedImage = m_layer->applyCageWarp("CageWarpCommand");
    m_layer->setOriginalImage(m_warpedImage,m_steps == 0 ? LayerItem::ImageType::Original : LayerItem::ImageType::Warped);
    m_layer->setTotalTransform(QTransform());
    m_layer->resetPixmap();
    m_layer->setPos(m_newPos);
    m_newSceneRect = m_layer->sceneBoundingRect();
    printMessage();
  }
}

// ---------------------- JSON ----------------------
QJsonObject CageWarpCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["layerId"] = m_layerId;
    obj["type"] = "CageWarp";
    obj["rows"] = m_rows;
    obj["columns"] = m_columns;
    obj["interpolation"] = m_interpolation;

    QJsonArray cageBeforeObj;
    for (const QPointF& p : m_before) {
        QJsonObject pointObj;
        pointObj["x"] = p.x();
        pointObj["y"] = p.y();
        cageBeforeObj.append(pointObj);
    }
    obj["cagepoints_before"] = cageBeforeObj;

    QJsonArray cageAfterObj;
    for (const QPointF& p : m_after) {
        QJsonObject pointObj;
        pointObj["x"] = p.x();
        pointObj["y"] = p.y();
        cageAfterObj.append(pointObj);
    }
    obj["cagepoints_after"] = cageAfterObj;

    QJsonObject rectObj;
    rectObj["x"] = m_rect.x();
    rectObj["y"] = m_rect.y();
    rectObj["width"] = m_rect.width();
    rectObj["height"] = m_rect.height();
    obj["rect"] = rectObj;

    QJsonObject topLeftObj;
    topLeftObj["x"] = m_newPos.x();
    topLeftObj["y"] = m_newPos.y();
    obj["topLeft_after"] = topLeftObj;

    return obj;
}

CageWarpCommand* CageWarpCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent )
{
  qCDebug(logEditor) << "CageWarpCommand::fromJson(): Processing...";
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
        qWarning() << "CageWarpCommand::fromJson(): Layer" << layerId << "not found.";
        return nullptr;
    }

    const int rows = obj["rows"].toInt(-1);
    const int columns = obj["columns"].toInt(-1);
    if ( rows <= 0 || columns <= 0 ) {
      qWarning() << "CageWarpCommand::fromJson(): Invalid rows/columns.";
      return nullptr;
    }

    QVector<QPointF> before;
    const QJsonArray beforePts = obj["cagepoints_before"].toArray();
    before.reserve(beforePts.size());
    for ( const QJsonValue& v : beforePts ) {
        const QJsonObject po = v.toObject();
        before.emplace_back(po["x"].toDouble(), po["y"].toDouble());
    }

    QVector<QPointF> after;
    const QJsonArray afterPts = obj["cagepoints_after"].toArray();
    after.reserve(afterPts.size());
    for (const QJsonValue& v : afterPts) {
        const QJsonObject po = v.toObject();
        after.emplace_back(po["x"].toDouble(), po["y"].toDouble());
    }

    const QJsonObject r = obj["rect"].toObject();
    const QRectF rect(r["x"].toDouble(),
                      r["y"].toDouble(),
                      r["width"].toDouble(),
                      r["height"].toDouble());
    
    QPointF newPos = rect.topLeft();
    if ( obj.contains("topLeft_after") ) {                  
     const QJsonObject topLeftAfter = obj["topLeft_after"].toObject();
     newPos = QPointF(topLeftAfter["x"].toDouble(),topLeftAfter["y"].toDouble());
    }
    
    if ( before.size() != after.size() || before.isEmpty() ) {
        qWarning() << "CageWarpCommand::fromJson(): Invalid point arrays. Adjusting points to match After Cage.";
        QVector<QPointF> newBefore;
        before.reserve(after.size());
        qreal dx = rect.width() / ( columns - 1 );
        qreal dy = rect.height() / ( rows - 1 );
        for ( int y = 0; y < rows; ++y ) {
          for ( int x = 0; x < columns; ++x ) {
            newBefore.emplace_back( rect.left() + x * dx, rect.top()  + y * dy );
          }
        }
        return new CageWarpCommand(layer, newBefore, after, rect, newPos, rows, columns, parent);
    } else {
        return new CageWarpCommand(layer, before, after, rect, newPos, rows, columns, parent);
    }
  }
}