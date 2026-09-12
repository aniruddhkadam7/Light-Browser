#include "SearchProvider.h"

#include <QSettings>
#include <QUrlQuery>

namespace {
// client=firefox/list-style params select the plain-JSON OpenSearch response
// shape instead of each provider's default (a JSONP callback, or an HTML
// results page) — a documented, widely-used response *format* switch, not a
// User-Agent header or any other identity spoofing.
const QVector<SearchProvider> kProviders = {
    {QStringLiteral("google"), QStringLiteral("Google"), QUrl(QStringLiteral("https://www.google.com/search")),
     QStringLiteral("google."), QUrl(QStringLiteral("https://www.google.com/complete/search?client=firefox"))},
    {QStringLiteral("bing"), QStringLiteral("Bing"), QUrl(QStringLiteral("https://www.bing.com/search")),
     QStringLiteral("bing."), QUrl(QStringLiteral("https://www.bing.com/osjson.aspx"))},
    {QStringLiteral("duckduckgo"), QStringLiteral("DuckDuckGo"), QUrl(QStringLiteral("https://duckduckgo.com/")),
     QStringLiteral("duckduckgo."), QUrl(QStringLiteral("https://duckduckgo.com/ac/?type=list"))},
};
}

const QVector<SearchProvider> &SearchProviders::all()
{
    return kProviders;
}

SearchProvider SearchProviders::current()
{
    // DuckDuckGo by default: unlike Google, it doesn't challenge normal
    // browser traffic with an IP-reputation CAPTCHA, so a shared/cloud IP
    // doesn't get every search blocked. Still just a QSettings value —
    // switching back to "google" (or to "bing") is a one-line change, no
    // code path is Google/DuckDuckGo-specific.
    const QString id = QSettings().value(QStringLiteral("searchProviderId"), QStringLiteral("duckduckgo")).toString();
    for (const SearchProvider &provider : kProviders) {
        if (provider.id == id)
            return provider;
    }
    return kProviders.first();
}

QUrl SearchProviders::buildSearchUrl(const SearchProvider &provider, const QString &query)
{
    QUrl url = provider.searchBaseUrl;
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("q"), query);
    url.setQuery(urlQuery);
    return url;
}
