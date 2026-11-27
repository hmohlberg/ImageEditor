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

#include <QBuffer>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>

#include "Config.h"
#include "ImageProcessor.h"
#include "ImageLoader.h"
#include "PolygonLayerData.h"

#include "../layer/LayerItem.h"
#include "../layer/EditablePolygon.h"
#include "../undo/AbstractCommand.h"
#include "../undo/PaintStrokeCommand.h"
#include "../undo/PerspectiveWarpCommand.h"
#include "../undo/TransformLayerCommand.h"
#include "../undo/DeleteUndoEntryCommand.h"
#include "../undo/LassoCutCommand.h"
#include "../undo/MirrorLayerCommand.h"
#include "../undo/MoveLayerCommand.h"
#include "../undo/CageWarpCommand.h"
#include "../undo/SetPivotCommand.h"

#include <iostream>
#include <algorithm>

#include <QMap>
#include <QSet>

// ──────────────────────────────────────────────────────────────────────────────
// concatenateTransforms helpers (file-scope)
// ──────────────────────────────────────────────────────────────────────────────

static QTransform readTransform(const QJsonObject& o)
{
    return QTransform(
        o["m11"].toDouble(1), o["m12"].toDouble(0), o["m13"].toDouble(0),
        o["m21"].toDouble(0), o["m22"].toDouble(1), o["m23"].toDouble(0),
        o["m31"].toDouble(0), o["m32"].toDouble(0), o["m33"].toDouble(1));
}

static QJsonObject writeTransform(const QTransform& t)
{
    QJsonObject o;
    o["m11"]=t.m11(); o["m12"]=t.m12(); o["m13"]=t.m13();
    o["m21"]=t.m21(); o["m22"]=t.m22(); o["m23"]=t.m23();
    o["m31"]=t.m31(); o["m32"]=t.m32(); o["m33"]=t.m33();
    return o;
}

static QJsonArray transformPoints(const QJsonArray& pts, const QTransform& T)
{
    QJsonArray result;
    for (const QJsonValue& v : pts) {
        const QJsonObject p = v.toObject();
        const QPointF q = T.map(QPointF(p["x"].toDouble(), p["y"].toDouble()));
        QJsonObject r;
        r["x"] = q.x();
        r["y"] = q.y();
        result.append(r);
    }
    return result;
}

// Returns the layerId for geometric transform commands, or -1 for non-geometric.
static int cmdLayerId(const QJsonObject& cmd)
{
    const QString t = cmd["type"].toString();
    if (t == "TransformLayer" || t == "MoveLayer" || t == "MirrorLayer" ||
        t == "PerspectiveWarp" || t == "CageWarp")
        return cmd["layerId"].toInt(-1);
    return -1;
}

