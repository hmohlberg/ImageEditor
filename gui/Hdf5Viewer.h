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
*/

#pragma once

#include <QWidget>
#include <QRgb>
#include <QSize>
#include <QVector>

struct Hdf5Level {
    int64_t rows = 0, cols = 0;
    int64_t chunkRows = 2048, chunkCols = 2048;
    int     channels  = 3;
    QString datasetPath;
};

class Hdf5GraphicsView;

class Hdf5Viewer : public QWidget
{
    Q_OBJECT

public:
    explicit Hdf5Viewer(QWidget* parent = nullptr);
    ~Hdf5Viewer();

    bool    open(const QString& path);
    void    closeFile();
    bool    isOpen() const;
    QString filePath() const { return m_filePath; }
    QSize   imageSize() const;
    void    setColorTable(const QVector<QRgb>& lut);

signals:
    void closeRequested();

private slots:
    void zoomIn();
    void zoomOut();
    void fitView();

private:
    void updateInfoLabel();

    Hdf5GraphicsView* m_view      = nullptr;
    QString           m_filePath;
    class QLabel*     m_infoLabel = nullptr;
};
