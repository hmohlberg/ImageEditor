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
#include <QApplication>

namespace QWidgetUtils
{

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

}