// ──────────────────────────────────────────────────────────────────────────────
// QJsonArray ImageProcessor::concatenateTransforms()
//
// Replaces consecutive TransformLayer sequences (same layer) with a single
// composed TransformLayer, and absorbs surrounding TransformLayer runs into
// each CageWarp's control points to reduce the number of interpolation steps.
//
// Rules:
//  - Only TransformLayer (rotate / scale) runs are composed via QTransform
//    multiplication; MoveLayer, MirrorLayer, PerspectiveWarp act as separators.
//  - A CageWarp absorbs its preceding TransformLayer run as a PRE-affine
//    (before-points transformed by T_pre^{-1}) and its following TransformLayer
//    run as a POST-affine (after-points transformed by T_post).
//  - Non-geometric commands (PaintStroke, LassoCut, …) act as separators but
//    are emitted unchanged.
// ──────────────────────────────────────────────────────────────────────────────
QJsonArray ImageProcessor::concatenateTransforms(const QJsonArray& undoStack) const
{
    // ── Step 1: collect per-layer chains ─────────────────────────────────────
    // layerId → ordered list of {globalIndex, cmd}
    QMap<int, QList<QPair<int,QJsonObject>>> chains;
    for (int i = 0; i < undoStack.size(); ++i) {
        const QJsonObject cmd = undoStack[i].toObject();
        const int lid = cmdLayerId(cmd);
        if (lid >= 0) chains[lid].append({i, cmd});
    }

    // Maps for output construction
    QMap<int, QJsonObject> replacements; // globalIndex → replacement command
    QSet<int>              skipSet;      // globalIndex → absorbed, omit from output

    // ── Step 2: process each layer's chain ───────────────────────────────────
    for (auto it = chains.cbegin(); it != chains.cend(); ++it) {
        const int          layerId = it.key();
        const auto&        ops     = it.value();

        // Log the chain
        qInfo() << "[concatenate] Layer" << layerId << "— chain:";
        for (const auto& [idx, cmd] : ops)
            qInfo() << "  [" << idx << "]" << cmd["type"].toString();

        // ── per-layer processing state ────────────────────────────────────────
        // Accumulate a run of consecutive TransformLayer commands
        struct Run {
            QList<int>  opIdx;      // indices into ops[]
            QTransform  composed;   // T_n * … * T_1
            QJsonObject firstCmd;
            QJsonObject lastCmd;
            bool empty() const { return opIdx.isEmpty(); }
            void clear()       { opIdx.clear(); composed = QTransform(); firstCmd = {}; lastCmd = {}; }
        };

        Run   currentRun;
        int   lastCageOpIdx = -1;   // index into ops[] of the last seen CageWarp

        // Emit the accumulated run as a single merged TransformLayer.
        // If the run has only one command, nothing changes; otherwise the first
        // entry is replaced and the rest are skipped.
        auto flushRun = [&]() {
            if (currentRun.empty()) return;
            if (currentRun.opIdx.size() > 1) {
                const int first = ops[currentRun.opIdx.first()].first;
                QJsonObject merged = currentRun.firstCmd;
                merged["newTransform"] = writeTransform(currentRun.composed);
                merged["newPosition"]  = currentRun.lastCmd["newPosition"];
                merged["text"]         = QString("Concatenated transforms (layer %1)").arg(layerId);
                replacements[first]    = merged;
                for (int k = 1; k < currentRun.opIdx.size(); ++k)
                    skipSet.insert(ops[currentRun.opIdx[k]].first);
                qInfo() << "[concatenate] Layer" << layerId << ": merged"
                        << currentRun.opIdx.size() << "TransformLayer ops → 1";
            }
            currentRun.clear();
        };

        // Absorb the current run into CageWarp at ops[cageOpIdx].
        // isPre=true  → transform before-points by T_run^{-1}
        // isPre=false → transform after-points  by T_run
        auto absorbRun = [&](int cageOpIdx, bool isPre) {
            if (currentRun.empty()) return;
            const int        cageGlobal = ops[cageOpIdx].first;
            QJsonObject      cage = replacements.contains(cageGlobal)
                                    ? replacements[cageGlobal]
                                    : ops[cageOpIdx].second;

            if (isPre) {
                bool ok = false;
                const QTransform Tinv = currentRun.composed.inverted(&ok);
                if (ok) {
                    cage["cagepoints_before"] = transformPoints(
                        cage["cagepoints_before"].toArray(), Tinv);
                    // pull the bounding rect back to pre-transform space
                    const QJsonObject r = cage["rect"].toObject();
                    const QRectF      newRect = Tinv.mapRect(QRectF(
                        r["x"].toDouble(), r["y"].toDouble(),
                        r["width"].toDouble(), r["height"].toDouble()));
                    QJsonObject nr;
                    nr["x"] = newRect.x(); nr["y"] = newRect.y();
                    nr["width"] = newRect.width(); nr["height"] = newRect.height();
                    cage["rect"] = nr;
                    for (int k : currentRun.opIdx) skipSet.insert(ops[k].first);
                    qInfo() << "[concatenate] Layer" << layerId << ": absorbed"
                            << currentRun.opIdx.size() << "pre-affines into CageWarp";
                } else {
                    // non-invertible: fall back to plain merge
                    flushRun();
                    currentRun.clear();
                    return;
                }
            } else {
                cage["cagepoints_after"] = transformPoints(
                    cage["cagepoints_after"].toArray(), currentRun.composed);
                // update top-left position
                const QJsonObject tla = cage["topLeft_after"].toObject();
                const QPointF     newTL = currentRun.composed.map(
                    QPointF(tla["x"].toDouble(), tla["y"].toDouble()));
                QJsonObject ntla;
                ntla["x"] = newTL.x(); ntla["y"] = newTL.y();
                cage["topLeft_after"] = ntla;
                for (int k : currentRun.opIdx) skipSet.insert(ops[k].first);
                qInfo() << "[concatenate] Layer" << layerId << ": absorbed"
                        << currentRun.opIdx.size() << "post-affines into CageWarp";
            }
            replacements[cageGlobal] = cage;
            currentRun.clear();
        };

        // ── scan the layer's chain ────────────────────────────────────────────
        for (int k = 0; k < ops.size(); ++k) {
            const QString type = ops[k].second["type"].toString();

            if (type == "TransformLayer") {
                const QTransform T = readTransform(ops[k].second["newTransform"].toObject());
                if (currentRun.empty()) {
                    currentRun.opIdx    = {k};
                    currentRun.composed = T;
                    currentRun.firstCmd = ops[k].second;
                    currentRun.lastCmd  = ops[k].second;
                } else {
                    currentRun.opIdx.append(k);
                    currentRun.composed = T * currentRun.composed; // newest on left
                    currentRun.lastCmd  = ops[k].second;
                }
            } else if (type == "CageWarp") {
                if (lastCageOpIdx >= 0) {
                    // post-affines of the previous cage
                    absorbRun(lastCageOpIdx, /*isPre=*/false);
                } else {
                    // pre-affines of this cage
                    absorbRun(k, /*isPre=*/true);
                    lastCageOpIdx = k;
                    continue;
                }
                lastCageOpIdx = k;
            } else {
                // separator: flush/absorb pending run
                if (lastCageOpIdx >= 0) {
                    absorbRun(lastCageOpIdx, /*isPre=*/false);
                    lastCageOpIdx = -1;
                } else {
                    flushRun();
                }
            }
        }

        // End of chain: finalise
        if (lastCageOpIdx >= 0)
            absorbRun(lastCageOpIdx, /*isPre=*/false);
        else
            flushRun();
    }

    // ── Step 3: build output array ────────────────────────────────────────────
    QJsonArray result;
    for (int i = 0; i < undoStack.size(); ++i) {
        if (skipSet.contains(i)) continue;
        result.append(replacements.contains(i) ? replacements[i] : undoStack[i]);
    }
    qInfo() << "[concatenate] Stack reduced from" << undoStack.size() << "to" << result.size() << "commands.";
    return result;
}

