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
      // qDebug() << "Verfügbare Schlüssel:" << settings.allKeys();
      // qDebug output
      m_windowSize = settings.value("Main/windowSize","default").toString();
      m_loggingIsEnabled = settings.value("Main/enableLogging", false).toBool();
      if ( m_loggingIsEnabled ) {
        QLoggingCategory::setFilterRules("editor.graphics.debug=true");
      } else {
        QLoggingCategory::setFilterRules("editor.graphics.debug=false");
      }
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
    }
    QColor lassoColor() const { return m_lassoColor; }
    QString windowSize() const { return m_windowSize; }
    int lassoWidth() const { return m_lassoWidth; }
    bool isLoggingEnabled() const { return m_loggingIsEnabled; }

   private:
   
    EditorStyle() : m_lassoColor(Qt::yellow), m_lassoWidth(0), m_loggingIsEnabled(true), m_windowSize("default") {}
    
    QColor m_lassoColor;
    QString m_windowSize;
    int m_lassoWidth;
    bool m_loggingIsEnabled;
    
 };

#endif