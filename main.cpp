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
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QDateTime>
#include <QString>
#include <QFile>
#include <QDir>

#include <iostream>

#include "core/Config.h"
#include "core/ImageLoader.h"
#include "core/ImageProcessor.h"

#include "gui/MainWindow.h"

// ---------------------- Init ----------------------
bool Config::verbose = false;
bool Config::isWhiteBackgroundImage = true;

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

static bool isPathWritable( const QString &path ) {
    QFileInfo checkInfo(path);
    if ( !checkInfo.exists() ) {
        // qDebug() << "Error: Path does not exists: " << path;
        return false;
    }
    if ( !checkInfo.isWritable() ) {
        // qDebug() << "Error: Path is write-protected or no permission: " << path;
        return false;
    }
    return true;
}

// ---------------------- Command Line Options ----------------------
static QJsonObject parser( const QCoreApplication *app, int argc ) {
  QJsonObject obj;
  QCommandLineParser parser;
  parser.setApplicationDescription("A simple ImageEditor with JSON-history support.");
  parser.addHelpOption();
  parser.addVersionOption();   
  QCommandLineOption fileOption(QStringList() << "f" << "file", "Path to input image file.", "file");
  parser.addOption(fileOption);
  QCommandLineOption projectFileOption(QStringList() << "project", "Path to input JSON-project file.", "json");
  parser.addOption(projectFileOption);
  QCommandLineOption classFileOption(QStringList() << "class", "Path to input image class file.", "file");
  parser.addOption(classFileOption);
  QCommandLineOption outFileOption(QStringList() << "o" << "output", "Path to output image file.", "file");
  parser.addOption(outFileOption);
  QCommandLineOption batchOption("batch", "Run application in batch mode without running graphical user interface.");
  parser.addOption(batchOption);
  QCommandLineOption configFileOption(QStringList() << "config", "Path to config file.", "file");
  parser.addOption(configFileOption);
  QCommandLineOption intermediateOption(QStringList() << "save-intermediate", "In batch mode, path to output an image after each step in the history.", "file");
  parser.addOption(intermediateOption);
  QCommandLineOption vulkanOption("vulkan", "If available enable hardware accelerated Vulkan rendering.");
  parser.addOption(vulkanOption);
  QCommandLineOption historyOption("history", "Print history of last calls to stdout. Optional: last <n> entries.");
  parser.addOption(historyOption);
  QCommandLineOption forceOption("force", "Overwrite an existing output file.");
  parser.addOption(forceOption);
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
  if ( !parser.isSet(fileOption) && !parser.isSet(projectFileOption) ) {
   qCritical() << "Error: Missing path to image file and history file. Need at least one!";
   parser.showHelp();
  }
  // --- Set variables ---
  obj["imagePath"] = parser.value(fileOption);
  if ( !validateFile(obj["imagePath"].toString(),"image file",{"png","mnc","mnc2","tif","tiff"}) ) {
    exit(1);
  }
  obj["outputPath"] = parser.value(outFileOption);
  obj["classPath"] = parser.value(classFileOption);
  obj["imagePath"] = parser.value(fileOption);
  if ( !validateFile(obj["imagePath"].toString(),"image file",{"png","mnc","mnc2","tif","tiff"}) ) {
   exit(1);
  }
  obj["historyPath"] = parser.value(projectFileOption);
  if ( !validateFile(obj["historyPath"].toString(),"project",{"json"}) ) {
   exit(1);
  }
  obj["configPath"] = parser.value(configFileOption);
  obj["save-intermediate"] = parser.value(intermediateOption);
  if ( parser.isSet(intermediateOption) && !isPathWritable(obj["save-intermediate"].toString()) ) {
   exit(1);
  }
  obj["vulkan"] = parser.isSet(vulkanOption);
  obj["force"] = parser.isSet(forceOption);
  obj["verbose"] = parser.isSet(verboseOption);
  
  return obj;
}

// ---------------------- Main ----------------------
int main( int argc, char *argv[] )
{
    // --- prepare (required for Linux.Debian systems in batch mode) ---
    if ( getenv("DISPLAY") == nullptr ) {
      setenv("QT_QPA_PLATFORM", "offscreen", 1);
    }
    QImageReader::setAllocationLimit(0); // dangerous
    
    // --- check first for gui option ---
    bool batchProcssing = false;
    for ( int i=0 ; i<argc ; ++i ) {
     if ( QString(argv[i]) == "--batch" ) batchProcssing = true;
    }
    
    // --- check whether batch processing is requested ---
    if ( batchProcssing ) {
      QCoreApplication *app = new QCoreApplication(argc,argv);
      app->setApplicationName("ImageEditor");
      app->setApplicationVersion("1.0");
      QJsonObject parsedOptions = parser(app,argc);
      QString imagePath = parsedOptions.value("imagePath").toString("");
      QString historyPath = parsedOptions.value("historyPath").toString("");
      if ( historyPath.isEmpty() ) {
       printError("Invalid input. Missing required option '--project <filename>' in batch mode.");
       return 1;
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
      QString saveIntermediatePath = parsedOptions.value("save-intermediate").toString("");
      ImageLoader loader;
      QImage image;
      if ( imagePath.isEmpty() ) {
       saveCurrentCall(argc, argv);
       ImageProcessor proc;
       proc.setIntermediatePath(saveIntermediatePath,outputPath);
       if ( !proc.process(historyPath) ) {
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
        if ( !proc.process(historyPath) ) {
         printError(QString("Malfunction in ImageProcessor::process(%1).").arg(historyPath));
         return 1;
        }
        image = proc.getOutputImage();
       } else {
        printError(QString("Malfunction in ImageLoader::load(%1).").arg(imagePath));
        return 0;
       }
      }
      image.setColorSpace(QColorSpace(QColorSpace::SRgb)); // no change
      if ( loader.saveAs(image,outputPath) ) {
       qInfo() << "Saved image file '" << outputPath << "'.";
       return 1;
      }
      return 1;
    }
    
    // --- gui processing ---
    QApplication *app = new QApplication(argc, argv);
    app->setApplicationName("ImageEditor");
    app->setApplicationVersion("1.0");
    QJsonObject parsedOptions = parser(app,argc);
    // --- create new history entry ---
    saveCurrentCall(argc, argv);
    // --- load config file ---
    QString configPath = parsedOptions.value("configPath").toString("");
    if ( !configPath.isEmpty() ) {
      EditorStyle::instance().load(configPath);
      qCDebug(logEditor) << "hi";
    }
    // --- call main programm ---
    MainWindow w(parsedOptions);
    w.show();
    return app->exec();
    
}