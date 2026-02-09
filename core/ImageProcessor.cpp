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

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "Config.h"
#include "ImageProcessor.h"
#include "ImageLoader.h"

#include "../layer/LayerItem.h"
#include "../undo/AbstractCommand.h"
#include "../undo/PaintStrokeCommand.h"
#include "../undo/TransformLayerCommand.h"
#include "../undo/LassoCutCommand.h"
#include "../undo/MirrorLayerCommand.h"
#include "../undo/MoveLayerCommand.h"
#include "../undo/CageWarpCommand.h"

#include <iostream>

// ----------------------- Constructor -----------------------
ImageProcessor::ImageProcessor( const QImage& image ) : m_image(image) 
{
  qDebug() << "ImageProcessor::ImageProcessor(): Processing...";
  { 
   m_skipMainImage = true;
   m_undoStack = new QUndoStack();
   buildMainImageLayer();
  }
}

ImageProcessor::ImageProcessor()
{
  m_undoStack = new QUndoStack();
}

// ----------------------- Methods -----------------------
QString ImageProcessor::saveIntermediate( AbstractCommand *cmd, const QString &name, int step )
{
  if ( m_saveIntermediate && cmd != nullptr ) {
    QString outfilename = QString("%1/%2_%3.png").arg(m_intermediatePath).arg(m_basename).arg(1000+step);
    LayerItem *layer = cmd->layer();
    if ( layer != nullptr ) {
        qInfo() << layer->pos() << " - " << layer->image().rect();
        layer->image().save(outfilename);
        return QString("%1 %2 %3\n").arg(1000+step).arg(name).arg(outfilename);
    }
  }
  return "";
}

void ImageProcessor::buildMainImageLayer() {
  if ( !m_image.isNull() ) {
     LayerItem* newLayer = new LayerItem(m_image);
     newLayer->setName("MainImage");
     newLayer->setIndex(0);
     newLayer->setParent(nullptr);
     newLayer->setUndoStack(m_undoStack);
     m_layers << newLayer;
   }
}

void ImageProcessor::setIntermediatePath( const QString& path, const QString& outname ) 
{
    m_intermediatePath = path;
    if ( outname.length() > 0 ) {
       QFileInfo fileInfo(outname);
       m_basename = fileInfo.baseName();
    }
    m_saveIntermediate = path.length() > 0 ? true : false;
}

