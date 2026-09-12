#pragma once

#include <QString>
#include <QUrl>
#include <QVector>

// A pluggable search engine definition. Only Google/Bing/DuckDuckGo are
// listed today, selected via the "searchProviderId" QSettings key (default
// "google") — no UI to switch yet, but resolveInput()/the New Tab search box
// both go through this instead of a hardcoded google.com URL, so adding a
// picker later is just reading a different id, not touching either call
// site.
struct SearchProvider
{
    QString id;
    QString displayName;
    QUrl searchBaseUrl; // query text goes in as its "q" query item
    QString tabTitleHostMarker; // substring of host() that marks a results tab
    // Suggestion ("did you mean") endpoint returning the standard OpenSearch
    // suggestions shape ["query", ["completion1", "completion2", ...]] —
    // an invalid/empty QUrl means this provider has no suggest endpoint
    // wired up yet, and the New Tab omnibox just falls back to local
    // history-only suggestions for it.
    QUrl suggestBaseUrl;
};

namespace SearchProviders {
const QVector<SearchProvider> &all();
SearchProvider current();
QUrl buildSearchUrl(const SearchProvider &provider, const QString &query);
}
