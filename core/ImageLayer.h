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

// -------------------------- Image Layer --------------------------
class ImageLayer {

public:

	ImageLayer( const QImage& image ) : m_image(image) {
	}
	
	int id() const { return m_index; }
	void setIndex( const int index ) { m_index = index; }
	
protected:

	QImage m_image;
	QImage m_originalImage;
	
	
private:

	int m_index = 0;

};