bool ImageProcessor::process( const QString& filePath ) 
{
 qDebug() << "ImageProcessor::load(): filePath='" << filePath << "'";
 { 
    QFile f(filePath);
    if ( !f.open(QIODevice::ReadOnly) ) {
     qDebug() << "ImageProcessor::load(): Cannot open '" << filePath << "'!";
     return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if ( !doc.isObject() ) return false;
    QJsonObject root = doc.object();
    
    // layers
    QJsonArray layerArray = root["layers"].toArray();
    if ( !m_skipMainImage ) {
      for ( const QJsonValue& v : layerArray ) {
        QJsonObject layerObj = v.toObject();
        QString name = layerObj["name"].toString();
        int id = layerObj["id"].toInt();
        if ( id == 0 ) {
          QString filename = layerObj["filename"].toString();
          QString pathname = layerObj["pathname"].toString();
          QString fullfilename = pathname+"/"+filename;
          ImageLoader loader;
          if ( loader.load(fullfilename,true) ) {
           m_image = loader.getImage();
           Config::isWhiteBackgroundImage = loader.hasWhiteBackground();
           buildMainImageLayer();
          } else {
            qDebug() << "ImageProcessor::load(): Cannot find '" << fullfilename << "'!";
            return false;
          }
        }
      }
    }
    int nCreatedLayers = m_layers.size();
    for ( const QJsonValue& v : layerArray ) {
      QJsonObject layerObj = v.toObject();
      int id = layerObj["id"].toInt();
      if ( id != 0 ) {
        QString name = layerObj["name"].toString();
        if ( layerObj.contains("data") ) {
         // qDebug() << "ImageProcessor::load(): Creating new layer " << name << ": id " << id << "...";
         QString imgBase64 = layerObj["data"].toString();
         QByteArray ba = QByteArray::fromBase64(imgBase64.toUtf8());
         QImage image;
         image.loadFromData(ba,"PNG");
         // >>>
         LayerItem* newLayer = new LayerItem(image);
         newLayer->setName(name);
         newLayer->setIndex(id);
         newLayer->setParent(nullptr);
         newLayer->setUndoStack(m_undoStack);
         m_layers << newLayer;
         nCreatedLayers += 1;
        }
      }
    }
  
    // --- Restore Undo/Redo Stack ---
    int nStep = 1;
    QString infoTextLines = "";
    QJsonArray undoArray = root["undoStack"].toArray();
    for ( const QJsonValue& v : undoArray ) {
        QJsonObject cmdObj = v.toObject();
        QString type = cmdObj["type"].toString();
        QString text = cmdObj["text"].toString();
        qDebug() << "ImageProcessor::load(): Found undo call: type=" << type << ", text=" << text;
        AbstractCommand* cmd = nullptr;
        if ( type == "PaintStrokeCommand" ) {
           cmd = PaintStrokeCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "LassoCutCommand" ) {
           cmd = LassoCutCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "MoveLayer" ) {
           cmd = MoveLayerCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "MirrorLayer" ) {
           cmd = MirrorLayerCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "CageWarp" ) {
           cmd = CageWarpCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "TransformLayerCommand" ) {
           cmd = TransformLayerCommand::fromJson(cmdObj, m_layers);
        } else {
           qDebug() << "ImageProcessor::load(): Not yet processed.";
        }
        // ggf. weitere Command-Typen hier hinzufügen
        if ( cmd ) {
            m_undoStack->push(cmd);
            infoTextLines += saveIntermediate(cmd,type,nStep);
        }
        nStep += 1;
    }
    if ( m_saveIntermediate && infoTextLines != "" ) {
      QString outfilename = QString("%1/%2.info").arg(m_intermediatePath).arg(m_basename);
      QFile file(outfilename);
      if ( file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        QTextStream out(&file);
         out << infoTextLines.trimmed();
        file.close();
      } else {
        qWarning() << "Warning: Cannot save info file " << outfilename << ": " << file.errorString();
      }
    }
    
    // ---  combine layer images ---
    if ( setOutputImage(0) ) {
      for ( auto* item : m_layers ) {
       auto* layer = dynamic_cast<LayerItem*>(item);
       if ( layer && layer->id() != 0 ) {
         QImage overlayImage = layer->image();
         if ( !overlayImage.isNull() ) {
          int x = layer->pos().x();
          int y = layer->pos().y();
          qInfo() << " layer=" << layer->name() << ", id=" << layer->id( )<< ", pos=" << layer->pos();
          QPainter painter(&m_outImage);
           painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
           painter.drawImage(x,y,overlayImage);
          painter.end();
         }
       }
      }
    } else {
      qInfo() << "Warning: Malfunction in ImageProcessor::setOutputImage().";
      return false;
    }
    
    return true;
 }
}

// ----------------------- Main image -----------------------
bool ImageProcessor::setOutputImage( int ident )
{
  qDebug() << "ImageProcessor::setOutputImage(): ident=" << ident;
  {
     for ( auto* item : m_layers ) {
       auto* layer = dynamic_cast<LayerItem*>(item);
       if ( layer && layer->id() == ident ) {
         m_outImage = layer->image(ident); 
         return true;
       }
     }
     return false;
  }
}

// ----------------------- Misc -----------------------
void ImageProcessor::printself()
{
  qDebug() << "ImageProcessor::printself(): Processing...";
  {
    for ( auto* item : m_layers ) {
     auto* layer = dynamic_cast<LayerItem*>(item);
     if ( layer ) {
       layer->printself();
     }
    }
  }
} 
