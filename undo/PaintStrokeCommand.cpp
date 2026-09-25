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

#include "../core/Config.h"
#include "../util/BrushUtils.h"
#include "../util/Compress.h"

#include <iostream>

// Paintbrush icon: diagonal handle + tapered bristle tip + paint stroke at bottom
const QByteArray PaintStrokeCommand::s_brushSvg =
    "<svg viewBox='0 0 64 64' xmlns='http://www.w3.org/2000/svg'>"
    "<line x1='48' y1='8' x2='30' y2='26' stroke='white' stroke-width='5' stroke-linecap='round'/>"
    "<path d='M26 30 L14 46 Q18 54 24 50 L36 34 Z' fill='white'/>"
    "<path d='M8 58 Q22 50 38 56 Q50 62 56 50' fill='none' stroke='white' stroke-width='3.5' stroke-linecap='round'/>"
    "</svg>";

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
    setIcon(AbstractCommand::getIconFromSvg(s_brushSvg));
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
    setIcon(AbstractCommand::getIconFromSvg(s_brushSvg));
    m_dirtyRect = QRect(m_points.first(), QSize(1,1));
    for ( const QPoint& p : m_points )
        m_dirtyRect |= QRect(p, QSize(1,1));
    int pad = m_radius + 2;
    m_dirtyRect.adjust(-pad, -pad, pad, pad);
    m_dirtyRect &= m_layer->image().rect();
    m_backup     = m_layer->image().copy(m_dirtyRect);   // display state backup for undo
    m_origBackup = m_layer->image(1).copy(m_dirtyRect);  // source image backup for colormap consistency
    m_layerId = layer->id();

    // Sample source-space background from m_originalImage corners.
    // This value is written into m_originalImage for painted pixels in redo() so that
    // future colormap changes produce the correct background there rather than re-mapping
    // the already-mapped display-space paint color.
    const QImage& orig = layer->image(1);
    if ( !orig.isNull() && orig.width() >= 2 && orig.height() >= 2 ) {
        int r = 0, g = 0, b = 0;
        const int iw = orig.width() - 1, ih = orig.height() - 1;
        for ( const QPoint& p : { QPoint{0,0}, QPoint{iw,0}, QPoint{0,ih}, QPoint{iw,ih} } )
            { QColor c(orig.pixel(p)); r += c.red(); g += c.green(); b += c.blue(); }
        m_origColor = QColor(r/4, g/4, b/4);
    } else {
        m_origColor = m_color;
    }
}

// --------------------------------  --------------------------------
void PaintStrokeCommand::paint( QImage &img )
{
  qCDebug(logEditor)<< "PaintStrokeCommand::paint(): image=(" << img.width() << "x" << img.height() << "), type=" << img.format() << ", npoints=" << m_points.size();
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
  qCDebug(logEditor) << "PaintStrokeCommand::undo(): Processing...";
  if ( !m_layer || m_backup.isNull() )
      return;
  QImage& orig = m_layer->image(1);
  if ( !m_origBackup.isNull() && !orig.isNull() && orig.size() == m_layer->image().size() )
      { QPainter p(&orig); p.drawImage(m_dirtyRect.topLeft(), m_origBackup); }
  // Re-derive m_image from restored m_originalImage using the current active LUT.
  // Restoring m_backup would show colours from the colormap era at stroke creation time.
  if ( !m_layer->activeLut().isEmpty() ) {
      m_layer->applyActiveLutToRegion(m_dirtyRect);
  } else {
      QImage& img = m_layer->image();
      QPainter p(&img); p.drawImage(m_dirtyRect.topLeft(), m_backup);
  }
  m_layer->updateImageRegion(m_dirtyRect);
}

void PaintStrokeCommand::redo()
{
  qCDebug(logEditor) << "PaintStrokeCommand::redo(): Processing...";
  if ( m_silent || !m_layer || m_points.isEmpty() )
    return;
  QImage& img  = m_layer->image();
  QImage& orig = m_layer->image(1);

  if ( !orig.isNull() && orig.size() == img.size() ) {
      // Build stroke mask to find exactly which pixels the stroke touches.
      QImage strokeMask(m_dirtyRect.size(), QImage::Format_Grayscale8);
      strokeMask.fill(0);
      const QPoint offset = -m_dirtyRect.topLeft();
      const QColor white(255, 255, 255, 255);
      if ( m_points.size() == 1 ) {
          BrushUtils::dab(strokeMask, m_points.first() + offset, white, m_radius, m_hardness);
      } else {
          for ( int i = 1; i < m_points.size(); ++i )
              BrushUtils::strokeSegment(strokeMask, m_points[i-1] + offset, m_points[i] + offset,
                                        white, m_radius, m_hardness);
      }
      // Write source-space background into m_originalImage for stroke pixels.
      const QRgb origBgRgb = m_origColor.rgba();
      for ( int y = m_dirtyRect.top(); y <= m_dirtyRect.bottom(); ++y ) {
          const uchar* maskLine = strokeMask.constScanLine(y - m_dirtyRect.top());
          QRgb*        origLine = reinterpret_cast<QRgb*>(orig.scanLine(y));
          for ( int x = m_dirtyRect.left(); x <= m_dirtyRect.right(); ++x ) {
              if ( maskLine[x - m_dirtyRect.left()] > 0 )
                  origLine[x] = origBgRgb;
          }
      }
      // Re-derive m_image from the updated m_originalImage via the current LUT.
      // This keeps stroke pixels consistent with the active colormap on every redo,
      // regardless of which colormap was in effect when the stroke was created.
      if ( !m_layer->activeLut().isEmpty() ) {
          m_layer->applyActiveLutToRegion(m_dirtyRect);
      } else {
          paint(img);
      }
  } else {
      paint(img);
  }
  m_layer->updateImageRegion(m_dirtyRect);
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