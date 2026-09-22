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

#include "LassoCutCommand.h"
#include "EditablePolygonCommand.h"
#include "../gui/MainWindow.h"

#include <QByteArray>
#include <QJsonObject>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <iostream>

// ---------------------- Constructor ----------------------
LassoCutCommand::LassoCutCommand( LayerItem* originalLayer, LayerItem* newLayer, const QRect& bounds,
                    const QImage& originalBackup, const int index, const QString& name, QUndoCommand* parent )
        : AbstractCommand(parent),
          m_originalLayer(originalLayer),
          m_newLayer(newLayer),
          m_bounds(bounds),
          m_backup(originalBackup),
          m_originalImageBackup(originalLayer->originalImage().copy()),
          m_name(name)
{
  qCDebug(logEditor) << "LassoCutCommand::LassoCutCommand(): Processing...";
  {
    newLayer->setPos(bounds.topLeft());
    m_originalLayerId = originalLayer->id();
    m_newLayerId = newLayer->id(); 
    setText(QString("%1 %2 Cut").arg(name).arg(index));    
    QByteArray rectSelectionSvg = 
      "<svg viewBox='0 0 64 64'>"
      "<rect x='10' y='14' width='44' height='36' "
      "fill='none' stroke='white' stroke-width='3' stroke-dasharray='6,4' stroke-linejoin='round'/>"
      "<path d='M10 24 V14 H20 M44 14 H54 V24 M54 40 V50 H44 M20 50 H10 V40' "
      "fill='none' stroke='white' stroke-width='3' stroke-linecap='round'/>"
      "</svg>";
    setIcon(AbstractCommand::getIconFromSvg(rectSelectionSvg));
    printMessage();
  }
}

void LassoCutCommand::printMessage( bool isUndo )
{
  if ( auto *ms = IMainSystem::instance() ) {
    IMainSystem::instance()->showMessage(QString("Created layer %1 with id %2 of size (%3x%4) at position (%5:%6)")
                                .arg(m_name).arg(m_newLayerId).arg(m_bounds.width()).arg(m_bounds.height())
                                .arg(m_bounds.x()).arg(m_bounds.y()) );
  }
}

// ---------------------- Undo/Redo ----------------------
void LassoCutCommand::undo()
{
  qCDebug(logEditor) << "LassoCutCommand::undo(): Processing...";
  {
    QImage tempImage = m_originalLayer->image();
    QPainter p(&tempImage);
     for ( int y = 0; y < m_backup.height(); ++y ) {
      for ( int x = 0; x < m_backup.width(); ++x ) {
        QColor maskPixel = m_backup.pixelColor(x, y);
        if ( maskPixel.alpha() > 0 ) {
          tempImage.setPixelColor(m_bounds.x()+x,m_bounds.y()+y,maskPixel);
        }
      }
     }
    p.end();
    m_originalLayer->setImage(tempImage);
    m_originalLayer->setOriginalImage(m_originalImageBackup);
    m_originalLayer->update();
    if ( m_newLayer->scene() ) {
      m_newLayer->scene()->removeItem(m_newLayer);
    }  
    if ( m_controller != nullptr ) {
     EditablePolygonCommand* editablePolyCommand = dynamic_cast<EditablePolygonCommand*>(m_controller);
     if ( editablePolyCommand != nullptr ) {
      editablePolyCommand->setVisible(true);
     }
    }
    m_newLayer->setVisible(false);
    m_newLayer->setInActive(true);
    MainWindow *window = dynamic_cast<MainWindow*>(m_newLayer->parent());
    if ( window != nullptr ) window->updateLayerList();
  }
}
    
