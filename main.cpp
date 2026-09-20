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

#include <QApplication>
#include <QImageReader>
#include <QColorSpace>
#include <QLoggingCategory>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QDateTime>
#include <QString>
#include <QtGlobal>
#include <QPainter>
#include <QFile>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QSslConfiguration>

#include <iostream>
#include <unistd.h>

#include "core/Config.h"
#include "core/IMainSystem.h"
#include "core/BatchMain.h"
#include "core/ImageLoader.h"
#include "core/ImageProcessor.h"
#include "core/BigTiffIO.h"
#include "core/BigTiffProjectApply.h"

#include "gui/MainWindow.h"
#include "core/version.h"

#include <tiffvers.h>
#ifdef HASHDF5
#  include <hdf5.h>
#endif


// ---------------------- Init ----------------------
IMainSystem* IMainSystem::m_instance = nullptr;
bool Config::verbose = false;
bool Config::force = false;
bool Config::forcedAlphaMasking = false;
bool Config::skipValidation = false;
bool Config::isWhiteBackgroundImage = true;
bool Config::gpuCageWarpProcessing = false;

Q_LOGGING_CATEGORY(logEditor, "editor.graphics")

// ---------------------- Helper ----------------------
static void showHistory( int n ) {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history.json";
    QFile file(path);
    if ( !file.open(QIODevice::ReadOnly) ) {
        std::cout << "No history available." << std::endl;
        return;
    }
    QJsonArray history = QJsonDocument::fromJson(file.readAll()).array();
    if ( n > 0 ) {
      std::cout << "--- Last " << n << " calls ---" << std::endl;
    } else {
      std::cout << "--- Last calls ---" << std::endl;
    }
    int k = 1;
    for ( const QJsonValue &val : history ) {
        QJsonObject obj = val.toObject();
        std::cout << obj["date"].toString().toStdString() << " | " 
                  << "ImageEditor "
                  << obj["args"].toString().toStdString() << std::endl;
        k += 1;
        if ( n > 0 && k > n ) return;
    }
}

static void saveCurrentCall( int argc, char *argv[] ) {
  // std::cout << "saveCurrentCall(): Processing..." << std::endl;
  {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path); 
    QString filePath = path + "/history.json";
    QFile file(filePath);
    QJsonArray history;
    if ( file.open(QIODevice::ReadOnly) ) {
        history = QJsonDocument::fromJson(file.readAll()).array();
        file.close();
    }
    QStringList argList;
    for( int i = 1; i < argc; ++i ) argList << argv[i];
    QJsonObject currentCall;
    currentCall.insert("date", QDateTime::currentDateTime().toString(Qt::ISODate));
    currentCall.insert("args", argList.join(" "));
    history.prepend(currentCall);
    if ( history.size() > 100 ) history.removeLast();
    if ( file.open(QIODevice::WriteOnly) ) {
        file.write(QJsonDocument(history).toJson());
    }
  }
}

static void printError( const QString &msg ) {
    // \033[1;31m makes it BOLD and RED
    std::cerr << "\033[1;31m" << "ERROR: " << "\033[0m" 
              << msg.toStdString() << std::endl;
}

static bool validateFile( const QString &filePath, const QString &optionName, const QStringList &allowedExtensions = {} ) {
  if ( filePath.startsWith("http://") || filePath.startsWith("https://") )
      return true;
  auto errorPrefix = []() { return "\033[1;31mERROR: \033[0m"; };
  if ( !filePath.isEmpty() ) {
    QFileInfo fileInfo(filePath);
    if ( !fileInfo.exists() ) {
        std::cerr << errorPrefix() << "File '" << filePath.toStdString() << "' does not exist." << std::endl;
        return false;
    } if ( !fileInfo.isFile() ) {
        std::cerr << errorPrefix() << "'" << filePath.toStdString() << "' is a directory, not a file." << std::endl;
        return false;
    } if ( !fileInfo.isReadable() ) {
        std::cerr << errorPrefix() << "File '" << filePath.toStdString() << "' is not readable (check permissions)." << std::endl;
        return false;
    }
    if ( !allowedExtensions.isEmpty() ) {
     QString suffix = fileInfo.suffix().toLower();
     if ( !allowedExtensions.contains(suffix) ) {
      std::cerr << errorPrefix() << "Invalid format for --" << optionName.toStdString() 
                      << ". Allowed: " << allowedExtensions.join(", ").toStdString() << "." << std::endl;
      return false;
     }
    }
  }
  return true;
}

