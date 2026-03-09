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
      // qDebug() << "Available keys:" << settings.allKeys();
      m_windowSize = settings.value("Main/windowSize","default").toString();
      m_loggingIsEnabled = settings.value("Main/enableLogging", false).toBool();
      if ( m_loggingIsEnabled ) {
        QLoggingCategory::setFilterRules("editor.graphics.debug=true");
      } else {
        QLoggingCategory::setFilterRules("editor.graphics.debug=false");
      }
      // cage quads
      m_useCageQuads = settings.value("Cage/quads", false).toBool();
      // lasso color
      QString rawColor = settings.value("Lasso/color", "yellow").toString();
      if ( QColor::isValidColorName(rawColor) ) {
        m_lassoColor = QColor::fromString(rawColor);
      }
      // lasso width
      int rawWidth = settings.value("Lasso/width", 2).toInt();
      if ( rawWidth >= 0 && rawWidth < 20 ) {
        m_lassoWidth = rawWidth;
      }
      // rotation angle
      m_rotationSingleStep = settings.value("ImageLayer/rotationSingleStep", 1.0).toDouble();
    }
    QColor lassoColor() const { return m_lassoColor; }
    QString windowSize() const { return m_windowSize; }
    int lassoWidth() const { return m_lassoWidth; }
    bool isLoggingEnabled() const { return m_loggingIsEnabled; }
    bool useCageQuads() const { return m_useCageQuads; }
    double rotationSingleStep() const { return m_rotationSingleStep; }

   private:
   
    EditorStyle() : m_lassoColor(Qt::yellow), m_lassoWidth(0), m_rotationSingleStep(1.0),
                      m_loggingIsEnabled(true), m_useCageQuads(false), m_windowSize("default") {}
    
    QColor m_lassoColor;
    QString m_windowSize;
    int m_lassoWidth;
    bool m_loggingIsEnabled;
    bool m_useCageQuads;
    double m_rotationSingleStep;
    
 };

#endif