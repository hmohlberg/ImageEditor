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

#include "CageWarpCommand.h"
#include "../layer/LayerItem.h"
#include "../core/Config.h"
#include <iostream>

// ---------------------- JSON ----------------------
CageWarpCommand::CageWarpCommand( LayerItem* layer,
           const QVector<QPointF>& before, const QVector<QPointF>& after, const QRectF& rect,
           int rows, int columns, QUndoCommand* parent )
    : AbstractCommand(parent),
      m_layer(layer),
      m_before(before),
      m_after(after),
      m_rect(rect),
      m_rows(rows),
      m_columns(columns)
{
    m_layerId = layer->id();
    setText(QString("Cage Warp %1").arg(m_layerId));
    QByteArray warpLayerSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<path d='M12 12 C25 18 39 18 52 12 M12 32 C25 38 39 38 52 32 M12 52 C25 58 39 58 52 52 "
      "M12 12 C18 25 18 39 12 52 M32 12 C38 25 38 39 32 52 M52 12 C58 25 58 39 52 52' "
      "fill='none' stroke='white' stroke-width='2.5' stroke-linecap='round'/>"
      "<circle cx='32' cy='32' r='3' fill='#007acc'/>"
      "</svg>";
   setIcon(AbstractCommand::getIconFromSvg(warpLayerSvg));
}

// 
void CageWarpCommand::pushNewWarpStep( const QVector<QPointF>& points )
{
  qCDebug(logEditor) << "CageWarpCommand::pushNewWarpStep(): Processing...";
  {
    m_after = points;
    m_steps += 1;
  }
}

// ---------------------- Undo/Redo ----------------------
void CageWarpCommand::undo()
{
  // qDebug() << "CageWarpCommand::undo(): m_beforepoints =" << m_before.size();
  {
    if ( !m_layer ) return;
    // Cage auf Startposition zurücksetzen
    CageMesh mesh = m_layer->cageMesh();
    mesh.setPoints(m_before);
    // m_layer->cageMesh().setPoints(m_before);
    // Triangle Warp auf Bild anwenden
    // ??? m_layer->applyTriangleWarp();
    m_layer->setCageVisible(LayerItem::OperationMode::CageWarp,false);
    m_layer->resetPixmap(); // holt das originale image
  }
}

void CageWarpCommand::redo()
{
  qCDebug(logEditor) << "CageWarpCommand::redo(): rows =" << m_rows << ", columns =" << m_columns << ", points =" << m_after.size();
  {
    if ( m_silent || !m_layer ) return;
    // Cage auf Endposition setzen
    
    m_layer->initCage(m_after,m_rect,m_rows,m_columns);
    
    // CageMesh mesh = m_layer->cageMesh();
    // mesh.setPoints(m_after);
    
    // m_layer->cageMesh().setPoints(m_after);
    // Triangle Warp auf Bild anwenden
    
    m_layer->setCageVisible(LayerItem::OperationMode::CageWarp,true);
    m_layer->applyTriangleWarp();
    
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
    for ( const QPointF &p : m_before ) {
     QJsonObject pointObj;
     pointObj.insert("x", p.x());
     pointObj.insert("y", p.y());
     cageBeforeObj.append(pointObj);
    }
   obj.insert("cagepoints_before", cageBeforeObj);
   QJsonArray cageAfterObj;
    for ( const QPointF &p : m_after ) {
     QJsonObject pointObj;
     pointObj.insert("x", p.x());
     pointObj.insert("y", p.y());
     cageAfterObj.append(pointObj);
    }
   obj.insert("cagepoints_after", cageAfterObj);
   QRectF bounds = m_layer->boundingRect();
   QJsonObject rectObj;
    rectObj.insert("x", bounds.x());
    rectObj.insert("y", bounds.y());
    rectObj.insert("width", bounds.width());
    rectObj.insert("height", bounds.height());
   obj["rect"] = rectObj;
   return obj;
}

CageWarpCommand* CageWarpCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent )
{
  qCDebug(logEditor) << "CageWarpCommand::fromJson(): Processing...";
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
      qWarning() << "CageWarpCommand::fromJson(): Layer " << layerId << " not found.";
      return nullptr;
    }
    int rows = obj["rows"].toInt(-1);
    int columns = obj["columns"].toInt(-1);
    // before
    QVector<QPointF> before;
    QJsonArray before_pts = obj["cagepoints_before"].toArray();
    before.reserve(before_pts.size());
    for ( const QJsonValue& v : before_pts ) {
        QJsonObject po = v.toObject();
        before.emplace_back(po["x"].toDouble(), po["y"].toDouble());
    }
    // after
    QVector<QPointF> after;
    QJsonArray after_pts = obj["cagepoints_after"].toArray();
    after.reserve(after_pts.size());
    for ( const QJsonValue& v : after_pts ) {
        QJsonObject po = v.toObject();
        after.emplace_back(po["x"].toDouble(), po["y"].toDouble());
    }
    // Rect
    QJsonObject r = obj["rect"].toObject();
    QRectF rect(r["x"].toDouble(),r["y"].toDouble(),r["width"].toInt(),r["height"].toInt());
    // create
    return new CageWarpCommand(layer,before,after,rect,rows,columns);
  }
}
