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

#include "Updater.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

static const char* kApiUrl =
    "https://api.github.com/repos/hmohlberg/ImageEditor/releases/latest";

Updater::Updater(const QString& currentVersion, QObject* parent)
    : QObject(parent),
      m_currentVersion(currentVersion),
      m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this,  &Updater::onReplyFinished);
}

void Updater::checkForUpdates()
{
    QUrl url(kApiUrl);
    QNetworkRequest req(url);
    req.setRawHeader("Accept",     "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "ImageEditor-UpdateChecker/1.0");
    m_nam->get(req);
}

// Compare two dotted version strings such as "1.2.3" or "v1.2.3".
// Returns true if candidate is strictly newer than current.
bool Updater::isNewer(const QString& current, const QString& candidate)
{
    auto strip = [](const QString& v) -> QStringList {
        QString s = v.trimmed();
        if ( s.startsWith('v') || s.startsWith('V') ) s = s.mid(1);
        return s.split('.', Qt::SkipEmptyParts);
    };
    const QStringList cur = strip(current);
    const QStringList cnd = strip(candidate);
    const int n = qMax(cur.size(), cnd.size());
    for ( int i = 0; i < n; ++i ) {
        const int c = (i < cur.size()) ? cur[i].toInt() : 0;
        const int d = (i < cnd.size()) ? cnd[i].toInt() : 0;
        if ( d > c ) return true;
        if ( d < c ) return false;
    }
    return false;
}

void Updater::onReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if ( reply->error() != QNetworkReply::NoError ) {
        emit checkFailed(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if ( doc.isNull() ) {
        emit checkFailed(QString("JSON parse error: %1").arg(parseErr.errorString()));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tag = obj.value("tag_name").toString();
    const QString url = obj.value("html_url").toString();

    if ( tag.isEmpty() ) {
        emit checkFailed("GitHub response contained no tag_name");
        return;
    }

    if ( isNewer(m_currentVersion, tag) )
        emit updateAvailable(tag, url);
    else if ( isNewer(tag, m_currentVersion) )
        emit localVersionNewer(m_currentVersion, tag);
    else
        emit noUpdateAvailable();
}