// ----------------------- Constructor -----------------------
ImageProcessor::ImageProcessor( const QImage& image ) : m_image(image) 
{
  qDebug() << "ImageProcessor::ImageProcessor(): Processing...";
  { 
   m_skipMainImage = !image.isNull();
   m_undoStack = new GuardedUndoStack();
   buildMainImageLayer();
  }
}

ImageProcessor::ImageProcessor()
{
  m_undoStack = new GuardedUndoStack();
}

// ----------------------- Methods -----------------------
QImage ImageProcessor::compositeLayers() const
{
  QImage result;
  for ( auto* item : m_layers ) {
    auto* layer = dynamic_cast<LayerItem*>(item);
    if ( layer && layer->id() == 0 ) {
      result = layer->image();
      break;
    }
  }
  if ( result.isNull() ) return result;

  const bool grayscale = result.format() == QImage::Format_Grayscale8;
  QList<LayerItem*> overlays;
  for (auto* layer : m_layers) {
    if (layer && layer->id() != 0 && !layer->isDeleted() && !layer->image().isNull())
      overlays.append(layer);
  }
  if (overlays.isEmpty())
    return result;
  // Project loading in the editor stacks layers by ID and blends in ARGB32.
  std::stable_sort(overlays.begin(), overlays.end(),
                   [](const LayerItem* a, const LayerItem* b) { return a->id() < b->id(); });
  result.convertTo(QImage::Format_ARGB32);
  QPainter painter(&result);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
  for (auto* layer : overlays) {
    painter.drawImage(int(layer->pos().x()), int(layer->pos().y()), layer->image());
  }
  painter.end();
  if (grayscale) {
    // Preserve the batch grayscale output without converting its sample values.
    result.setColorSpace(QColorSpace());
    result.convertTo(QImage::Format_Grayscale8);
  }
  return result;
}

QString ImageProcessor::saveIntermediate( AbstractCommand *cmd, const QString &name, int step )
{
  if ( m_saveIntermediate ) {
    QString outfilename = QString("%1_%2.png").arg(m_intermediateBase).arg(step, 4, 10, QChar('0'));
    QImage composited = compositeLayers();
    if ( !composited.isNull() ) {
      composited.save(outfilename);
      return QString("%1 %2 %3\n").arg(step, 4, 10, QChar('0')).arg(name).arg(outfilename);
    }
  }
  return "";
}

