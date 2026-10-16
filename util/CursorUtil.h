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
*/

#pragma once

#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>

inline void setEnlargedStandardCursor( int targetSize,
                                       const QColor& fillColor   = Qt::white,
                                       const QColor& borderColor = Qt::black )
{
    if ( targetSize == 0 ) return;

    QIcon arrowIcon = QIcon::fromTheme("cursor-arrow");
    if ( arrowIcon.isNull() )
        arrowIcon = QIcon::fromTheme("arrow");
    if ( !arrowIcon.isNull() ) {
        QPixmap pixmap = arrowIcon.pixmap(targetSize, targetSize);
        if ( !pixmap.isNull() ) {
            QApplication::setOverrideCursor(QCursor(
                pixmap.scaled(targetSize, targetSize,
                              Qt::KeepAspectRatio, Qt::SmoothTransformation), 0, 0));
            return;
        }
    }

    QPixmap fallbackPixmap(targetSize, targetSize);
    fallbackPixmap.fill(Qt::transparent);
    QPainter painter(&fallbackPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPolygon pfeil;
    pfeil << QPoint(0, 0)
          << QPoint(0, targetSize * 0.75)
          << QPoint(targetSize * 0.22, targetSize * 0.53)
          << QPoint(targetSize * 0.47, targetSize * 0.95)
          << QPoint(targetSize * 0.58, targetSize * 0.89)
          << QPoint(targetSize * 0.33, targetSize * 0.48)
          << QPoint(targetSize * 0.55, targetSize * 0.48);
    painter.setPen(QPen(borderColor, 2));
    painter.setBrush(fillColor);
    painter.drawPolygon(pfeil);
    painter.end();
    QApplication::setOverrideCursor(QCursor(fallbackPixmap, 0, 0));
}
