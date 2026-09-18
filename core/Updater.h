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

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

// Checks the GitHub releases API for a newer version of the application.
// Call checkForUpdates() once at startup; the result arrives asynchronously
// via updateAvailable() or noUpdateAvailable().
class Updater : public QObject
{
    Q_OBJECT

public:
    explicit Updater(const QString& currentVersion, QObject* parent = nullptr);

    void checkForUpdates();

signals:
    void updateAvailable(const QString& newVersion, const QString& releaseUrl);
    void noUpdateAvailable();
    void localVersionNewer(const QString& localVersion, const QString& githubVersion);
    void checkFailed(const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    static bool isNewer(const QString& current, const QString& candidate);

    QString                  m_currentVersion;
    QNetworkAccessManager*   m_nam;
};
