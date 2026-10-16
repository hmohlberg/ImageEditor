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
 #include <QFile>
 #include <QMap>
 #include <QTextStream>

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
    static bool gpuCageWarpProcessing;
    static bool concatenate;
    
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
      m_path = path;
      QSettings settings(path, QSettings::IniFormat);
      m_version = settings.value("Main/version","public").toString();
      m_windowSize = settings.value("Main/windowSize","default").toString();
      m_loggingIsEnabled = settings.value("Main/enableLogging", false).toBool();
      if ( m_loggingIsEnabled ) {
        QLoggingCategory::setFilterRules("editor.graphics.debug=true\nqt.network.http2=false");
      } else {
        QLoggingCategory::setFilterRules("editor.graphics.debug=false\nqt.network.http2=false");
      }
      m_hasPerspective = settings.value("Main/perspective", true).toBool();
      m_binaryMasking = settings.value("Main/binaryMasking", true).toBool();
      m_crosshair = settings.value("Main/crosshair", true).toBool();
      m_showDocksAtStartup = settings.value("Main/showDocksAtStartup", false).toBool();
      m_showOverviewMap    = settings.value("Main/showOverviewMap",    false).toBool();
      m_pixelScale = settings.value("Main/pixelScale", 20.0).toDouble();
      
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
      m_usegpu = settings.value("Cage/gpu", false).toBool();
      m_gpuCatmullRom = settings.value("Cage/gpuCatmullRom", true).toBool();
      // Live warp during drag (GPU only, experimental)
      m_liveWarp = settings.value("Cage/liveWarp", false).toBool();
      // Prevent self-intersection of cage quad edges
      m_noSelfIntersection = settings.value("Cage/noSelfIntersection", true).toBool();
      m_cageGridCols   = settings.value("Cage/gridCols",    3).toInt();
      m_squareCageQuads = settings.value("Cage/squareQuads", false).toBool();
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
      if ( rawWidth >= 0 ) {
        m_lassoWidth = rawWidth;
      }
      
      // Polygon width (control size double of this value)
      int polygonWidth = settings.value("Polygon/width", 2).toInt();
      if ( polygonWidth >= 0 ) {
        m_polygonWidth = polygonWidth;
      }
      int polygonHandleSize = settings.value("Polygon/handleSize", 10).toInt();
      if ( polygonHandleSize >= 0 ) {
        m_polygonHandleSize = polygonHandleSize;
      }
      { QColor c(settings.value("Polygon/handleColor", "#ff0000").toString());
        if ( c.isValid() ) m_polygonHandleColor = c; }
      
      // github:// shorthand base URL
      m_githubBaseUrl = settings.value("Network/githubBaseUrl",
          "https://raw.githubusercontent.com/hmohlberg/ImageEditor/main/samples").toString();
      // Models
      m_lamaModelPath = settings.value("Models/lamaModelPath", QString()).toString();
      // ImageHeader display adjustments
      m_brightness = settings.value("ImageHeader/brightness", 0).toInt();
      m_contrast   = settings.value("ImageHeader/contrast",   0).toInt();
      // ImageLayer allowIntegerMoveOnly
      m_allowIntegerMoveOnly = settings.value("ImageLayer/integerMoveOnly", true).toBool();
      // ImageLayer overlay opacity
      m_layerOverlayOpacity = settings.value("ImageLayer/overlayOpacity", 0.0).toDouble();
      // ImageLayer rotation angle
      m_rotationSingleStep = settings.value("ImageLayer/rotationSingleStep", 1.0).toDouble();
      // ImageLayer handle radius
      m_handleRadius = settings.value("ImageLayer/handleRadius", 4.0).toDouble();
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

      loadComments(path);
    }
    
    QColor getHandleColor() const { return m_handleColor; }
    int brightness() const { return m_brightness; }
    int contrast()   const { return m_contrast; }
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
    int polygonWidth() const { return m_polygonWidth; }
    int polygonHandleSize() const { return m_polygonHandleSize; }
    QColor polygonHandleColor() const { return m_polygonHandleColor; }
    int controlPointRadius() const { return m_controlPointRadius; }
    double handleRadius() const { return m_handleRadius; }
    double pixelScale() const { return m_pixelScale; }
    bool crosshair() const { return m_crosshair; }
    bool showDocksAtStartup() const { return m_showDocksAtStartup; }
    bool showOverviewMap()    const { return m_showOverviewMap; }
    bool isLoggingEnabled() const { return m_loggingIsEnabled; }
    bool useCageQuads() const { return m_useCageQuads; }
    bool useGPU() const { return m_usegpu; }
    bool gpuCatmullRom() const { return m_gpuCatmullRom; }
    bool liveWarp() const { return m_liveWarp; }
    bool noSelfIntersection() const { return m_noSelfIntersection; }
    int  cageGridCols() const { return m_cageGridCols; }
    bool squareCageQuads() const { return m_squareCageQuads; }
    bool useClaudeQuads() const { return m_useClaudeQuads; }
    bool hasPerspective() const { return m_hasPerspective; }
    bool binaryMasking() const { return m_binaryMasking; }
    bool allowIntegerMoveOnly() const { return m_allowIntegerMoveOnly; }
    double rotationSingleStep() const { return m_rotationSingleStep; }
    double layerOverlayOpacity() const { return m_layerOverlayOpacity; }
    Qt::TransformationMode transformationMode() const { return m_transformationMode; }
    InterpolationMode interpolationMode() const { return m_interpolationMode; }
    QString path() const { return m_path; }
    QString githubBaseUrl() const { return m_githubBaseUrl; }
    QString lamaModelPath() const { return m_lamaModelPath; }

    // Setters (used by ConfigDialog to apply changes at runtime)
    void setBrightness(int v) { m_brightness = qBound(-100, v, 100); }
    void setContrast(int v)   { m_contrast   = qBound(-100, v, 100); }
    void setLoggingEnabled(bool v) { m_loggingIsEnabled = v; QLoggingCategory::setFilterRules(v ? "editor.graphics.debug=true\nqt.network.http2=false" : "editor.graphics.debug=false\nqt.network.http2=false"); }
    void setWindowSize(const QString& v) { m_windowSize = v; }
    void setHasPerspective(bool v) { m_hasPerspective = v; }
    void setBinaryMasking(bool v) { m_binaryMasking = v; }
    void setCrosshair(bool v) { m_crosshair = v; }
    void setShowDocksAtStartup(bool v) { m_showDocksAtStartup = v; }
    void setShowOverviewMap(bool v)    { m_showOverviewMap = v; }
    void setCursorSize(int v) { m_cursorSize = v; }
    void setCursorFillColor(const QColor& v) { m_cursorFillColor = v; }
    void setCursorBorderColor(const QColor& v) { m_cursorBorderColor = v; }
    void setUseClaudeQuads(bool v) { m_useClaudeQuads = v; }
    void setUseCageQuads(bool v) { m_useCageQuads = v; }
    void setUseGPU(bool v) { m_usegpu = v; }
    void setGpuCatmullRom(bool v) { m_gpuCatmullRom = v; }
    void setLiveWarp(bool v) { m_liveWarp = v; }
    void setNoSelfIntersection(bool v) { m_noSelfIntersection = v; }
    void setCageGridCols(int v) { m_cageGridCols = v; }
    void setSquareCageQuads(bool v) { m_squareCageQuads = v; }
    void setControlPointRadius(int v) { m_controlPointRadius = v; }
    void setControlPointColor(const QColor& v) { m_controlPointColor = v; }
    void setCageGridColor(const QColor& v) { m_gridColor = v; }
    void setCageWarpColor(const QColor& v) { m_cageWarpColor = v; }
    void setHandleColor(const QColor& v) { m_handleColor = v; }
    void setHandleSize(int v) { m_handleSize = v; }
    void setLassoColor(const QColor& v) { m_lassoColor = v; }
    void setLassoWidth(int v) { m_lassoWidth = v; }
    void setPolygonWidth(int v) { m_polygonWidth = v; }
    void setPolygonHandleSize(int v) { m_polygonHandleSize = v; }
    void setPolygonHandleColor(const QColor& v) { m_polygonHandleColor = v; }
    void setAllowIntegerMoveOnly(bool v) { m_allowIntegerMoveOnly = v; }
    void setLayerOverlayOpacity(double v) { m_layerOverlayOpacity = v; }
    void setRotationSingleStep(double v) { m_rotationSingleStep = v; }
    void setHandleRadius(double v) { m_handleRadius = v; }
    void setPixelScale(double v) { m_pixelScale = v; }
    void setTransformationMode(Qt::TransformationMode v) { m_transformationMode = v; }
    void setInterpolationMode(InterpolationMode v) { m_interpolationMode = v; }
    void setPath(const QString& v) { m_path = v; }
    void setGithubBaseUrl(const QString& v) { m_githubBaseUrl = v; }
    void setLamaModelPath(const QString& v) { m_lamaModelPath = v; }

    void resetToDefaults() {
      m_lassoColor        = Qt::red;
      m_handleColor       = Qt::cyan;
      m_handleSize        = 10;
      m_lassoWidth        = 3;
      m_polygonWidth      = 10;
      m_polygonHandleSize = 10;
      m_polygonHandleColor= Qt::red;
      m_handleRadius      = 4.0;
      m_cursorSize        = 0;
      m_controlPointRadius= 4;
      m_gridColor         = Qt::green;
      m_controlPointColor = Qt::red;
      m_cursorFillColor   = Qt::black;
      m_cursorBorderColor = Qt::white;
      m_cageWarpColor     = Qt::green;
      m_rotationSingleStep= 0.5;
      m_pixelScale        = 20.0;
      m_layerOverlayOpacity = 0.8;
      m_loggingIsEnabled  = false;
      m_useCageQuads      = true;
      m_usegpu            = false;
      m_gpuCatmullRom     = false;
      m_liveWarp          = false;
      m_noSelfIntersection= true;
      m_cageGridCols    = 3;
      m_squareCageQuads = false;
      m_allowIntegerMoveOnly = true;
      m_useClaudeQuads    = true;
      m_hasPerspective    = true;
      m_binaryMasking     = true;
      m_crosshair         = true;
      m_showDocksAtStartup = false;
      m_showOverviewMap    = false;
      m_windowSize        = "default";
      m_version           = "public";
      m_githubBaseUrl     = "https://raw.githubusercontent.com/hmohlberg/ImageEditor/main/samples";
      m_brightness        = 0;
      m_contrast          = 0;
      m_transformationMode= Qt::FastTransformation;
      m_interpolationMode = InterpolationMode::Linear;
      QLoggingCategory::setFilterRules("editor.graphics.debug=false\nqt.network.http2=false");
    }

    void save() {
      if ( m_path.isEmpty() ) return;
      QFile f(m_path);
      if ( !f.open(QIODevice::WriteOnly | QIODevice::Text) ) return;
      QTextStream out(&f);
      out.setEncoding(QStringConverter::Utf8);

      auto b  = [](bool v)   { return v ? QStringLiteral("true") : QStringLiteral("false"); };
      auto n  = [](int v)    { return QString::number(v); };
      auto d  = [](double v) { return QString::number(v, 'g', 10); };

      // write a section header; track current section for comment lookup
      QString currentSec;
      auto sec = [&](const char* name) {
        currentSec = QString::fromLatin1(name);
        out << "\n[" << name << "]\n";
      };
      // write key=value; if a file was loaded use stored comments, else use tooltip default
      auto val = [&](const char* key, const QString& value, const char* defaultComment) {
        QString comment;
        if ( !m_commentsLoaded ) {
          comment = QString::fromUtf8(defaultComment);
        } else {
          comment = m_savedComments.value(currentSec + QLatin1Char('/') + QLatin1String(key));
        }
        if ( !comment.isEmpty() ) {
          for ( const QString& line : comment.split(QLatin1Char('\n')) )
            out << "; " << line << "\n";
        }
        out << key << " = " << value << "\n";
      };

      out << "; ImageEditor configuration file — generated automatically, safe to edit manually.\n";
      out << "; Comment lines start with ';'. Boolean values: true / false.\n";

      sec("Main");
      val("version",           m_version,
          "Configuration profile name (e.g. 'public'). Informational only.");
      val("windowSize",        m_windowSize,
          "Initial window size at startup: default, maximum, fullscreen, or predefined mni layout.");
      val("enableLogging",     b(m_loggingIsEnabled),
          "Enables debug logging output to the console.");
      val("perspective",       b(m_hasPerspective),
          "Enables perspective transformation mode for image layers.");
      val("binaryMasking",     b(m_binaryMasking),
          "Restricts mask values to 0 or 1 (binary on/off) instead of continuous values.");
      val("crosshair",         b(m_crosshair),
          "Shows a crosshair overlay at the cursor position in the image view.");
      val("showDocksAtStartup", b(m_showDocksAtStartup),
          "Show all dock panels automatically when the application starts.");
      val("showOverviewMap",   b(m_showOverviewMap),
          "Show a minimap thumbnail above the layer list in the dock, with a red rectangle indicating the currently visible area.");
      val("pixelScale",        d(m_pixelScale),
          "Scale factor applied when exporting pixel-accurate overlays (e.g. mask images).");
      val("cursorSize",        n(m_cursorSize),
          "Radius of the brush preview circle displayed at the cursor position, in pixels.");
      val("cursorFillColor",   m_cursorFillColor.name(),
          "Fill color of the brush preview cursor circle.");
      val("cursorBorderColor", m_cursorBorderColor.name(),
          "Border color of the brush preview cursor circle.");

      sec("Cage");
      val("claudeQuads",        b(m_useClaudeQuads),
          "Uses Claude's quad subdivision algorithm for cage warping.");
      val("quads",              b(m_useCageQuads),
          "Activates quad-based cage control instead of triangulation.");
      val("gpu",                b(m_usegpu),
          "Enables GPU-accelerated cage warp rendering via OpenGL.");
      val("gpuCatmullRom",      b(m_gpuCatmullRom),
          "Uses Catmull-Rom spline interpolation on the GPU for smoother warp results.");
      val("liveWarp",           b(m_liveWarp),
          "Updates the warp result interactively while dragging control points (requires GPU).");
      val("noSelfIntersection", b(m_noSelfIntersection),
          "Prevents cage control points from crossing each other during warping.");
      val("gridCols",           n(m_cageGridCols),
          "Number of columns in the cage control grid.");
      val("squareQuads",        b(m_squareCageQuads),
          "Automatically sets the number of rows so each cage quad is approximately square.");
      val("controlPointRadius", n(m_controlPointRadius),
          "Display radius of cage control point handles in pixels.");
      val("controlPointColor",  m_controlPointColor.name(),
          "Color used to draw cage control point handles.");
      val("gridColor",          m_gridColor.name(),
          "Color of the cage grid lines.");
      val("color",              m_cageWarpColor.name(),
          "Color of the cage warp boundary outline.");

      sec("Scale");
      val("handleColor", m_handleColor.name(),
          "Color of the scale and rotate transform handles.");
      val("handleSize",  n(m_handleSize),
          "Size of the scale and rotate transform handles in pixels.");

      sec("Lasso");
      val("color", m_lassoColor.name(),
          "Color of the freehand lasso selection outline.");
      val("width", n(m_lassoWidth),
          "Line width of the lasso selection outline in pixels.");

      sec("Polygon");
      val("width",       n(m_polygonWidth),
          "Line width of the polygon outline in pixels.");
      val("handleSize",  n(m_polygonHandleSize),
          "Size of polygon vertex handles in pixels.");
      val("handleColor", m_polygonHandleColor.name(),
          "Color of the polygon vertex handles.");

      sec("ImageLayer");
      val("integerMoveOnly",    b(m_allowIntegerMoveOnly),
          "Restricts layer movement to whole pixel positions only.");
      val("overlayOpacity",     d(m_layerOverlayOpacity),
          "Opacity of the semi-transparent red overlay shown when Ctrl+dragging a layer (0.0–1.0).");
      val("rotationSingleStep", d(m_rotationSingleStep),
          "Angle increment per step when rotating a layer with the rotation handle, in degrees.");
      val("handleRadius",       d(m_handleRadius),
          "Radius of the rotation and scale transform handles in pixels.");
      val("transformationMode",
          (m_transformationMode == Qt::FastTransformation) ? QStringLiteral("fast") : QStringLiteral("smooth"),
          "Rendering quality when transforming a layer: fast (nearest-neighbour) or smooth (bilinear).");
      {
        QString im;
        switch (m_interpolationMode) {
          case InterpolationMode::Nearest: im = "nearest"; break;
          case InterpolationMode::Bicubic: im = "bicubic"; break;
          default:                         im = "linear";  break;
        }
        val("interpolationMode", im,
            "Pixel interpolation used when scaling or rotating layers: nearest, linear, or bicubic.");
      }

      sec("ImageHeader");
      val("brightness", n(m_brightness),
          "Global brightness adjustment applied to the displayed image (-100 to 100).");
      val("contrast",   n(m_contrast),
          "Global contrast adjustment applied to the displayed image (-100 to 100).");

      sec("Network");
      val("githubBaseUrl", m_githubBaseUrl,
          "Base URL used to resolve github:// protocol paths when loading remote resources.");

      sec("Models");
      val("lamaModelPath", m_lamaModelPath,
          "Path to the LaMa ONNX model file (lama.onnx). Leave empty to use the default location.");

      f.close();
    }

   private:

    void loadComments( const QString& path ) {
      m_savedComments.clear();
      m_commentsLoaded = false;
      QFile f(path);
      if ( !f.open(QIODevice::ReadOnly | QIODevice::Text) ) return;
      QTextStream in(&f);
      QString section;
      QStringList pending;
      while ( !in.atEnd() ) {
        QString line = in.readLine().trimmed();
        if ( line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#')) ) {
          QString text = line.mid(1);
          if ( text.startsWith(QLatin1Char(' ')) ) text = text.mid(1);
          pending << text;
        } else if ( line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']')) ) {
          section = line.mid(1, line.length() - 2).trimmed();
          pending.clear();
        } else if ( line.contains(QLatin1Char('=')) ) {
          int eq = line.indexOf(QLatin1Char('='));
          QString key = line.left(eq).trimmed();
          if ( !pending.isEmpty() )
            m_savedComments[section + QLatin1Char('/') + key] = pending.join(QLatin1Char('\n'));
          pending.clear();
        } else {
          pending.clear();
        }
      }
      m_commentsLoaded = true;
    }

    EditorStyle()
        : m_lassoColor(Qt::red), 
          m_handleColor(Qt::cyan),
          m_handleSize(10),
          m_lassoWidth(3), 
          m_polygonWidth(10),
          m_polygonHandleSize(10),
          m_polygonHandleColor(Qt::red),
          m_handleRadius(4.0),
          m_cursorSize(0),
          m_controlPointRadius(4),
          m_gridColor(Qt::green),
          m_controlPointColor(Qt::red),
          m_cursorFillColor(Qt::black),
          m_cursorBorderColor(Qt::white),
          m_rotationSingleStep(0.5),
          m_layerOverlayOpacity(0.8),
          m_loggingIsEnabled(false), 
          m_useCageQuads(true),
          m_usegpu(false),
          m_gpuCatmullRom(false),
          m_liveWarp(false),
          m_noSelfIntersection(true),
          m_cageGridCols(3),
          m_squareCageQuads(false),
          m_allowIntegerMoveOnly(true),
          m_useClaudeQuads(true), 
          m_hasPerspective(true),
          m_binaryMasking(true),
          m_crosshair(true),
          m_showDocksAtStartup(false),
          m_showOverviewMap(false),
          m_windowSize("default"),
          m_version("public"),
          m_cageWarpColor(Qt::green),
          m_transformationMode(Qt::FastTransformation),
          m_interpolationMode(InterpolationMode::Linear),
          m_path(QString()),
          m_githubBaseUrl("https://raw.githubusercontent.com/hmohlberg/ImageEditor/main/samples")
    { 
      if ( m_loggingIsEnabled ) {
        QLoggingCategory::setFilterRules("editor.graphics.debug=true\nqt.network.http2=false");
      } else {
        QLoggingCategory::setFilterRules("editor.graphics.debug=false\nqt.network.http2=false");
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
    QString m_path;
    QString m_githubBaseUrl;
    QString m_lamaModelPath;
    
    Qt::TransformationMode m_transformationMode;
    InterpolationMode m_interpolationMode;
    
    bool m_crosshair;
    bool m_showDocksAtStartup;
    bool m_showOverviewMap;
    bool m_loggingIsEnabled;
    bool m_useCageQuads;
    bool m_useClaudeQuads;
    bool m_hasPerspective;
    bool m_binaryMasking;
    bool m_allowIntegerMoveOnly;
    bool m_usegpu;
    bool m_gpuCatmullRom;
    bool m_liveWarp;
    bool m_noSelfIntersection;
    bool m_squareCageQuads;

    int m_lassoWidth;
    int m_polygonWidth;
    int m_polygonHandleSize;
    QColor m_polygonHandleColor;
    int m_controlPointRadius;
    int m_handleSize;
    int m_cageGridCols;
    int m_cursorSize;
    
    double m_handleRadius;
    double m_rotationSingleStep;
    double m_layerOverlayOpacity;
    double m_pixelScale = 20.0;
    int m_brightness = 0;
    int m_contrast   = 0;

    bool m_commentsLoaded = false;
    QMap<QString, QString> m_savedComments;

 };

#endif