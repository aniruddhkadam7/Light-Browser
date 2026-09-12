#pragma once

#include <QByteArray>
#include <QObject>
#include <QUrl>
#include <QVector>

struct HistoryEntry
{
    QUrl url;
    QString title;
    qint64 lastVisited = 0; // ms since epoch
    int visitCount = 1;
    QByteArray favicon; // PNG bytes, empty if none captured yet
};

// Tracks visited pages, persisted as JSON under the app's data directory.
// Most-recent-first in memory so recentEntries()/search() need no sorting.
class HistoryManager : public QObject
{
    Q_OBJECT
public:
    explicit HistoryManager(QObject *parent = nullptr);

    void recordVisit(const QUrl &url, const QString &title);
    // Attaches/refreshes the favicon already fetched by QtWebEngine for a
    // recorded visit (no extra network request — just persisting what the
    // browser already has), for the New Tab page's Most Visited tiles.
    void setFavicon(const QUrl &url, const QByteArray &pngData);
    QVector<HistoryEntry> recentEntries(int limit = 300) const;
    QVector<HistoryEntry> search(const QString &query, int limit = 8) const;
    // Top distinct hosts by visit count (one entry per host — its most
    // recently visited page), for the New Tab page's "Most Visited" tiles.
    QVector<HistoryEntry> mostVisited(int limit = 8) const;
    void clear();

signals:
    void changed();

private:
    void load();
    void save() const;
    QString storagePath() const;

    QVector<HistoryEntry> m_entries;
};
