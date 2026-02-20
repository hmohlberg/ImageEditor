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

#include "PaintStrokeCommand.h"
#include "AbstractCommand.h"
#include <QPainter>
#include <QtMath>

#include "../util/BrushUtils.h"
#include "../util/Compress.h"

#include <iostream>

// -------------------------------- Constructor --------------------------------
PaintStrokeCommand::PaintStrokeCommand( LayerItem* layer,
        const QPoint& pos, const QColor& color, int radius,qreal hardness, QUndoCommand* parent )
    : AbstractCommand(parent),
      m_layer(layer),
      m_backup(layer->image()),  // Backup für Undo
      m_pos(pos),
      m_radius(radius),
      m_hardness(hardness),
      m_color(color)
{
    setText(QString("PaintStroke at (%1,%2)").arg(pos.x()).arg(pos.y()));
    m_layerId = layer->id();
    redo(); // sofort ausführen
}

PaintStrokeCommand::PaintStrokeCommand( LayerItem* layer,
        const QVector<QPoint>& strokePoints, const QColor& color, int radius, float hardness, QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_points(strokePoints)
    , m_color(color)
    , m_radius(radius)
    , m_hardness(hardness)
{
    Q_ASSERT(m_layer);
    Q_ASSERT(!m_points.isEmpty());
    setText(QString("PaintStroke %1").arg(strokePoints.size()));
    m_dirtyRect = QRect(m_points.first(), QSize(1,1));
    for ( const QPoint& p : m_points )
        m_dirtyRect |= QRect(p, QSize(1,1));
    int pad = m_radius + 2;
    m_dirtyRect.adjust(-pad, -pad, pad, pad);
    m_dirtyRect &= m_layer->image().rect();
    m_backup = m_layer->image(1).copy(m_dirtyRect);
    // --- test save ---
    // QString text = CompressUtils::toGZipBase64(m_backup);
    // CompressUtils::saveToFile("/tmp/testimage.txt",text);
    // --- --- --- ---
    m_layerId = layer->id();
}

// --------------------------------  --------------------------------
void PaintStrokeCommand::paint( QImage &img )
{
  // std::cout << "PaintStrokeCommand::paint(): image=(" << img.width() << "x" << img.height() << "), type=" << img.format() << ", npoints=" << m_points.size() << std::endl;
  {
    if ( m_points.size() == 1 ) {
      BrushUtils::dab(
            img,
            m_points.first(),
            m_color,
            m_radius,
            m_hardness
      );
      return;
    }
    for ( int i = 1; i < m_points.size(); ++i ) {
      // std::cout << " point1=(" << m_points[i-1].x() << ":" << m_points[i-1].y() << ") -> point2=(" << m_points[i].x() << ":" << m_points[i].y() << ")" << std::endl;
      BrushUtils::strokeSegment(
            img,
            m_points[i - 1],
            m_points[i],
            m_color,
            m_radius,
            m_hardness
      );
    }
  }
}

// --------------------------------  --------------------------------
void PaintStrokeCommand::undo()
{
  // std::cout << "PaintStrokeCommand::undo(): Processing..." << std::endl;
  {
    if ( !m_layer || m_backup.isNull() )
        return;
    QImage& img = m_layer->image();
    QPainter p(&img);
    p.drawImage(m_dirtyRect.topLeft(), m_backup);
    m_layer->updateImageRegion(m_dirtyRect);
  }
}

void PaintStrokeCommand::redo()
{
  // std::cout << "PaintStrokeCommand::redo(): Processing..." << std::endl;
  {
    if ( m_silent || !m_layer || m_points.isEmpty() )
      return;
    QImage& img = m_layer->image();
    paint(img);
    m_layer->updateImageRegion(m_dirtyRect);
  }
}

// -------------- JSON stuff -------------- 
QJsonObject PaintStrokeCommand::toJson() const
{
    QJsonObject obj = AbstractCommand::toJson();
    obj["type"] = "PaintStrokeCommand";
    // Layer-ID (wichtig!)
    obj["layerId"] = m_layer ? m_layer->id() : -1;
    // Brush
    obj["radius"]   = m_radius;
    obj["hardness"] = m_hardness;
    QJsonObject colorObj;
    colorObj["r"] = m_color.red();
    colorObj["g"] = m_color.green();
    colorObj["b"] = m_color.blue();
    colorObj["a"] = m_color.alpha();
    obj["color"] = colorObj;
    // Stroke Points
    QJsonArray pts;
    for ( const QPoint& p : m_points ) {
        QJsonObject po;
        po["x"] = p.x();
        po["y"] = p.y();
        pts.append(po);
    }
    obj["points"] = pts;
    return obj; 
}

// *** *** *** *** BEGIN OLD STYLE *** *** *** ***
PaintStrokeCommand* PaintStrokeCommand::fromJson( const QJsonObject& obj, LayerItem* layer )
{
    return new PaintStrokeCommand(
              layer,
              QPoint(obj["posX"].toInt(), obj["posY"].toInt()),
              QColor(obj["red"].toInt(), obj["green"].toInt(),obj["blue"].toInt()),
              obj["radius"].toInt(),
              obj["hardness"].toDouble()
              
    );
}
// *** *** *** *** END OLD STYLE *** *** *** ***

PaintStrokeCommand* PaintStrokeCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent )
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
        qWarning() << "PaintStrokeCommand::fromJson(): Layer not found:" << layerId;
        return nullptr;
    }
    
    // Color
    QJsonObject c = obj["color"].toObject();
    QColor color(
        c["r"].toInt(),
        c["g"].toInt(),
        c["b"].toInt(),
        c["a"].toInt(255)
    );

    // Brush
    int radius     = obj["radius"].toInt(1);
    float hardness = float(obj["hardness"].toDouble(1.0));
    
    // Stroke Points
    QVector<QPoint> points;
    QJsonArray pts = obj["points"].toArray();
    points.reserve(pts.size());
    for ( const QJsonValue& v : pts ) {
        QJsonObject po = v.toObject();
        points.emplace_back(po["x"].toInt(), po["y"].toInt());
    }
    if ( points.size() < 2 ) {
        qWarning() << "PaintStrokeCommand::fromJson(): Invalid stroke.";
        return nullptr;
    }
    
    // Create
    return new PaintStrokeCommand(
        layer,
        points,
        color,
        radius,
        hardness,
        parent
    );
}