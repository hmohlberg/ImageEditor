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

#include "PaintCommand.h"

#include "../layer/LayerItem.h"

#include <QPainter>
#include <QtMath>
#include <QRect>

PaintCommand::PaintCommand( LayerItem* layer, const QPoint& pos, const QColor& color, int radius, qreal hardness )
    : m_layer(layer), m_image(&(layer->image())), m_pos(pos), m_color(color), m_radius(radius), m_hardness(hardness)
{
    QString desc = QString("Paint on Layer at (%1:%2)").arg(pos.x()).arg(pos.y());
    setText(desc);
    // Backup nur des zu bearbeitenden Bereichs
    QRect r(pos.x()-radius, pos.y()-radius, radius*2+1, radius*2+1);
    r = r.intersected(m_image->rect());
    m_backup = m_image->copy(r);
    redo();
}

void PaintCommand::undo()
{
    if ( !m_layer || !m_image || m_image->isNull() ) return;
    QRect r(m_pos.x()-m_radius, m_pos.y()-m_radius, m_radius*2+1, m_radius*2+1);
    r = r.intersected(m_image->rect());
    QPainter painter(m_image);
    painter.drawImage(r.topLeft(), m_backup);
    m_layer->updatePixmap();
}

void PaintCommand::redo()
{
    if ( !m_image || m_image->isNull() ) return;
    const int left   = qMax(0, m_pos.x() - m_radius);
    const int top    = qMax(0, m_pos.y() - m_radius);
    const int right  = qMin(m_image->width()-1,  m_pos.x() + m_radius);
    const int bottom = qMin(m_image->height()-1, m_pos.y() + m_radius);
    const int brushRad2 = m_radius*m_radius;
    const int r_col = m_color.red();
    const int g_col = m_color.green();
    const int b_col = m_color.blue();
    for ( int y = top; y <= bottom; ++y ) {
        int dy = y - m_pos.y();
        int dy2 = dy*dy;
        for ( int x = left; x <= right; ++x ) {
            int dx = x - m_pos.x();
            int dist2 = dx*dx + dy2;
            if ( dist2 > brushRad2 ) continue;
            qreal dist = qSqrt(dist2);
            qreal t = dist / m_radius;
            qreal alpha = qPow(1.0 - t, m_hardness);
            QRgb bg = m_image->pixel(x,y);
            int r_new = qBound(0, int(qRed(bg)   + alpha*(r_col - qRed(bg))),   255);
            int g_new = qBound(0, int(qGreen(bg) + alpha*(g_col - qGreen(bg))), 255);
            int b_new = qBound(0, int(qBlue(bg)  + alpha*(b_col - qBlue(bg))),  255);
            m_image->setPixel(x,y,qRgb(r_new,g_new,b_new));
        }
    }
    m_layer->updatePixmap();
}