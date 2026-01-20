/*
   SPDX-FileCopyrightText: 2026 (c) OpenAI Assistant

   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "radioartworkresolver.h"

#include "elisa_settings.h"
#include "playerLogging.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;

namespace
{
constexpr auto kDiscogsEndpoint = "https://api.discogs.com/database/search";
constexpr auto kUserAgent = "Elisa (https://kde.org/elisa)";
}

RadioArtworkResolver::RadioArtworkResolver(QObject *parent)
    : QObject(parent)
{
}

void RadioArtworkResolver::requestArtwork(const QPersistentModelIndex &index, const QUrl &streamUrl, const QString &title, const QString &artistOrStation)
{
    if (!index.isValid() || !isDiFmStream(streamUrl)) {
        return;
    }

    QString artist;
    QString trackTitle;
    if (!parseArtistTitle(title, artistOrStation, artist, trackTitle)) {
        return;
    }

    const auto key = QStringLiteral("%1 - %2").arg(artist, trackTitle).trimmed().toLower();

    if (mCache.contains(key)) {
        const auto cached = mCache.value(key);
        if (cached.isValid() && !cached.isEmpty()) {
            Q_EMIT artworkResolved(index, cached);
        }
        return;
    }

    mPending[key].push_back(index);
    if (mInFlight.contains(key)) {
        return;
    }

    mInFlight.insert(key);
    startDiscogsLookup(key, artist, trackTitle);
}

void RadioArtworkResolver::startDiscogsLookup(const QString &key, const QString &artist, const QString &title)
{
    const auto token = Elisa::ElisaConfiguration::discogsToken().trimmed();
    if (token.isEmpty()) {
        finishKey(key, {});
        return;
    }

    QUrl url(QString::fromLatin1(kDiscogsEndpoint));
    QUrlQuery query;
    query.addQueryItem(u"artist"_s, artist);
    query.addQueryItem(u"track"_s, title);
    query.addQueryItem(u"type"_s, u"release"_s);
    query.addQueryItem(u"per_page"_s, u"1"_s);
    query.addQueryItem(u"page"_s, u"1"_s);
    url.setQuery(query.toString(QUrl::FullyEncoded));

    auto request = buildRequest(url);
    request.setRawHeader("Authorization", QByteArray("Discogs token=") + token.toUtf8());
    request.setRawHeader("Accept", "application/json");

    auto reply = mNetworkAccess.get(request);
    mReplyKey.insert(reply, key);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const auto key = mReplyKey.take(reply);

        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            finishKey(key, {});
            return;
        }

        const auto payload = reply->readAll();
        reply->deleteLater();

        const auto document = QJsonDocument::fromJson(payload);
        if (!document.isObject()) {
            finishKey(key, {});
            return;
        }

        const auto results = document.object().value(u"results"_s).toArray();
        if (results.isEmpty()) {
            finishKey(key, {});
            return;
        }

        const auto resultObject = results.first().toObject();
        QString coverUrl = resultObject.value(u"cover_image"_s).toString();
        if (coverUrl.isEmpty()) {
            coverUrl = resultObject.value(u"thumb"_s).toString();
        }

        finishKey(key, QUrl(coverUrl));
    });
}

void RadioArtworkResolver::finishKey(const QString &key, const QUrl &url)
{
    mInFlight.remove(key);
    if (url.isValid() && !url.isEmpty()) {
        mCache.insert(key, url);
    }

    const auto pending = mPending.take(key);
    if (url.isEmpty()) {
        return;
    }

    for (const auto &index : pending) {
        if (index.isValid()) {
            Q_EMIT artworkResolved(index, url);
        }
    }
}

bool RadioArtworkResolver::parseArtistTitle(const QString &streamTitle, const QString &artistOrStation, QString &artist, QString &title) const
{
    const auto trimmedTitle = streamTitle.trimmed();
    const auto separator = u" - "_s;

    if (trimmedTitle.contains(separator)) {
        const auto parts = trimmedTitle.split(separator);
        if (parts.size() >= 2) {
            artist = parts.first().trimmed();
            title = parts.mid(1).join(separator).trimmed();
        }
    } else if (!artistOrStation.trimmed().isEmpty() && !trimmedTitle.isEmpty()) {
        artist = artistOrStation.trimmed();
        title = trimmedTitle;
    }

    return !artist.isEmpty() && !title.isEmpty();
}

bool RadioArtworkResolver::isDiFmStream(const QUrl &streamUrl) const
{
    const auto host = streamUrl.host().toLower();
    return host.contains(u"di.fm"_s) || host.contains(u"digitallyimported"_s);
}

QNetworkRequest RadioArtworkResolver::buildRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QVariant(QString::fromLatin1(kUserAgent)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(u"application/json"_s));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

#include "moc_radioartworkresolver.cpp"
