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

#pragma once

#include <QString>
#include <QDebug>

class BatchMain : public IMainSystem {
 
 public:

    BatchMain() { 
        IMainSystem::setInstance(this); 
    }
    
    void showMessage( const QString &text, int msgType = 0 ) override {
        // printf("%s\n", text.toLocal8Bit().constData()); // Ausgabe in Konsole
        qDebug() << "BATCHMAIN: " << text;
    }
    void updateLayerOperationParameter( int mode, double value1, double value2 = 0.0 ) override {
       // nothing to do
    }
    double getLayerOperationParameter( int mode ) override {
        return 0.0;
    }
    QString getSelectedLayerItemName() override { 
        return ""; 
    }

};