static QString downloadImageFromUrl( const QString& url )
{
    QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    QNetworkReply* reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::sslErrors,
                     reply, [reply](const QList<QSslError>& errors) {
        for ( const auto& e : errors )
            std::cerr << "SSL warning: " << e.errorString().toStdString() << std::endl;
        reply->ignoreSslErrors();
    });
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if ( reply->error() != QNetworkReply::NoError ) {
        std::cerr << "\033[1;31mERROR: \033[0m"
                  << "Download failed: " << reply->errorString().toStdString() << std::endl;
        reply->deleteLater();
        return {};
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    const QString tempPath = QDir::tempPath() + "/imageeditor_url_download.png";
    QFile f(tempPath);
    if ( !f.open(QIODevice::WriteOnly) ) {
        std::cerr << "\033[1;31mERROR: \033[0m"
                  << "Could not write temp file: " << tempPath.toStdString() << std::endl;
        return {};
    }
    f.write(data);
    f.close();
    return tempPath;
}

static QStringList parseFileList( const QString& path )
{
    QStringList entries;
    QFile f(path);
    if ( !f.open(QIODevice::ReadOnly | QIODevice::Text) ) return entries;
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    QString searchPath;
    for ( const auto& line : lines ) {
        const QString t = line.trimmed();
        if ( t.isEmpty() || t.startsWith('#') ) continue;
        if ( t.startsWith("http://") || t.startsWith("https://") ) {
            entries << t;
            continue;
        }
        QFileInfo fi(t);
        if ( fi.isAbsolute() ) {
            if ( fi.isDir() )
                searchPath = t;           // directory → becomes search path
            else
                entries << t;             // absolute file path
        } else {
            // relative filename → resolve against current search path
            entries << ( searchPath.isEmpty() ? t : searchPath + "/" + t );
        }
    }
    return entries;
}

static bool isPathWritable( const QString &path ) {
  QFileInfo checkInfo(path);
  if ( !checkInfo.exists() ) {
    return false;
  }
  if ( !checkInfo.isWritable() ) {
    return false;
  }
  return true;
}

