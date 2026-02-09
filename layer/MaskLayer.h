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
#include <QVector>
#include <QSize>

class MaskLayer : public QObject {

	Q_OBJECT

public:
    explicit MaskLayer( const QSize& size );
    explicit MaskLayer( int width, int height );
    
    void setPixel( int x, int y, uchar label );
    uchar pixel( int x, int y ) const;
    
    int width() { return m_image.width(); }
    int height() { return m_image.height(); }
    const QImage& image() const { return m_image; }
    void clear();
    
    quint8 labelAt( int x, int y ) const;
    void setLabelAt( int x, int y, quint8 label );
    void setImage( const QImage& image );

    const QVector<quint8>& data() const;
    
    void emitChanged() { emit changed(); }
    
signals:
	void changed();

private:
    QImage m_image;
    int m_width;
    int m_height;
    QVector<quint8> m_labels; // 0..9
    
};