void ImageProcessor::buildMainImageLayer() {
  if ( !m_image.isNull() ) {
     LayerItem* newLayer = new LayerItem("MainImage",m_image);
     newLayer->setName("MainImage");
     newLayer->setIndex(0);
     newLayer->setParent(nullptr);
     newLayer->setUndoStack(m_undoStack);
     m_layers << newLayer;
   }
}

void ImageProcessor::setIntermediatePath( const QString& templateFile )
{
    if ( templateFile.isEmpty() ) {
        m_saveIntermediate = false;
        m_intermediateBase = "";
        return;
    }
    if ( templateFile.endsWith(".png", Qt::CaseInsensitive) )
        m_intermediateBase = templateFile.left(templateFile.length() - 4);
    else
        m_intermediateBase = templateFile;
    m_saveIntermediate = true;
}

static bool compactPolygonLayer(QJsonObject& layer, QJsonArray& history, const QImage& source,
                                const PolygonLayerData::Recipe& recipe,
                                const QImage& layerImage, const QImage& embeddedMask)
{
    if (layerImage.isNull())
        return false;

    const QImage expected = layerImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const auto matches = [&](const QPolygonF& points) {
        const QImage rebuilt = PolygonLayerData::extract(source, points, recipe.bounds);
        return PolygonLayerData::samePixels(rebuilt, expected) &&
               (embeddedMask.isNull() || PolygonLayerData::samePixels(
                    PolygonLayerData::binaryMask(rebuilt), embeddedMask));
    };

    QJsonObject polygon = history[recipe.polygonIndex].toObject();
    const QPolygonF points = PolygonLayerData::cutPoints(polygon);
    if (!matches(points)) {
        const QJsonArray edits = polygon["undo"].toArray();
        if (points.size() < 3 || edits.isEmpty() || polygon.contains("layerPoints"))
            return false;
        EditablePolygon model("ImageProcessor::process()");
        model.setPolygon(points);
        model.undoStackFromJson(edits);
        const QPolygonF edited = model.polygon();
        if (edited == points || !matches(edited))
            return false;
        polygon["layerPoints"] = PolygonLayerData::pointsToJson(edited);
        history[recipe.polygonIndex] = polygon;
    }

    layer.remove("data");
    layer.remove("x");
    layer.remove("y");
    layer["rebuildFromPolygon"] = true;
    layer["binaryMask"] = true; // --save-json exports ordinary cut layers as binary masks.
    return true;
}

