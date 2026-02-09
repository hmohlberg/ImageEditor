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

#include <QObject.h>
#include <QImage.h>

#include "ImageProcessor.h"

// -------------------------- ImageWoker --------------------------
class ImageWorker : public QObject {
    Q_OBJECT

private:
    ImageProcessor m_processor; // Der Worker nutzt den Prozessor

public slots:
    void process( const QString &path ) {
        QImage img(path);
        m_processor.applyFilter(img); // Delegiert die Arbeit an den Prozessor
        emit imageReady(img);         // Meldet Ergebnis zurück
    }
    
signals:
    void imageReady( const QImage &img );

};