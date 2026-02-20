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

#include "ImageLoader.h"
#include "../util/QImageUtils.h"

#include <QDebug>
#include <QApplication>

#ifdef HASITK
  #include "itkImage.h"
  #include "itkImageFileReader.h"
  #include "itkImageFileWriter.h"
  #include "itkRescaleIntensityImageFilter.h"
#endif

#include <iostream>

// -------------------------- ImageLoader --------------------------
QImage ImageLoader::loadMincImage( const QString& filePath ) 
{
   #ifdef HASITK
     // Wir nutzen float für den Import, um maximale Präzision der MINC-Daten zu behalten
     typedef itk::Image<float, 2> ImageType; 
     typedef itk::ImageFileReader<ImageType> ReaderType;
     auto reader = ReaderType::New();
     reader->SetFileName(filePath.toStdString());
     // Filter, um die oft hohen medizinischen Werte (z.B. 12-bit) auf 8-bit (Qt) zu skalieren
     typedef itk::Image<unsigned char, 2> VisualImageType;
     typedef itk::RescaleIntensityImageFilter<ImageType, VisualImageType> RescaleFilterType;
     auto rescaleFilter = RescaleFilterType::New();
     rescaleFilter->SetInput(reader->GetOutput());
     rescaleFilter->SetOutputMinimum(0);
     rescaleFilter->SetOutputMaximum(255);
     try {
        rescaleFilter->Update();
     } catch ( const itk::ExceptionObject& e ) {
        qDebug() << "ITK Error:" << e.GetDescription();
        return QImage();
     }
     VisualImageType::Pointer itkImage = rescaleFilter->GetOutput();
     auto region = itkImage->GetLargestPossibleRegion();
     int width = region.GetSize()[0];
     int height = region.GetSize()[1];
     // QImage erstellt eine Ansicht auf den ITK-Buffer
     QImage qImg(itkImage->GetBufferPointer(), width, height, QImage::Format_Grayscale8);
     // .copy() ist wichtig, da der ITK-Buffer am Ende dieser Funktion gelöscht wird!
     return qImg.copy();
   #else
     return QImage();
   #endif
}

QPixmap ImageLoader::loadMincAsPixmap( const QString& filePath ) 
{
  QImage image = loadMincImage(filePath);
  if ( !image.isNull() ) {
   return QPixmap::fromImage(image);
  }
  return QPixmap();
}


bool ImageLoader::load( const QString& filePath, bool asImage )
{
 // qDebug() << "ImageLoader::load(): filePath='" << filePath;
 {
  if ( filePath.isEmpty() ) 
   return false;
  if ( filePath.endsWith(".mnc", Qt::CaseInsensitive) || filePath.endsWith(".mnc2", Qt::CaseInsensitive) ) {
   if ( asImage ) {
    m_image = loadMincImage(filePath);
    m_hasImage = true;
   } else {
    m_pixmap = loadMincAsPixmap(filePath);
   }
  } else {
   if ( asImage ) {
    m_image.load(filePath);
    if ( m_image.format() == QImage::Format_Indexed8 || m_image.format() == QImage::Format_Mono ) {
     m_image = m_image.convertToFormat(QImage::Format_ARGB32);
    }
    m_hasImage = true;
   } else {
    m_pixmap.load(filePath);
   }
  }
  if ( ( !asImage && m_pixmap.isNull() ) || ( asImage && m_image.isNull() ) ) {
   return false;
  }
  return true;
 }
}

bool ImageLoader::saveAs( const QImage& image, const QString& filePath )
{
 qDebug() << "ImageLoader::saveAs(): filePath='" << filePath << "': " << image.format();
 {
  if ( !image.isNull() ) {
    return image.save(filePath);
  }
  return false;
 }
}

bool ImageLoader::hasWhiteBackground()
{
  if ( m_hasImage ) {
    return !QImageUtils::hasBlackBackground(m_image);
  } else {
    if ( qobject_cast<QApplication*>(qApp) ) {
      QImage img = m_pixmap.toImage();
      return !QImageUtils::hasBlackBackground(img);
    }
  }
  return false;
}