bool ImageProcessor::process( const QString& filePath, bool forcedAlphaMasking, bool processHistory ) 
{
 qDebug() << "ImageProcessor::process(): filePath='" << filePath << "', forcedAlphaMasking =" << forcedAlphaMasking << ", processHistory =" << processHistory;
 { 
    QFile f(filePath);
    if ( !f.open(QIODevice::ReadOnly) ) {
     qDebug() << LogColor::Red << "ImageProcessor::process(): Cannot open '" << filePath << "'!" << LogColor::Reset;
     return false;
    }
    m_jsonDocument = QJsonDocument::fromJson(f.readAll());
    f.close();
    if ( !m_jsonDocument.isObject() ) return false;
    QJsonObject root = m_jsonDocument.object();
    
    // layers
    QJsonArray updatedLayers;
    QJsonArray layerArray = root["layers"].toArray();
    if ( !m_skipMainImage ) {
      bool haveMainImage = false;
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
            qDebug() << LogColor::Red << "ImageProcessor::process(): Cannot find '" << fullfilename << "'!" << LogColor::Reset;
            if (processHistory)
                return false;
          }
          haveMainImage = true;
        }
      }
      if ( !haveMainImage && processHistory ) {
        qDebug() << LogColor::Red << "ImageProcessor::process(): The main image is missing from the project file. Use the --file option to specify it.";
        return false;
      }
    }
    QHash<int, PolygonLayerData::Cut> polygonCuts;
    int invalidPolygonLayer = -1;
    if (!PolygonLayerData::rebuild(root, m_image, polygonCuts, invalidPolygonLayer)) {
        qWarning() << "Could not rebuild polygon layer from the project history:" << invalidPolygonLayer;
        return false;
    }

    QJsonArray undoStack = root["undoStack"].toArray();
    const bool compact = EditorStyle::instance().compactPolygonLayers();
    const auto cutRecipes = (processHistory || compact) && !m_image.isNull()
        ? PolygonLayerData::recipes(undoStack, m_image.size())
        : QHash<int, PolygonLayerData::Recipe>();
    QHash<int, QImage> polygonCutImages;

    // loading layers
    qInfo() << "Processing layer stack...";
    int nCreatedLayers = m_layers.size();
    for ( const QJsonValue& v : layerArray ) {
     if ( v.isObject() ) {
      QJsonObject layerObj = v.toObject();
      QString name    = layerObj["name"].toString();
      QString creator = layerObj.value("creator").toString();
      int id = layerObj["id"].toInt();
      qInfo() << " " << name << ": id =" << id;
      if ( id != 0 ) {
        const auto polygonCut = polygonCuts.constFind(id);
        if (polygonCut != polygonCuts.cend()) {
          auto* layer = new LayerItem(name, polygonCut->image);
          layer->setIndex(id);
          layer->setUndoStack(m_undoStack);
          m_layers << layer;
          ++nCreatedLayers;
          if (processHistory)
              polygonCutImages.insert(id, polygonCut->image);
          if (!processHistory && !compact) {
              const bool binary = layerObj["binaryMask"].toBool();
              const QImage image = binary ? PolygonLayerData::binaryMask(polygonCut->image) : polygonCut->image;
              QByteArray bytes;
              QBuffer buffer(&bytes);
              if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG")) {
                  qWarning() << "Could not encode polygon layer:" << id;
                  return false;
              }
              layerObj.remove("rebuildFromPolygon");
              layerObj["data"] = QString::fromLatin1(bytes.toBase64());
              layerObj["x"] = polygonCut->bounds.x();
              layerObj["y"] = polygonCut->bounds.y();
          }
          updatedLayers.append(layerObj);
        } else if ( layerObj.contains("data") ) {
         LayerItem* newLayer = nullptr;
         QString imgBase64 = layerObj["data"].toString();
         QByteArray ba = QByteArray::fromBase64(imgBase64.toUtf8());
         QImage mask;
         if (!mask.loadFromData(ba, "PNG") && !processHistory) {
             qWarning() << "Could not decode embedded image for layer:" << id;
             return false;
         }
         bool isBinaryMask = layerObj.value("binaryMask").toBool(false);
         int x = layerObj.value("x").toInt(-1);
         int y = layerObj.value("y").toInt(-1);
         if ( !(  x > 0 && y > 0 ) ) {
           QJsonArray undoArray = root["undoStack"].toArray();
           for ( const QJsonValue& v : undoArray ) {
             QJsonObject cmdObj = v.toObject();
             QString type = cmdObj["type"].toString();
             if ( type == "LassoCut" || type == "LassoCutCommand" ) {
               if( id == cmdObj["newLayerId"].toInt(-1) ) {
                 QJsonObject r = cmdObj["rect"].toObject();
                 x = r["x"].toInt();
                 y = r["y"].toInt();
               }
             }
           }
         }
         QRect rect = QRect(x,y,mask.width(), mask.height());
         if ( isBinaryMask && x >= 0 && y >= 0 && !m_image.isNull() ) {
          QImage subImage = m_image.copy(x, y, mask.width(), mask.height());
          subImage = subImage.convertToFormat(QImage::Format_ARGB32);
          int backgroundPixelColor = Config::isWhiteBackgroundImage ? 255 : 0;
          for ( int y = 0; y < subImage.height(); ++y ) {
            QRgb *rowData = reinterpret_cast<QRgb*>(subImage.scanLine(y));
            const uchar *maskData = mask.constScanLine(y);
            for ( int x = 0; x < subImage.width(); ++x ) {
             // if ( maskData[x] != 255 ) {
             //  rowData[x] = qRgba(qRed(rowData[x]), qGreen(rowData[x]), qBlue(rowData[x]), 0);
             // }
             if ( maskData[x] != 255 || (qRed(rowData[x]) == backgroundPixelColor && qGreen(rowData[x]) == backgroundPixelColor && qBlue(rowData[x]) == backgroundPixelColor) ) {
               rowData[x] = qRgba(qRed(rowData[x]), qGreen(rowData[x]), qBlue(rowData[x]), 0);
             }
            }
          }
          newLayer = new LayerItem("SubImage",subImage);
         } else if ( forcedAlphaMasking && !m_image.isNull() ) {
          if ( mask.format() != QImage::Format_ARGB32 && mask.format() != QImage::Format_ARGB32_Premultiplied ) {
            mask = mask.convertToFormat(QImage::Format_ARGB32);
          }
          QImage subImage = m_image.copy(x, y, mask.width(), mask.height());
          subImage = subImage.convertToFormat(QImage::Format_ARGB32);
          int backgroundPixelColor = Config::isWhiteBackgroundImage ? 255 : 0;
          for ( int y = 0; y < subImage.height(); ++y ) {
            auto *rowData = reinterpret_cast<QRgb*>(subImage.scanLine(y));
            const auto *maskRowData = reinterpret_cast<const QRgb*>(mask.constScanLine(y));  
            for ( int x = 0; x < subImage.width(); ++x ) {
              // OLD: if ( qRed(rowData[x]) == backgroundPixelColor || qAlpha(maskRowData[x]) != 255 ) {
              if ( qRed(rowData[x]) == backgroundPixelColor && qGreen(rowData[x]) == backgroundPixelColor && qBlue(rowData[x]) == backgroundPixelColor && qAlpha(maskRowData[x]) == 255 ) {
                rowData[x] = 0; 
              }
            }
          }
          newLayer = new LayerItem("SubImage",subImage); 
         } else if ( creator == "Inpainting" ) {
          if ( mask.format() != QImage::Format_ARGB32 )
            mask = mask.convertToFormat(QImage::Format_ARGB32);
          newLayer = new LayerItem("InpaintResult", mask);
          newLayer->setPos(QPointF(x, y));
         } else {
          newLayer = new LayerItem("MaskImage",mask);
         }
         newLayer->setName(name);
         newLayer->setIndex(id);
         newLayer->setParent(nullptr);
         newLayer->setUndoStack(m_undoStack);
         m_layers << newLayer;
         nCreatedLayers += 1;
         const auto recipe = cutRecipes.constFind(id);
         if (recipe != cutRecipes.cend() && creator != "Inpainting" && !layerObj.contains("srcLayerId")) {
             if (processHistory) {
                 // Keep the cut alpha and discard hidden RGB just as reconstruction does.
                 const auto cut = newLayer->image().convertToFormat(QImage::Format_ARGB32_Premultiplied);
                 newLayer->setOriginalImage(cut);
                 newLayer->setImage(cut);
                 polygonCutImages.insert(id, cut);
             } else if (compactPolygonLayer(layerObj, undoStack, m_image, recipe.value(), newLayer->image(),
                                           isBinaryMask ? mask.convertToFormat(QImage::Format_Grayscale8) : QImage())) {
                 updatedLayers.append(layerObj);
                 continue;
             }
         }
         // build new json stack
         if ( !isBinaryMask && creator != "Inpainting" ) {
          layerObj["data"] = newLayer->getAlphaMaskData();
          layerObj["binaryMask"] = true;
          layerObj["x"] = x;
          layerObj["y"] = y;
         }
         updatedLayers.append(layerObj);
        } else {
         updatedLayers.append(layerObj);
        }
      } else {
        updatedLayers.append(layerObj);
      }
     } else {
      updatedLayers.append(v);
     }
    }
    // loading undoStack
    qInfo() << "Processing undo stack...";
    QJsonArray updateUndoStack;
    const bool stripWarpCache = !processHistory && !EditorStyle::instance().saveCageWarpCache();
    for ( const QJsonValue& v : undoStack ) {
     if ( v.isObject() ) {
      QJsonObject layerObj = v.toObject();
      QString name = layerObj["text"].toString();
      QString type = layerObj["type"].toString();
      if ( type == "CageWarp" ) {
       QJsonObject topLeft = layerObj["topLeft_after"].toObject();
       double x = topLeft["x"].toDouble();
       double y = topLeft["y"].toDouble();
       qInfo() << " " << name << ": type =" << type << ", topLeftPos = (" << x << ":" << y << ")";
       topLeft["x"] = qRound(x);
       topLeft["y"] = qRound(y);
       layerObj["topLeft_after"] = topLeft;
       QJsonObject rect = layerObj["rect"].toObject();
       rect["x"] = qRound(rect["x"].toDouble());
       rect["y"] = qRound(rect["y"].toDouble());
       layerObj["rect"] = rect;
      } else if ( type == "MoveLayer" ) {
       double fromX = layerObj["fromX"].toDouble();
       double fromY = layerObj["fromY"].toDouble();
       double toX = layerObj["toX"].toDouble();
       double toY = layerObj["toY"].toDouble();
       qInfo() << " " << name << ": type =" << type << ", from (" << fromX << ":" << fromY << ") to (" << toX << ":" << toY << ")";
       layerObj["fromX"] = qRound(fromX);
       layerObj["fromY"] = qRound(fromY);
       layerObj["toX"] = qRound(toX);
       layerObj["toY"] = qRound(toY);
      } else {
       qInfo() << " " << name << ": type =" << type;
      }
      if (stripWarpCache && (type == "CageWarp" || type == "CageWarpCommand")) {
        layerObj.remove("warpedPhysicalImageData");
        layerObj.remove("warpedPhysicalImageVersion");
      }
      updateUndoStack.append(layerObj);
     }
    }
    // output
    if ( !processHistory ) {
      root["layers"] = updatedLayers;
      root["undoStack"] = updateUndoStack;
      m_jsonDocument.setObject(root);
      return true;
    }
  
    // --- Restore Undo/Redo Stack ---
    int nStep = 1;
    QString infoTextLines = "";
    QVector<QRgb> identityLut(256);
    for (int i = 0; i < identityLut.size(); ++i)
        identityLut[i] = qRgb(i, i, i);
    QJsonArray undoArray = root["undoStack"].toArray();
    if ( Config::concatenate )
        undoArray = concatenateTransforms(undoArray);
    for ( const QJsonValue& v : undoArray ) {
      QJsonObject cmdObj = v.toObject();
      QString type = cmdObj["type"].toString();
      QString text = cmdObj["text"].toString();
      qDebug() << "ImageProcessor::process(): Processing undo call: type=" << type << ", text=" << text;
      if ( processHistory ) { 
        AbstractCommand* cmd = nullptr;
        if ( type == "PaintStroke" || type == "PaintStrokeCommand" ) {
           cmd = PaintStrokeCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "LassoCut" || type == "LassoCutCommand" ) {
           const QImage cutImage = polygonCutImages.value(cmdObj["newLayerId"].toInt(-1));
           cmd = LassoCutCommand::fromJson(cmdObj, m_layers, nullptr, cutImage);
        } else if ( type == "MoveLayer" || type == "MoveLayerCommand" ) {
           cmd = MoveLayerCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "MirrorLayer" || type == "MirrorLayerCommand" ) {
           cmd = MirrorLayerCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "CageWarp" || type == "CageWarpCommand" ) {
           cmd = CageWarpCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "TransformLayer" || type == "TransformLayerCommand" ) {
           cmd = TransformLayerCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "PerspectiveWarp" || type == "PerspectiveWarpCommand" ) {
           cmd = PerspectiveWarpCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "SetPivot" ) {
           cmd = SetPivotCommand::fromJson(cmdObj, m_layers);
        } else if ( type == "DeleteUndoEntry" || type == "DeleteUndoEntryCommand" ) {
           cmd = DeleteUndoEntryCommand::fromJson(m_undoStack, cmdObj, m_layers);
        } else {
           qDebug() << LogColor::Red << "ImageProcessor::process(): Command " << type << " not yet processed." << LogColor::Reset;
        }
        // ggf. weitere Command-Typen hier hinzufügen
        if ( cmd ) {
            LayerItem* layer = cmd->layer();
            const bool updateLayerAlpha = layer && !layer->alphaMask().isNull() &&
                (dynamic_cast<TransformLayerCommand*>(cmd) || dynamic_cast<MirrorLayerCommand*>(cmd));
            m_undoStack->push(cmd);
            if (updateLayerAlpha)
                layer->applyLutWithTransform(identityLut);
            infoTextLines += saveIntermediate(cmd,type,nStep);
        } else {
            qDebug() << LogColor::Red << "ImageProcessor::process(): Invalid command." << LogColor::Reset;
        }
      }
      nStep += 1;
    }
    if ( m_saveIntermediate && infoTextLines != "" ) {
      QString outfilename = QString("%1.info").arg(m_intermediateBase);
      QFile file(outfilename);
      if ( file.open(QIODevice::WriteOnly | QIODevice::Text) ) {
        QTextStream out(&file);
         out << infoTextLines.trimmed();
        file.close();
      } else {
        qWarning() << "Warning: Cannot save info file " << outfilename << ": " << file.errorString();
      }
    }
    
    m_outImage = compositeLayers();
    return !m_outImage.isNull();
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
