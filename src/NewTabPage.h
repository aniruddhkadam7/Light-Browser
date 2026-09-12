#pragma once

#include <QString>
#include <QVector>

struct HistoryEntry;

// Builds the static local New Tab page: no network requests, no remote
// resources — everything (styles, icons, tile data, and the history list the
// omnibox-style search box matches against as the user types) is inlined
// into one HTML string handed to QWebEnginePage::setHtml().
namespace NewTabPage {
QString build(const QVector<HistoryEntry> &mostVisited, const QVector<HistoryEntry> &recentHistory,
               const QString &searchProviderName);

// The Chrome/Brave-style "You've gone Incognito" page: same search box and
// omnibox suggestion wiring, but no Most Visited tiles and no local-history
// suggestions — matching what those browsers show on an incognito New Tab.
QString buildIncognito(const QString &searchProviderName);
}
