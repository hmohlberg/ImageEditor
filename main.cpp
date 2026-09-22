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
#include <QSslSocket>
#include <QUrlQuery>

#include <iostream>
#include <unistd.h>

#include "core/Config.h"
#include "core/IMainSystem.h"
#include "core/BatchMain.h"
#include "core/ImageLoader.h"
#include "core/ImageProcessor.h"
#ifdef HASTIFF
#include "core/BigTiffIO.h"
#include "core/BigTiffProjectApply.h"
#endif

#include "gui/MainWindow.h"
#include "core/version.h"

#include <tiffvers.h>
#ifdef HASHDF5
#  include <hdf5.h>
#endif
#ifdef HASITK
#  include <itkConfigure.h>
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
    if ( history.size() > 1000 ) history.removeLast();
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
    const QString urlExt = QFileInfo(QUrl(url).path()).suffix().toLower();
    static const QStringList knownExts = {
        "png","jpg","jpeg","bmp","tif","tiff","h5","hdf5","hdf","mnc","mnc2","list","json"};
    const QString useExt  = knownExts.contains(urlExt) ? urlExt : "png";
    const QString tempPath = QDir::tempPath() + "/imageeditor_url_download." + useExt;
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

struct FLEntry { QString title, imagePath, projectPath; };

static QList<FLEntry> parseFileList( const QString& path )
{
    QList<FLEntry> entries;
    QFile f(path);
    if ( !f.open(QIODevice::ReadOnly | QIODevice::Text) ) return entries;
    const QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    QString searchPath;
    int autoIndex = 1;
    for ( const auto& line : lines ) {
        const QString t = line.trimmed();
        if ( t.isEmpty() || t.startsWith('#') ) continue;

        if ( t.contains(';') ) {
            // new format: title;imagePath[;projectPath]
            const QStringList parts = t.split(';');
            const QString title = parts[0].trimmed();
            const QString img   = parts.size() > 1 ? parts[1].trimmed() : QString();
            const QString proj  = parts.size() > 2 ? parts[2].trimmed() : QString();
            if ( title.compare("path", Qt::CaseInsensitive) == 0 ) {
                searchPath = img; continue;
            }
            if ( img.isEmpty() ) continue;
            auto resolve = [&](const QString& p) -> QString {
                if ( p.startsWith("http://") || p.startsWith("https://") || p.startsWith("github://") )
                    return p;
                return QFileInfo(p).isAbsolute() ? p
                       : ( searchPath.isEmpty() ? p : searchPath + "/" + p );
            };
            entries.append({ title, resolve(img), resolve(proj) });
        } else {
            // old format (no semicolon) — auto-title "Image N"
            if ( t.startsWith("http://") || t.startsWith("https://") ) {
                entries.append({ QString("Image %1").arg(autoIndex++), t, {} }); continue;
            }
            QFileInfo fi(t);
            if ( fi.isAbsolute() ) {
                if ( fi.isDir() ) { searchPath = t; continue; }
                entries.append({ QString("Image %1").arg(autoIndex++), t, {} });
            } else {
                const QString resolved = searchPath.isEmpty() ? t : searchPath + "/" + t;
                entries.append({ QString("Image %1").arg(autoIndex++), resolved, {} });
            }
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
  QCommandLineOption docksOption("docks", "Show layer and history docks on startup.");
  parser.addOption(docksOption);
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
  // pre-load config so github:// expansion uses the configured base URL
  {
    const QStringList args = app->arguments();
    for ( int i = 1; i < args.size() - 1; ++i ) {
      if ( args[i] == "--config" ) {
        EditorStyle::instance().load(args[i + 1]);
        break;
      }
    }
  }

  // --- Set variables ---
  {
    QString imageFilePath = parser.value(fileOption);
    // expand github:// shorthand
    if ( imageFilePath.startsWith("github://") )
      imageFilePath = EditorStyle::instance().githubBaseUrl()
                      + "/" + imageFilePath.mid(9);
#ifdef HASTIFF
    auto isWebBigTiffUrl = [](const QString& url) -> bool {
        return (url.startsWith("http://") || url.startsWith("https://"))
               && QUrlQuery(QUrl(url)).hasQueryItem("resolution");
    };
#else
    auto isWebBigTiffUrl = [](const QString&) -> bool { return false; };
#endif
    if ( imageFilePath.startsWith("http://") || imageFilePath.startsWith("https://") ) {
      if ( !isWebBigTiffUrl(imageFilePath) ) {
        std::cout << "Downloading image from URL: " << imageFilePath.toStdString() << std::endl;
        imageFilePath = downloadImageFromUrl(imageFilePath);
        if ( imageFilePath.isEmpty() ) exit(1);
      }
    }
    // .list file: parse entries, use first as imagePath, store all in fileList
    if ( QFileInfo(imageFilePath).suffix().toLower() == "list" ) {
        if ( !validateFile(imageFilePath, "image file", {"list"}) ) exit(1);
        const QList<FLEntry> entries = parseFileList(imageFilePath);
        if ( entries.isEmpty() ) {
            std::cerr << "\033[1;31mERROR: \033[0m"
                      << "File list '" << imageFilePath.toStdString() << "' is empty." << std::endl;
            exit(1);
        }
        QJsonArray arr;
        for ( const auto& e : entries ) {
            QJsonObject o;
            o["title"]       = e.title;
            o["imagePath"]   = e.imagePath;
            o["projectPath"] = e.projectPath;
            arr.append(o);
        }
        obj["fileList"] = arr;
        // use first entry's image
        imageFilePath = entries.first().imagePath;
        if ( imageFilePath.startsWith("github://") )
            imageFilePath = EditorStyle::instance().githubBaseUrl() + "/" + imageFilePath.mid(9);
        obj["imageDisplayName"]  = entries.first().title;
        obj["imageOriginalPath"] = imageFilePath; // URL/path before download, for filelist dedup
        if ( imageFilePath.startsWith("http://") || imageFilePath.startsWith("https://") ) {
            if ( !isWebBigTiffUrl(imageFilePath) ) {
                std::cout << "Downloading image from URL: " << imageFilePath.toStdString() << std::endl;
                imageFilePath = downloadImageFromUrl(imageFilePath);
                if ( imageFilePath.isEmpty() ) exit(1);
            }
        }
        // use first entry's project as default (--project will override below if set)
        if ( !entries.first().projectPath.isEmpty() ) {
            QString projPath = entries.first().projectPath;
            if ( projPath.startsWith("github://") )
                projPath = EditorStyle::instance().githubBaseUrl() + "/" + projPath.mid(9);
            if ( projPath.startsWith("http://") || projPath.startsWith("https://") ) {
                std::cout << "Downloading project from URL: " << projPath.toStdString() << std::endl;
                projPath = downloadImageFromUrl(projPath);
                if ( projPath.isEmpty() ) exit(1);
            }
            obj["historyPath"] = projPath;
        }
    }
    if ( !isWebBigTiffUrl(imageFilePath) ) {
      if ( !validateFile(imageFilePath,"image file",{"png","mnc","mnc2","tif","tiff","h5","hdf5","hdf"}) ) {
        exit(1);
      }
    }
    obj["imagePath"] = imageFilePath;
  }
  obj["outputPath"] = parser.value(outFileOption);
  obj["classPath"] = parser.value(classFileOption);
  {
    QString projectPath = parser.value(projectFileOption);
    if ( projectPath == "none" ) {
      obj["projectNone"] = true;
    } else if ( !projectPath.isEmpty() ) {
      // expand github:// shorthand
      if ( projectPath.startsWith("github://") )
        projectPath = EditorStyle::instance().githubBaseUrl() + "/" + projectPath.mid(9);
      // download if URL
      if ( projectPath.startsWith("http://") || projectPath.startsWith("https://") ) {
        if ( !parser.isSet(fileOption) ) {
          std::cerr << "\033[1;31mERROR: \033[0m"
                    << "--file is required when loading a project via URL.\n";
          exit(1);
        }
        std::cout << "Downloading project from URL: " << projectPath.toStdString() << std::endl;
        projectPath = downloadImageFromUrl(projectPath);
        if ( projectPath.isEmpty() ) exit(1);
      } else {
        if ( !validateFile(projectPath,"project",{"json"}) ) exit(1);
      }
      obj["historyPath"] = projectPath;
      // flag that --project was given explicitly on the CLI so MainWindow can ask
      // which filelist entry it belongs to (only relevant when a filelist is loaded)
      if ( obj.contains("fileList") )
          obj["projectFromCLI"] = true;
    }
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
  obj["showDocks"] = parser.isSet(docksOption);
  obj["scaleFactor"] = parser.isSet(scaleOption)
                       ? parser.value(scaleOption).toInt() : 20;
  
  return obj;
}

// ---------------------- Version check ----------------------
static void printLicense()
{
    QFile f(":/LICENSE.txt");
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

// Prints build info — requires QCoreApplication to already exist (for SSL query).
static void printBuildInfo()
{
    const QString localVer = APP_VERSION;
    std::cout << "ImageEditor " << localVer.toStdString() << std::endl;
    std::cout << "  Qt:              " << QT_VERSION_STR << std::endl;
#ifdef TIFFLIB_VERSION_STR_MAJ_MIN_MIC
    std::cout << "  BigTIFF support: yes (libtiff " << TIFFLIB_VERSION_STR_MAJ_MIN_MIC << ")" << std::endl;
#else
    std::cout << "  BigTIFF support: yes (libtiff >= 4.0)" << std::endl;
#endif
#ifdef HASHDF5
    std::cout << "  HDF5 support:    yes (" << H5_VERSION << ")" << std::endl;
#else
    std::cout << "  HDF5 support:    no" << std::endl;
#endif
#ifdef HASITK
    std::cout << "  ITK support:     yes ("
              << ITK_VERSION_MAJOR << "." << ITK_VERSION_MINOR << "." << ITK_VERSION_PATCH
              << ")" << std::endl;
#else
    std::cout << "  ITK support:     no" << std::endl;
#endif
    {
        const QString sslVer = QSslSocket::sslLibraryVersionString();
        std::cout << "  SSL:             "
                  << ( sslVer.isEmpty() ? "not available" : sslVer.toStdString() )
                  << std::endl;
    }
}

// Checks GitHub for a newer release — requires QCoreApplication to already exist.
static void checkForUpdates()
{
    const QString localVer = APP_VERSION;
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

static void printVersionInfo( int argc, char* argv[] )
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("ImageEditor");
    app.setApplicationVersion(APP_VERSION);
    printBuildInfo();
    checkForUpdates();
}

static void printAbout( int argc, char* argv[] )
{
    // One QCoreApplication shared by all three steps so Qt resources stay accessible.
    QCoreApplication app(argc, argv);
    app.setApplicationName("ImageEditor");
    app.setApplicationVersion(APP_VERSION);
    printBuildInfo();
    checkForUpdates();
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
      // file lists are not supported in batch mode — check before full parsing
      for ( int i = 1; i < argc - 1; ++i ) {
        if ( QString(argv[i]) == "--file" ) {
          const QString val = QString(argv[i + 1]);
          // strip github:// / https:// to get the path component for suffix check
          const QString checkPath = val.startsWith("github://") || val.startsWith("http://") || val.startsWith("https://")
              ? QUrl(val).path() : val;
          if ( QFileInfo(checkPath).suffix().toLower() == "list" ) {
            printError("File lists (.list) are not supported in batch mode.");
            return 1;
          }
          break;
        }
      }
      QCoreApplication *app = new QCoreApplication(argc,argv);
      BatchMain batch;
      app->setApplicationName("ImageEditor");
      app->setApplicationVersion(APP_VERSION);
      QJsonObject parsedOptions = parser(app,argc);
      QString imagePath = parsedOptions.value("imagePath").toString("");
      QString historyPath = parsedOptions.value("historyPath").toString("");
      const bool projectNone = parsedOptions.value("projectNone").toBool();
      if ( historyPath.isEmpty() && !projectNone ) {
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

      // BigTIFF input: use specialised pipelines instead of QImage::load()
      // which would try to load the full-resolution image into RAM.
#ifdef HASTIFF
      {
        const QString outExt = QFileInfo(outputPath).suffix().toLower();
        const bool isTiffOut = (outExt == "tif" || outExt == "tiff");
        if (!imagePath.isEmpty() && bigTiffIsBigTiff(imagePath)) {
          if (QFile::exists(outputPath)) QFile::remove(outputPath);
          saveCurrentCall(argc, argv);

          const int scaleFactor = parsedOptions.value("scaleFactor").toInt(20);
          QString errMsg;

          if (isTiffOut) {
            // ── tile-based BigTIFF → BigTIFF pipeline ──────────────────────
            bool ok = false;
            if (!historyPath.isEmpty()) {
              QFile pf(historyPath);
              if (!pf.open(QIODevice::ReadOnly)) {
                printError(QString("Cannot open project file: %1").arg(historyPath));
                return 1;
              }
              QJsonObject proj = QJsonDocument::fromJson(pf.readAll()).object();
              pf.close();
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

          } else {
            // ── BigTIFF → raster image (PNG, …) via pyramid level ──────────
            QImage img = bigTiffReadLevel(imagePath, scaleFactor, &errMsg);
            if (img.isNull()) {
              printError(QString("BigTIFF read failed: %1").arg(errMsg));
              return 1;
            }
            if (!projectNone && !historyPath.isEmpty()) {
              ImageProcessor proc(img);
              proc.setIntermediatePath(
                  parsedOptions.value("save-intermediate").toString(""), outputPath);
              if (!proc.process(historyPath, forcedAlphaMasking, true)) {
                printError(QString("Processing failed for BigTIFF level."));
                return 1;
              }
              img = proc.getOutputImage();
            }
            img.setColorSpace(QColorSpace(QColorSpace::SRgb));
            ImageLoader saver;
            if (saver.saveAs(img, outputPath)) {
              qInfo() << "Saved" << outputPath;
              return 0;
            }
            printError(QString("Could not save output file: %1").arg(outputPath));
            return 1;
          }
        }
      }
#endif

      QString saveIntermediatePath = parsedOptions.value("save-intermediate").toString("");
      ImageLoader loader;
      QImage image;
      if ( projectNone ) {
       // --project none: load input and write copy without any processing
       if ( imagePath.isEmpty() ) {
        printError("--project none requires --file.");
        return 1;
       }
       saveCurrentCall(argc, argv);
       if ( !loader.load(imagePath, true) ) {
        printError(QString("Malfunction in ImageLoader::load(%1).").arg(imagePath));
        return 1;
       }
       image = loader.getImage();
      } else if ( imagePath.isEmpty() ) {
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