static void setEnlargedStandardCursor( int targetSize, const QColor &fillColor = Qt::white, const QColor &borderColor = Qt::black ) {
  if ( targetSize != 0 ) {
    QIcon arrowIcon = QIcon::fromTheme("cursor-arrow");
    if ( arrowIcon.isNull() ) {
        arrowIcon = QIcon::fromTheme("arrow");
    }
    if ( !arrowIcon.isNull() ) {
        QPixmap pixmap = arrowIcon.pixmap(targetSize, targetSize);
        if ( !pixmap.isNull() ) {
            QApplication::setOverrideCursor(QCursor(pixmap.scaled(targetSize, targetSize, 
                                         Qt::KeepAspectRatio, 
                                         Qt::SmoothTransformation), 0, 0));
            return;
        }
    }
    QPixmap fallbackPixmap(targetSize, targetSize);
    fallbackPixmap.fill(Qt::transparent);
    QPainter painter(&fallbackPixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPolygon pfeil;
    pfeil << QPoint(0, 0)
          << QPoint(0, targetSize * 0.75)
          << QPoint(targetSize * 0.22, targetSize * 0.53)
          << QPoint(targetSize * 0.47, targetSize * 0.95)
          << QPoint(targetSize * 0.58, targetSize * 0.89)
          << QPoint(targetSize * 0.33, targetSize * 0.48)
          << QPoint(targetSize * 0.55, targetSize * 0.48);
    painter.setPen(QPen(borderColor, 2));
    painter.setBrush(fillColor);  
    painter.drawPolygon(pfeil);
    painter.end();
    QCursor enlargedCursor(fallbackPixmap, 0, 0);
    QApplication::setOverrideCursor(enlargedCursor);
  }
}

// ---------------------- Command Line Options ----------------------
static QJsonObject parser( const QCoreApplication *app, int argc ) {
  QJsonObject obj;
  QCommandLineParser parser;
  parser.setApplicationDescription("A simple ImageEditor with JSON-history support.");
  parser.addHelpOption();
  QCommandLineOption aboutOption("about",
      "Print version, authors, and license information.");
  parser.addOption(aboutOption);
  QCommandLineOption fileOption(QStringList() << "f" << "file",
      "Path to input image file, HTTP/HTTPS URL, or .list file (first entry is loaded).", "file");
  parser.addOption(fileOption);
  QCommandLineOption projectFileOption(QStringList() << "project", "Path to input JSON-project file.", "json");
  parser.addOption(projectFileOption);
  QCommandLineOption classFileOption(QStringList() << "class", "Path to input image class file.", "file");
  parser.addOption(classFileOption);
  QCommandLineOption outFileOption(QStringList() << "o" << "output", "Path to output image file.", "file");
  parser.addOption(outFileOption);
  QCommandLineOption batchOption("batch", "Run the application in batch mode without launching the graphical user interface (this is automatically enabled when an output file is specified).");
  parser.addOption(batchOption);
  QCommandLineOption guiOption("gui", "Run the application in GUI mode, even if no input has been provided.");
  parser.addOption(guiOption);
  QCommandLineOption configFileOption(QStringList() << "config", "Path to config file.", "file");
  parser.addOption(configFileOption);
  // QCommandLineOption updateLayerOption("update-layer", "Update layer mask generated from a polygon.");
  // parser.addOption(updateLayerOption);
  QCommandLineOption alphaMaskingOption("alpha-masking", "Forced alpha channel mask processing.");
  parser.addOption(alphaMaskingOption);
  QCommandLineOption skipValidationOption("skip-validation", "Skip all validation checks and force the loading of the input image.");
  parser.addOption(skipValidationOption);
  QCommandLineOption saveJSONOption(QStringList() << "save-json", "In batch mode, save a loaded project file in the latest version.", "file");
  parser.addOption(saveJSONOption);
  QCommandLineOption intermediateOption(QStringList() << "save-intermediate", "In batch mode, path to output an image after each step in the history.", "file");
  parser.addOption(intermediateOption);
  QCommandLineOption concatOption("concatenate", "Concatenate image transformations in batch mode.");
  parser.addOption(concatOption);
  QCommandLineOption historyOption("history", "Print history of last calls to stdout. Optional: last <n> entries.");
  parser.addOption(historyOption);
  QCommandLineOption forceOption("force", "Overwrite an existing output file.");
  parser.addOption(forceOption);
  QCommandLineOption scaleOption(QStringList() << "scale",
      "Coordinate scale factor between the project file resolution and the "
      "BigTIFF resolution (default: 20, i.e. project at 20 µm, BigTIFF at 1 µm).",
      "factor");
  parser.addOption(scaleOption);
  QCommandLineOption debugOption("debug", "Enable debug output to stdout.");
  parser.addOption(debugOption);
  QCommandLineOption verboseOption("verbose", "Enable verbose output to stdout.");
  parser.addOption(verboseOption);
  parser.process(*app);
  
  // --- history ---
  if ( parser.isSet(historyOption) ) {
    int n = -1;
    QStringList positionalArgs = parser.positionalArguments();
    if ( !positionalArgs.isEmpty() ) {
        bool ok;
        int val = positionalArgs.first().toInt(&ok);
        if ( ok ) {
            n = val;
        }
    }
    showHistory(n);
    exit(1);
  }
  
  // --- Check required options ---
  if ( !parser.isSet(fileOption) && !parser.isSet(projectFileOption) && !parser.isSet(guiOption)) {
   qCritical() << "Error: Missing path to image file and history file. Need at least one!";
   parser.showHelp();
  }
  // --- Set variables ---
  {
    QString imageFilePath = parser.value(fileOption);
    if ( imageFilePath.startsWith("http://") || imageFilePath.startsWith("https://") ) {
      obj["imageDisplayName"] = imageFilePath;
      std::cout << "Downloading image from URL: " << imageFilePath.toStdString() << std::endl;
      imageFilePath = downloadImageFromUrl(imageFilePath);
      if ( imageFilePath.isEmpty() ) exit(1);
    }
    // .list file: parse entries, use first as imagePath, store all in fileList
    if ( QFileInfo(imageFilePath).suffix().toLower() == "list" ) {
        const QStringList entries = parseFileList(imageFilePath);
        if ( entries.isEmpty() ) {
            std::cerr << "\033[1;31mERROR: \033[0m"
                      << "File list '" << imageFilePath.toStdString() << "' is empty." << std::endl;
            exit(1);
        }
        QJsonArray arr;
        for ( const auto& e : entries ) arr.append(e);
        obj["fileList"] = arr;
        imageFilePath = entries.first();
        obj["imageDisplayName"] = imageFilePath;
        if ( imageFilePath.startsWith("http://") || imageFilePath.startsWith("https://") ) {
            std::cout << "Downloading image from URL: " << imageFilePath.toStdString() << std::endl;
            imageFilePath = downloadImageFromUrl(imageFilePath);
            if ( imageFilePath.isEmpty() ) exit(1);
        }
    }
    if ( !validateFile(imageFilePath,"image file",{"png","mnc","mnc2","tif","tiff","h5","hdf5","hdf"}) ) {
      exit(1);
    }
    obj["imagePath"] = imageFilePath;
  }
  obj["outputPath"] = parser.value(outFileOption);
  obj["classPath"] = parser.value(classFileOption);
  obj["historyPath"] = parser.value(projectFileOption);
  if ( !validateFile(obj["historyPath"].toString(),"project",{"json"}) ) {
   exit(1);
  }
  obj["saveJSONPath"] = parser.value(saveJSONOption);
  obj["configPath"] = parser.value(configFileOption);
  obj["save-intermediate"] = parser.value(intermediateOption);
  if ( parser.isSet(intermediateOption) && !isPathWritable(obj["save-intermediate"].toString()) ) {
   exit(1);
  }
  obj["concatenate"] = parser.isSet(concatOption);
  obj["vulkan"] = false;
  obj["gpu"] = false;
  obj["alphaMasking"] = parser.isSet(alphaMaskingOption);
  obj["skipValidation"] = parser.isSet(skipValidationOption);
  obj["force"] = parser.isSet(forceOption);
  obj["debug"] = parser.isSet(debugOption);
  obj["verbose"] = parser.isSet(verboseOption);
  obj["scaleFactor"] = parser.isSet(scaleOption)
                       ? parser.value(scaleOption).toInt() : 20;
  
  return obj;
}

// ---------------------- Version check ----------------------
static void printLicense()
{
    QFile f(":/licence.txt");
    if ( f.open(QIODevice::ReadOnly | QIODevice::Text) )
        std::cout << f.readAll().toStdString() << std::endl;
    else
        std::cerr << "License file not found in resources." << std::endl;
}

static void printAuthors()
{
    QFile f(":/AUTHORS");
    if ( f.open(QIODevice::ReadOnly | QIODevice::Text) )
        std::cout << f.readAll().toStdString() << std::endl;
    else
        std::cerr << "AUTHORS file not found in resources." << std::endl;
}

static void printVersionInfo( int argc, char* argv[] )
{
    const QString localVer = APP_VERSION;
    std::cout << "ImageEditor " << localVer.toStdString() << std::endl;
    std::cout << "  BigTIFF support: yes (libtiff " << TIFFLIB_VERSION_STR_MAJ_MIN_MIC << ")" << std::endl;
#ifdef HASHDF5
    std::cout << "  HDF5 support:    yes (" << H5_VERSION << ")" << std::endl;
#else
    std::cout << "  HDF5 support:    no" << std::endl;
#endif

    // Spin up a minimal core app so QNetworkAccessManager works
    QCoreApplication app(argc, argv);
    app.setApplicationName("ImageEditor");
    app.setApplicationVersion(localVer);

    QNetworkAccessManager nam;
    QUrl url("https://api.github.com/repos/hmohlberg/ImageEditor/releases/latest");
    QNetworkRequest req(url);
    req.setRawHeader("Accept",     "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "ImageEditor-UpdateChecker/1.0");
    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if ( reply->error() != QNetworkReply::NoError ) {
        std::cerr << "Could not reach GitHub: "
                  << reply->errorString().toStdString() << std::endl;
        reply->deleteLater();
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    const QString githubTag = obj["tag_name"].toString().trimmed();
    reply->deleteLater();

    if ( githubTag.isEmpty() ) {
        std::cerr << "No release tag found on GitHub." << std::endl;
        return;
    }

    // Normalise "v1.2.3" → ["1","2","3"] and compare component-wise
    auto normalise = [](const QString& v) -> QStringList {
        QString s = v.trimmed();
        if ( s.startsWith('v') || s.startsWith('V') ) s = s.mid(1);
        return s.split('.', Qt::SkipEmptyParts);
    };
    auto isNewer = [&normalise](const QString& base, const QString& cand) -> bool {
        const QStringList b = normalise(base), c = normalise(cand);
        const int n = qMax(b.size(), c.size());
        for ( int i = 0; i < n; ++i ) {
            const int bv = (i < b.size()) ? b[i].toInt() : 0;
            const int cv = (i < c.size()) ? c[i].toInt() : 0;
            if ( cv > bv ) return true;
            if ( cv < bv ) return false;
        }
        return false;
    };

    if ( isNewer(localVer, githubTag) )
        std::cout << "Status: Outdated. Latest version on GitHub is "
                  << githubTag.toStdString() << "." << std::endl;
    else if ( isNewer(githubTag, localVer) )
        std::cout << "Status: Local version is ahead of GitHub (latest release: "
                  << githubTag.toStdString()
                  << "). The GitHub repository is not up to date." << std::endl;
    else
        std::cout << "Status: Up to date. No update available." << std::endl;
}

static void printAbout( int argc, char* argv[] )
{
    printVersionInfo(argc, argv);
    std::cout << std::endl;
    printAuthors();
    std::cout << std::endl;
    printLicense();
}

// ---------------------- Main ----------------------
int main( int argc, char *argv[] )
{   
    // --- prepare (required for Linux.Debian systems in batch mode) ---
    if ( getenv("DISPLAY") == nullptr ) {
      setenv("QT_QPA_PLATFORM", "offscreen", 1);
    }
    QImageReader::setAllocationLimit(0); // dangerous
    
    // --- check first for version / gui / batch options ---
    bool batchProcessing = false;
    bool guiProcessing = false;
    for ( int i=0 ; i<argc ; ++i ) {
     if ( QString(argv[i]) == "--version" || QString(argv[i]) == "-v" ) {
       printVersionInfo(argc, argv);
       return 0;
     }
     if ( QString(argv[i]) == "--license" || QString(argv[i]) == "-l" ) {
       printLicense();
       return 0;
     }
     if ( QString(argv[i]) == "--authors" || QString(argv[i]) == "-a" ) {
       printAuthors();
       return 0;
     }
     if ( QString(argv[i]) == "--about" ) {
       printAbout(argc, argv);
       return 0;
     }
     if ( QString(argv[i]) == "--debug" ) {
       qputenv("QT_LOGGING_RULES", "editor.graphics.debug=true");
     }
     if ( QString(argv[i]) == "--batch" || QString(argv[i]) == "--output" || QString(argv[i]) == "--save-json" ) batchProcessing = true;
     if ( QString(argv[i]) == "--gui" ) guiProcessing = true;
    }
    
    // --- check whether batch processing is requested ---
    if ( batchProcessing ) {
      QCoreApplication *app = new QCoreApplication(argc,argv);
      BatchMain batch;
      app->setApplicationName("ImageEditor");
      app->setApplicationVersion(APP_VERSION);
      QJsonObject parsedOptions = parser(app,argc);
      QString imagePath = parsedOptions.value("imagePath").toString("");
      QString historyPath = parsedOptions.value("historyPath").toString("");
      if ( historyPath.isEmpty() ) {
       printError("Invalid input. Missing required option '--project <filename>' in batch mode.");
       return 1;
      }
      bool forcedAlphaMasking = parsedOptions.value("alphaMasking").toBool();
      QString saveJSONPath = parsedOptions.value("saveJSONPath").toString("");
      if ( !saveJSONPath.isEmpty() ) {
        if ( QFile::exists(saveJSONPath) && parsedOptions.value("force").toBool() == false ) {
          printError(QString("Output JSON file '%1' already exists. Use command line option --force to overwrite.").arg(saveJSONPath));
          return 1; 
        }
        QImage image;
        if ( !imagePath.isEmpty() ) {
         ImageLoader loader;
         loader.load(imagePath,true);
         image = loader.getImage();
        }
        ImageProcessor proc(image);
        proc.process(historyPath,forcedAlphaMasking,false);
        QJsonDocument document = proc.document();
        QFile file(saveJSONPath); 
        if ( !file.open(QIODevice::WriteOnly | QIODevice::Text ) ) {
          qWarning() << "FATAL ERROR: Could not create new JSON file" << file.errorString();
          return 0;
        }
        QByteArray bytes = document.toJson(QJsonDocument::Indented);
         file.write(bytes);
        file.close();
        qInfo() << "Saved JSON file" << saveJSONPath << ".";
        return 0;
      }
      QString outputPath = parsedOptions.value("outputPath").toString("");
      if ( outputPath.isEmpty() ) {
       printError("Invalid input. Missing required option '--output <filename>' in batch mode.");
       return 1;
      }
      if ( QFile::exists(outputPath) && parsedOptions.value("force").toBool() == false ) {
        printError(QString("Output file '%1' already exists. Use command line option --force to overwrite.").arg(outputPath));
        return 1;
      }

      // BigTIFF input with TIFF output: use the tile-based BigTIFF pipeline
      // instead of loading the whole image into a QImage (which would fail
      // for large files and never produce BigTIFF output).
      {
        const QString outExt = QFileInfo(outputPath).suffix().toLower();
        if (!imagePath.isEmpty()
            && (outExt == "tif" || outExt == "tiff")
            && bigTiffIsBigTiff(imagePath))
        {
          if (QFile::exists(outputPath)) QFile::remove(outputPath);
          saveCurrentCall(argc, argv);

          QString errMsg;
          bool ok = false;

          if (!historyPath.isEmpty()) {
            // Load the project JSON and apply it tile-by-tile
            QFile pf(historyPath);
            if (!pf.open(QIODevice::ReadOnly)) {
              printError(QString("Cannot open project file: %1").arg(historyPath));
              return 1;
            }
            QJsonObject proj = QJsonDocument::fromJson(pf.readAll()).object();
            pf.close();

            int scaleFactor = parsedOptions.value("scaleFactor").toInt(20);
            qInfo() << "Applying project to BigTIFF (scale factor" << scaleFactor << ")…";
            ok = bigTiffApplyProject(imagePath, outputPath, proj,
                                     scaleFactor, {}, &errMsg);
            if (!ok) {
              printError(QString("BigTIFF project apply failed: %1").arg(errMsg));
              return 1;
            }
          } else {
            qInfo() << "BigTIFF input detected — copying pyramid to" << outputPath;
            ok = bigTiffCopyPyramid(imagePath, outputPath, {}, &errMsg);
            if (!ok) {
              printError(QString("BigTIFF copy failed: %1").arg(errMsg));
              return 1;
            }
          }

          qInfo() << "Saved BigTIFF to" << outputPath;
          return 0;
        }
      }

      QString saveIntermediatePath = parsedOptions.value("save-intermediate").toString("");
      ImageLoader loader;
      QImage image;
      if ( imagePath.isEmpty() ) {
       saveCurrentCall(argc, argv);
       ImageProcessor proc;
       proc.setIntermediatePath(saveIntermediatePath,outputPath);
       if ( !proc.process(historyPath,forcedAlphaMasking,true) ) {
        printError(QString("Malfunction in ImageProcessor::process(%1).").arg(historyPath));
        return 1;
       }
       image = proc.getOutputImage();
      } else {
       if ( loader.load(imagePath,true) ) {
        saveCurrentCall(argc, argv);
        Config::isWhiteBackgroundImage = loader.hasWhiteBackground();
        ImageProcessor proc(loader.getImage());
        proc.setIntermediatePath(saveIntermediatePath,outputPath);
        if ( !proc.process(historyPath,forcedAlphaMasking,true) ) {
         printError(QString("Malfunction in ImageProcessor::process(%1).").arg(historyPath));
         return 1;
        }
        image = proc.getOutputImage();
       } else {
        printError(QString("Malfunction in ImageLoader::load(%1).").arg(imagePath));
        return 1;
       }
      }
      image.setColorSpace(QColorSpace(QColorSpace::SRgb));
      if ( loader.saveAs(image,outputPath) ) {
       qInfo() << "Saved image file " << outputPath << ".";
       return 0;
      }
      return 0;
    }
    
    // --- gui processing ---
    QApplication *app = new QApplication(argc, argv);
    app->setApplicationName("ImageEditor");
    app->setApplicationVersion(APP_VERSION);
    app->setQuitOnLastWindowClosed(true);
    QJsonObject parsedOptions = parser(app,argc);
    // --- create new history entry ---
    saveCurrentCall(argc, argv);
    // --- load config file ---
    QString configPath = parsedOptions.value("configPath").toString("");
    if ( !configPath.isEmpty() ) {
      EditorStyle::instance().load(configPath);
    }
    setEnlargedStandardCursor(EditorStyle::instance().cursorSize(),
               EditorStyle::instance().cursorFillColor(),EditorStyle::instance().cursorBorderColor());
    int result = 0;
    {
      // --- call main programm ---
      MainWindow w(parsedOptions);
      w.show();
      result = app->exec();
    }
    _exit(result);
    // qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&){});
    // return result;
}