void LassoCutCommand::redo()
{
  qCDebug(logEditor) << "LassoCutCommand::redo(): Processing...";
  {
    if ( m_silent ) return;
    QImage tempImage = m_originalLayer->image();
    // Detect the actual background colour from the image corners so colourmap
    // changes (e.g. Invert: white→black) are respected.
    QColor color = Config::isWhiteBackgroundImage ? Qt::white : Qt::black;
    if ( !tempImage.isNull() && tempImage.width() > 1 && tempImage.height() > 1 ) {
        int r = 0, g = 0, b = 0;
        for ( const QPoint& p : { QPoint{0,0}, QPoint{tempImage.width()-1,0},
                                   QPoint{0,tempImage.height()-1}, QPoint{tempImage.width()-1,tempImage.height()-1} } )
        {
            QColor c(tempImage.pixel(p));
            r += c.red(); g += c.green(); b += c.blue();
        }
        color = QColor(r/4, g/4, b/4);
    }
    for ( int y = 0; y < m_backup.height(); ++y ) {
      for ( int x = 0; x < m_backup.width(); ++x ) {
        QColor maskPixel = m_backup.pixelColor(x, y);
        if ( maskPixel.alpha() > 128 ) {
          tempImage.setPixelColor(m_bounds.x()+x,m_bounds.y()+y,color); // hier wird im orignal image ge-cuttet
        }
      }
    }
    m_originalLayer->setImage(tempImage);
    // Keep m_originalImage in sync: InvertLayerCommand applies LUT from originalImage(),
    // so we fill the cut region there too (with the raw-colorspace background).
    {
        QImage origImg = m_originalLayer->originalImage().copy();
        const QColor origBg = Config::isWhiteBackgroundImage ? Qt::white : Qt::black;
        for ( int y = 0; y < m_backup.height(); ++y ) {
            for ( int x = 0; x < m_backup.width(); ++x ) {
                if ( m_backup.pixelColor(x, y).alpha() > 128 )
                    origImg.setPixelColor(m_bounds.x()+x, m_bounds.y()+y, origBg);
            }
        }
        m_originalLayer->setOriginalImage(origImg);
    }
    if ( !m_newLayer->scene() && m_originalLayer->scene() ) {
      m_originalLayer->scene()->addItem(m_newLayer);
    }
    m_newLayer->setVisible(true);
    m_newLayer->setInActive(false);
    // controller handling
    if ( m_controller != nullptr ) {
     EditablePolygonCommand* editablePolyCommand = dynamic_cast<EditablePolygonCommand*>(m_controller);
     if ( editablePolyCommand != nullptr ) {
      editablePolyCommand->setVisible(false);
     }
    }
    m_originalLayer->update();
    MainWindow *window = dynamic_cast<MainWindow*>(m_newLayer->parent());
    if ( window != nullptr ) window->updateLayerList();
    printMessage();
  }
}

// ---------------------- JSON ----------------------
QJsonObject LassoCutCommand::toJson() const
{
   QJsonObject obj = AbstractCommand::toJson();
   obj["originalLayerId"] = m_originalLayerId;
   obj["newLayerId"] = m_newLayerId;
   obj["name"] = m_name;
   QJsonObject rectObj;
   rectObj.insert("x", m_bounds.x());
   rectObj.insert("y", m_bounds.y());
   rectObj.insert("width", m_bounds.width());
   rectObj.insert("height", m_bounds.height());
   obj["rect"] = rectObj;
   obj["type"] = "LassoCutCommand";
   return obj;
}

LassoCutCommand* LassoCutCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers, QUndoCommand* parent )
{
  qCDebug(logEditor) << "LassoCutCommand::fromJson(): Processing...";
  {
    // Original Layer
    const int originalLayerId = obj["originalLayerId"].toInt(-1);
    LayerItem* originalLayer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == originalLayerId ) {
            originalLayer = l;
            break;
        }
    }
    if ( !originalLayer ) {
      qWarning() << "LassoCutCommand::fromJson(): Original layer " << originalLayerId << " not found.";
      return nullptr;
    }
    // New Layer
    const int newLayerId = obj["newLayerId"].toInt(-1);
    LayerItem* newLayer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == newLayerId ) {
            newLayer = l;
            break;
        }
    }
    if ( !newLayer ) {
      qWarning() << "LassoCutCommand::fromJson(): New layer " << newLayerId << " not found.";
      return nullptr;
    }
    // >>>
    QString name = obj.value("name").toString("Unknown");
    QJsonObject r = obj["rect"].toObject();
    QRect rect(r["x"].toInt(),r["y"].toInt(),r["width"].toInt(),r["height"].toInt());
    // >>>
    return new LassoCutCommand(originalLayer,newLayer,rect,newLayer->originalImage(),newLayerId,name);
  }
}