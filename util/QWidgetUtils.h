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

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QBrush>

namespace QWidgetUtils
{

  QWidget* getSeparatorLine() {
    QWidget* line = new QWidget();
    line->setFixedWidth(1);
    line->setStyleSheet("background-color: #A5A5A5;");
    line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return line;
  }
  
  QBrush createCheckerBrush( int tileSize=16, QColor c1=QColor("#3a3a3a"), QColor c2=QColor("#2a2a2a") ) {
    QPixmap pix(tileSize*2,tileSize*2);
    pix.fill(c1);
    QPainter p(&pix);
     p.fillRect(QRect(0, 0, tileSize, tileSize), c2);
     p.fillRect(QRect(tileSize, tileSize, tileSize, tileSize), c2);
    p.end();
    return QBrush(pix);
  }

}