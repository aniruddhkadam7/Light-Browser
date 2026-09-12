#include "OmniboxBridge.h"

#include "SearchProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

namespace {
// Providers disagree on suggestion response shape: Google/Bing use the
// classic OpenSearch ["query", ["completion", ...]] pair, DuckDuckGo's `ac`
// endpoint instead returns a bare array of {"phrase": "completion"}
// objects. Try both rather than hardcoding one, since whichever provider is
// actually configured determines which shape shows up.
QStringList parseSuggestions(const QByteArray &body)
{
    QStringList suggestions;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray())
        return suggestions;
    const QJsonArray arr = doc.array();

    if (arr.size() > 1 && arr.at(1).isArray()) {
        for (const QJsonValue &v : arr.at(1).toArray()) {
            if (v.isString())
                suggestions << v.toString();
        }
        return suggestions;
    }

    for (const QJsonValue &v : arr) {
        if (v.isObject() && v.toObject().contains(QStringLiteral("phrase")))
            suggestions << v.toObject().value(QStringLiteral("phrase")).toString();
    }
    return suggestions;
}
}

OmniboxBridge::OmniboxBridge(QNetworkAccessManager *networkManager, QObject *parent)
    : QObject(parent), m_networkManager(networkManager)
{
}

void OmniboxBridge::requestSuggestions(const QString &query)
{
    if (m_pendingReply) {
        m_pendingReply->abort();
        m_pendingReply = nullptr;
    }

    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        emit suggestionsReady(query, {});
        return;
    }

    const SearchProvider provider = SearchProviders::current();
    if (provider.suggestBaseUrl.isEmpty() || !provider.suggestBaseUrl.isValid()) {
        emit suggestionsReady(query, {});
        return;
    }

    QUrl url = provider.suggestBaseUrl;
    QUrlQuery urlQuery(url);
    urlQuery.addQueryItem(QStringLiteral("q"), trimmed);
    url.setQuery(urlQuery);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QVariant::fromValue(QNetworkRequest::NoLessSafeRedirectPolicy));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) LightBrowser/1.0"));

    QNetworkReply *reply = m_networkManager->get(request);
    m_pendingReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply, query] {
        reply->deleteLater();
        if (m_pendingReply == reply)
            m_pendingReply = nullptr;

        // A CAPTCHA/"unusual traffic" response comes back as HTML, not
        // JSON — parseSuggestions() just returns an empty list for it.
        const QStringList suggestions =
            reply->error() == QNetworkReply::NoError ? parseSuggestions(reply->readAll()) : QStringList();
        emit suggestionsReady(query, suggestions);
    });
}
