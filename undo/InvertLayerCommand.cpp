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

#include "InvertLayerCommand.h"
#include "../layer/LayerItem.h"

#include <iostream>

// ------------------------------- helper ------------------------------- 
static void invertImage_old( QImage& img )
{
    // std::cout << "InvertLayerCommand::invertImage(): Processing..." << std::endl;
    if ( img.format() != QImage::Format_ARGB32 )
        img = img.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();
    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb px = line[x];
            line[x] = qRgba(
                255 - qRed(px),
                255 - qGreen(px),
                255 - qBlue(px),
                qAlpha(px)
            );
        }
    }
}

static void setImageLUT_old( QImage& img, const QVector<QRgb>& lut )
{
    // std::cout << "InvertLayerCommand::invertImage(): Processing..." << std::endl;
    if ( img.format() != QImage::Format_ARGB32 )
        img = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb src = line[x];
            int alpha = qAlpha(src);          // Alpha unverändert
            int r     = qRed(src);
            int g     = qGreen(src);
            int b     = qBlue(src);
            // LUT über Grauwert oder individuelle Map
            int gray = qGray(src);            // Index für LUT
            QRgb mapped = (gray < lut.size()) ? lut[gray] : qRgb(r,g,b);
            // Zusammensetzen mit Original-Alpha
            line[x] = qRgba(qRed(mapped), qGreen(mapped), qBlue(mapped), alpha);
        }
    }
}

// -------------------------------  ------------------------------- 
InvertLayerCommand::InvertLayerCommand( LayerItem* layer, const QVector<QRgb>& lut, int idx, QUndoCommand* parent )
    : AbstractCommand(parent)
    , m_layer(layer)
    , m_backup(layer->image())
    , m_layerId(idx)
    , m_lut(lut)
{
    setText("Changed Layer with LUT");
}

void InvertLayerCommand::undo()
{
    if ( !m_layer ) return;
    m_layer->image() = m_backup;
    m_layer->updatePixmap();
}

void InvertLayerCommand::redo()
{   
    if ( m_silent || !m_layer ) return;
    QImage& img = m_layer->image();
    const QImage& original = m_layer->originalImage(); // das Ausgangsbild
    if ( original.format() != QImage::Format_ARGB32 ) {
     qWarning() << "InvertLayerCommand(): Only supports ARGB32 format!";
     return;
    }
    img = original.copy();  // Basis für die LUT-Anwendung
    for ( int y = 0; y < img.height(); ++y ) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(original.constScanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QRgb src = srcLine[x];
            int alpha = qAlpha(src);
            int gray = qGray(src); // Index für LUT
            QRgb mapped = (gray < m_lut.size()) ? m_lut[gray] : src;
            dstLine[x] = qRgba(qRed(mapped), qGreen(mapped), qBlue(mapped), alpha);
        }
    }
    m_layer->updatePixmap();
}

// -------------- history related methods  -------------- 
QJsonObject InvertLayerCommand::toJson() const
{
   return {
     {"type", type()}
   };
}

InvertLayerCommand* InvertLayerCommand::fromJson( const QJsonObject& obj, const QList<LayerItem*>& layers )
{
    // Original Layer
    const int layerId = obj["layerId"].toInt(-1);
    LayerItem* layer = nullptr;
    for ( LayerItem* l : layers ) {
        if ( l->id() == layerId ) {
            layer = l;
            break;
        }
    }
    if ( !layer ) {
      qWarning() << "InvertLayerCommand::fromJson(): Original layer " << layerId << " not found.";
      return nullptr;
    }
    QVector<QRgb> lut;
    return new InvertLayerCommand(layer,lut);
}