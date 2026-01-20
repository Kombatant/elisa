/*
   SPDX-FileCopyrightText: 2026 (c) OpenAI Assistant

   SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef RADIOARTWORKRESOLVER_H
#define RADIOARTWORKRESOLVER_H

#include "elisaLib_export.h"

#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QPersistentModelIndex>
#include <QSet>
#include <QUrl>

class QNetworkReply;

class ELISALIB_EXPORT RadioArtworkResolver : public QObject
{
    Q_OBJECT

public:
    explicit RadioArtworkResolver(QObject *parent = nullptr);

    void requestArtwork(const QPersistentModelIndex &index, const QUrl &streamUrl, const QString &title, const QString &artistOrStation);

Q_SIGNALS:
    void artworkResolved(const QPersistentModelIndex &index, const QUrl &url);

private:
    void startDiscogsLookup(const QString &key, const QString &artist, const QString &title);
    void finishKey(const QString &key, const QUrl &url);

    [[nodiscard]] bool parseArtistTitle(const QString &streamTitle, const QString &artistOrStation, QString &artist, QString &title) const;

    [[nodiscard]] bool isDiFmStream(const QUrl &streamUrl) const;

    [[nodiscard]] QNetworkRequest buildRequest(const QUrl &url) const;

    QNetworkAccessManager mNetworkAccess;
    QHash<QString, QUrl> mCache;
    QHash<QString, QList<QPersistentModelIndex>> mPending;
    QHash<QNetworkReply *, QString> mReplyKey;
    QSet<QString> mInFlight;
};

#endif // RADIOARTWORKRESOLVER_H
