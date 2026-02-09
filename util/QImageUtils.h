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

#include <QImage>

namespace QImageUtils
{

  inline bool hasBlackBackground( const QImage &img ) {
    if ( img.isNull() ) return false;
    QColor c1 = img.pixelColor(0, 0);
    QColor c2 = img.pixelColor(img.width() - 1, 0);
    QColor c3 = img.pixelColor(0, img.height() - 1);
    QColor c4 = img.pixelColor(img.width() - 1, img.height() - 1);
    return (c1 == Qt::black && c2 == Qt::black && c3 == Qt::black && c4 == Qt::black);
  }

  inline QImage blurAlphaMask( const QImage& src, int radius )
  {
    if ( radius <= 0 )
        return src;
    // Downscale-Faktor
    const int scale = qMax(1, radius / 2);
    QSize smallSize(
        qMax(1, src.width()  / scale),
        qMax(1, src.height() / scale)
    );
    QImage small = src.scaled(
        smallSize,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
    );
    QImage blurred = small.scaled(
        src.size(),
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
    );
    return blurred;
  }
  
}