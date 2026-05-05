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

#ifndef CONFIG_H
 #define CONFIG_H
 
 #include <QColor>
 #include <QString>
 #include <QSettings>
 #include <QLoggingCategory>
 
 #include <iostream>
 
 // >>>
 struct LogColor {
    static constexpr const char* Red    = "\033[91m";
    static constexpr const char* Green  = "\033[92m";
    static constexpr const char* Yellow = "\033[93m";
    static constexpr const char* Blue   = "\033[94m";
    static constexpr const char* Reset  = "\033[0m";
 };
 
 // >>>
 Q_DECLARE_LOGGING_CATEGORY(logEditor)

 // ---------------- Constructor ----------------
 class Config {
 
  public:
  
    static bool verbose;
    static bool isWhiteBackgroundImage;
    
 };
 
 // ---------------- Constructor ----------------
 class EditorStyle {
 
   public:

    static EditorStyle& instance() {
      static EditorStyle inst;
      return inst;
    }
    
    void load( const QString &path ) {
      QSettings settings(path, QSettings::IniFormat);
      m_version = settings.value("Main/version","public").toString();
      m_windowSize = settings.value("Main/windowSize","default").toString();
      m_loggingIsEnabled = settings.value("Main/enableLogging", false).toBool();
      if ( m_loggingIsEnabled ) {
        QLoggingCategory::setFilterRules("editor.graphics.debug=true");
      } else {
        QLoggingCategory::setFilterRules("editor.graphics.debug=false");
      }
      m_hasPerspective = settings.value("Main/perspective", false).toBool();
      m_binaryMasking = settings.value("Main/binaryMasking", false).toBool();
      
      // Cage quads
      m_useCageQuads = settings.value("Cage/quads", true).toBool();
      // Cage control point radius
      m_controlPointRadius = settings.value("Cage/controlPointRadius", 4).toInt();
      // Cage color
      QString cageWarpColor = settings.value("Cage/color", "green").toString();
      if ( QColor::isValidColorName(cageWarpColor) ) {
        m_cageWarpColor = QColor::fromString(cageWarpColor);
      }
      
      // Scale 
      QString handleColor = settings.value("Scale/handleColor", "cyan").toString();
      if ( QColor::isValidColorName(handleColor) ) {
        m_handleColor = QColor::fromString(handleColor);
      }
      m_handleSize = settings.value("Scale/handleSize", 10).toInt();
      
      // Lasso color
      QString rawColor = settings.value("Lasso/color", "yellow").toString();
      if ( QColor::isValidColorName(rawColor) ) {
        m_lassoColor = QColor::fromString(rawColor);
      }
      // Lasso width
      int rawWidth = settings.value("Lasso/width", 2).toInt();
      if ( rawWidth >= 0 && rawWidth < 20 ) {
        m_lassoWidth = rawWidth;
      }
      
      // ImageLayer rotation angle
      m_rotationSingleStep = settings.value("ImageLayer/rotationSingleStep", 1.0).toDouble();
      // ImageLayer transformationMode
      QString transformMode = settings.value("ImageLayer/transformationMode", "fast").toString().trimmed().toLower();
      if ( transformMode == "fast" || transformMode == "fasttransformation" ) {
        m_transformationMode = Qt::FastTransformation;
      } else if ( transformMode == "smooth" || transformMode == "smoothtransformation" ) {
        m_transformationMode = Qt::SmoothTransformation;
      }
      
    }
    
    QColor getHandleColor() const { return m_handleColor; }
    int getHandleSize() const { return m_handleSize; }
    QColor lassoColor() const { return m_lassoColor; }
    QColor cageWarpColor() const { return m_cageWarpColor; }
    QString windowSize() const { return m_windowSize; }
    QString version() const { return m_version; }
    int lassoWidth() const { return m_lassoWidth; }
    int controlPointRadius() const { return m_controlPointRadius; }
    bool isLoggingEnabled() const { return m_loggingIsEnabled; }
    bool useCageQuads() const { return m_useCageQuads; }
    bool hasPerspective() const { return m_hasPerspective; }
    bool binaryMasking() const { return m_binaryMasking; }
    double rotationSingleStep() const { return m_rotationSingleStep; }
    Qt::TransformationMode transformationMode() const { return m_transformationMode; }

   private:
   
    EditorStyle()
        : m_lassoColor(Qt::red), 
          m_handleColor(Qt::cyan),
          m_handleSize(10),
          m_lassoWidth(3), 
          m_controlPointRadius(4),
          m_rotationSingleStep(0.5),
          m_loggingIsEnabled(false), 
          m_useCageQuads(true), 
          m_hasPerspective(false),
          m_binaryMasking(false),
          m_windowSize("default"),
          m_version("public"),
          m_cageWarpColor(Qt::green), 
          m_transformationMode(Qt::FastTransformation) 
    { 
      if ( m_loggingIsEnabled ) {
        QLoggingCategory::setFilterRules("editor.graphics.debug=true");
      } else {
        QLoggingCategory::setFilterRules("editor.graphics.debug=false");
      }
    }
   
    QColor m_handleColor; 
    QColor m_lassoColor;
    QColor m_cageWarpColor;
    QString m_windowSize;
    QString m_version;
    
    Qt::TransformationMode m_transformationMode;
    
    int m_lassoWidth;
    int m_controlPointRadius;
    int m_handleSize;
    bool m_loggingIsEnabled;
    bool m_useCageQuads;
    bool m_hasPerspective;
    bool m_binaryMasking;
    double m_rotationSingleStep;
    
 };

#endif