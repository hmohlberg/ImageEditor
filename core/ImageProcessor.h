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

#pragma once

#include <QObject>
#include <QImage>
#include <QPixmap>
#include <QUndoStack>

// --- ---
class AbstractCommand;
class LayerItem;

// -------------------------- ImageProcessor --------------------------
class ImageProcessor {

public:

    ImageProcessor();
    ImageProcessor( const QImage& image );
    
    QImage getOutputImage() const { return m_outImage; }
    
    // --------------------------  --------------------------
    void setIntermediatePath( const QString& path = "", const QString& outname = "" );
    bool setOutputImage( int ident );
    bool process( const QString& filePath );
    void printself();
   
    
private:

    QString saveIntermediate( AbstractCommand *cmd, const QString &name, int step );

    bool m_skipMainImage = false;
    bool m_saveIntermediate = false;
   
    QImage m_image;
    QImage m_outImage;
    
    QString m_intermediatePath = "";
    QString m_basename = "";
    
    QUndoStack* m_undoStack = nullptr;
    
    QList<LayerItem*> m_layers;
    
    void buildMainImageLayer();

};