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
    static bool force;
    static bool forcedAlphaMasking;
    static bool skipValidation;
    static bool isWhiteBackgroundImage;
    
 };
 
 // ---------------- Constructor ----------------
 class EditorStyle {
 
   public:
   
    enum InterpolationMode { Nearest, Linear, Bicubic, System };

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
      m_hasPerspective = settings.value("Main/perspective", true).toBool();
      m_binaryMasking = settings.value("Main/binaryMasking", true).toBool();
      m_crosshair = settings.value("Main/crosshair", true).toBool();
      
      // cursor stuff
      m_cursorSize = settings.value("Main/cursorSize",0).toInt();
      QString cursorFillColor = settings.value("Main/cursorFillColor", "black").toString();
      if ( QColor::isValidColorName(cursorFillColor) ) {
        m_cursorFillColor = QColor::fromString(cursorFillColor);
      }
      QString cursorBorderColor = settings.value("Main/cursorBorderColor", "white").toString();
      if ( QColor::isValidColorName(cursorBorderColor) ) {
        m_cursorBorderColor = QColor::fromString(cursorBorderColor);
      }
      
      // Claude quads
      m_useClaudeQuads = settings.value("Cage/claudeQuads", true).toBool();
      // Cage quads
      m_useCageQuads = settings.value("Cage/quads", true).toBool();
      // Cage control point radius
      m_controlPointRadius = settings.value("Cage/controlPointRadius", 4).toInt();
      // Cage control point color
      QString controlPointColor = settings.value("Cage/controlPointColor", "red").toString();
      if ( QColor::isValidColorName(controlPointColor) ) {
        m_controlPointColor = QColor::fromString(controlPointColor);
      }
      // Cage grid color
      QString cageGridColor = settings.value("Cage/gridColor", "green").toString();
      if ( QColor::isValidColorName(cageGridColor) ) {
        m_gridColor = QColor::fromString(cageGridColor);
      }
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
      // ImageLayer interpolationMode
      QString interpolationMode = settings.value("ImageLayer/interpolationMode", "linear").toString().trimmed().toLower();
      if ( interpolationMode == "nearest" ) {
        m_interpolationMode = InterpolationMode::Nearest;
      } else if ( interpolationMode == "bicubic" ) {
        m_interpolationMode = InterpolationMode::Bicubic;
      } else {
        m_interpolationMode = InterpolationMode::Linear;
      }
      
    }
    
    QColor getHandleColor() const { return m_handleColor; }
    int getHandleSize() const { return m_handleSize; }
    QColor lassoColor() const { return m_lassoColor; }
    QColor cageGridColor() const { return m_gridColor; }
    QColor cageWarpColor() const { return m_cageWarpColor; }
    QColor controlPointColor() const { return m_controlPointColor; }
    QString windowSize() const { return m_windowSize; }
    QString version() const { return m_version; }
    int cursorSize() const { return m_cursorSize; }
    QColor cursorFillColor() const { return m_cursorFillColor; }
    QColor cursorBorderColor() const { return m_cursorBorderColor; }
    int lassoWidth() const { return m_lassoWidth; }
    int controlPointRadius() const { return m_controlPointRadius; }
    bool crosshair() const { return m_crosshair; }
    bool isLoggingEnabled() const { return m_loggingIsEnabled; }
    bool useCageQuads() const { return m_useCageQuads; }
    bool useClaudeQuads() const { return m_useClaudeQuads; }
    bool hasPerspective() const { return m_hasPerspective; }
    bool binaryMasking() const { return m_binaryMasking; }
    double rotationSingleStep() const { return m_rotationSingleStep; }
    Qt::TransformationMode transformationMode() const { return m_transformationMode; }
    InterpolationMode interpolationMode() const { return m_interpolationMode; }

   private:
   
    EditorStyle()
        : m_lassoColor(Qt::red), 
          m_handleColor(Qt::cyan),
          m_handleSize(10),
          m_lassoWidth(3), 
          m_cursorSize(0),
          m_controlPointRadius(4),
          m_gridColor(Qt::green),
          m_controlPointColor(Qt::red),
          m_cursorFillColor(Qt::black),
          m_cursorBorderColor(Qt::white),
          m_rotationSingleStep(0.5),
          m_loggingIsEnabled(false), 
          m_useCageQuads(true), 
          m_useClaudeQuads(true), 
          m_hasPerspective(true),
          m_binaryMasking(true),
          m_crosshair(true),
          m_windowSize("default"),
          m_version("public"),
          m_cageWarpColor(Qt::green), 
          m_transformationMode(Qt::FastTransformation),
          m_interpolationMode(InterpolationMode::Linear) 
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
    QColor m_gridColor;
    QColor m_controlPointColor;
    QColor m_cursorFillColor;
    QColor m_cursorBorderColor;
    QString m_windowSize;
    QString m_version;
    
    Qt::TransformationMode m_transformationMode;
    InterpolationMode m_interpolationMode;
    
    bool m_crosshair;
    bool m_loggingIsEnabled;
    bool m_useCageQuads;
    bool m_useClaudeQuads;
    bool m_hasPerspective;
    bool m_binaryMasking;
    
    int m_lassoWidth;
    int m_controlPointRadius;
    int m_handleSize;
    int m_cursorSize;
    
    double m_rotationSingleStep;
    
 };

#endif