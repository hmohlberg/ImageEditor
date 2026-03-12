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

#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QBrush>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStyle>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>

namespace QWidgetUtils
{

  // extract last integer number from a string
  int getLayerIndexFromString( const QString &text ) {
    QString buffer;
    bool foundDigit = false;
    for (int i = text.length() - 1; i >= 0; --i) {
      if (text[i].isDigit()) {
        buffer.prepend(text[i]);
        foundDigit = true;
      } else if (foundDigit) {
        break; 
      }
    }
    return buffer.toInt();
  }

  QWidget* getSeparatorLine() {
    QWidget* line = new QWidget();
    line->setFixedWidth(1);
    line->setStyleSheet("background-color: #A5A5A5;");
    line->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    return line;
  }
  
  QBrush createCheckerBrush( int tileSize=16, QColor c1=QColor("#3a3a3a"), QColor c2=QColor("#2a2a2a") ) {
    QPixmap pix(tileSize*2,tileSize*2);
    pix.fill(c1);
    QPainter p(&pix);
     p.fillRect(QRect(0, 0, tileSize, tileSize), c2);
     p.fillRect(QRect(tileSize, tileSize, tileSize, tileSize), c2);
    p.end();
    return QBrush(pix);
  }
  
  // --- --- ---
  int showIconDialog( QWidget *parent, const QString &title, const QString &labeltext ) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(600);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    // 1. Bereich: Icon und Text nebeneinander
    QHBoxLayout *contentLayout = new QHBoxLayout();
    
    // Icon vom System laden (z.B. Information, Warning oder Question)
    QLabel *iconLabel = new QLabel(&dialog);
    QIcon icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    iconLabel->setPixmap(icon.pixmap(64, 64)); 
    iconLabel->setAlignment(Qt::AlignTop);   
    iconLabel->setContentsMargins(0, 10, 0, 0);  
    
    QLabel *textLabel = new QLabel(labeltext, &dialog);
    textLabel->setWordWrap(true);
    
    contentLayout->addWidget(iconLabel);
    contentLayout->addSpacing(15); 
    contentLayout->addWidget(textLabel, 1); 
    
    mainLayout->addLayout(contentLayout);
    mainLayout->addSpacing(20); 

    // 2. Bereich: Rechtsbündige Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(); 

    QPushButton *btnOk = new QPushButton("Revoke", &dialog);
    QPushButton *btnIgnore = new QPushButton("Delete", &dialog);
    QPushButton *btnCancel = new QPushButton("Cancel", &dialog);

    buttonLayout->addWidget(btnOk);
    buttonLayout->addWidget(btnIgnore);
    buttonLayout->addWidget(btnCancel);

    mainLayout->addLayout(buttonLayout);

    // Rückgabewerte
    int result = 0;
    QObject::connect(btnOk, &QPushButton::clicked, [&]() { result = 1; dialog.accept(); });
    QObject::connect(btnIgnore, &QPushButton::clicked, [&]() { result = 2; dialog.accept(); });
    QObject::connect(btnCancel, &QPushButton::clicked, [&]() { result = 3; dialog.reject(); });

    dialog.exec();
    return result;
  }
  
  // --- --- ---
  bool handleMissingImage( const QString &imagePath, QString &outNewPath ) {
    QMessageBox msgBox;
    msgBox.setWindowTitle("Could not find image");
    msgBox.setText(QString("The image could not be found at the following path:\n\n%1").arg(imagePath));
    msgBox.setInformativeText("Do you want to cancel loading the project or select another file?");
    msgBox.setIcon(QMessageBox::Warning);
    QPushButton *searchButton = msgBox.addButton("Select new image path...", QMessageBox::ActionRole);
    QPushButton *abortButton = msgBox.addButton("Abort loading", QMessageBox::RejectRole);
    msgBox.exec();
    if ( msgBox.clickedButton() == searchButton ) {
        QString fileName = QFileDialog::getOpenFileName(nullptr, 
            "Select Image", "", "Image (*.png *.bmp *.jpg *.mnc)");
        if ( !fileName.isEmpty() ) {
            outNewPath = fileName;
            return true;
        }
    }
    return false;
  }
  
  // --- --- ---
  bool showImageMismatchError( const QString &imagePath, const QString &projectPath ) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setWindowTitle("Error loading the project file");
    msgBox.setText("Abort: Main images do not match.");
    msgBox.setInformativeText(QString(
        "Main images in the viewer and from the project file do not match.\n\n"
        "Main image in viewer: %1\n"
        "Main image in project file: %2\n\n"
        "Do you want to cancel loading or clear old data and reload project data?"
    ).arg(imagePath).arg(projectPath));
    QPushButton *clearDataButton = msgBox.addButton("Clear previous data and reload", QMessageBox::ActionRole);
    QPushButton *abortButton = msgBox.addButton("Abort loading", QMessageBox::RejectRole);
    msgBox.exec();
    if ( msgBox.clickedButton() == clearDataButton ) {
      return true;
    }
    return false;
  }

}