#pragma once

#include <QObject>
#include <QUrl>
#include <QVector>

struct HistoryEntry
{
    QUrl url;
    QString title;
    qint64 lastVisited = 0; // ms since epoch
    int visitCount = 1;
};

// Tracks visited pages, persisted as JSON under the app's data directory.
// Most-recent-first in memory so recentEntries()/search() need no sorting.
class HistoryManager : public QObject
{
    Q_OBJECT
public:
    explicit HistoryManager(QObject *parent = nullptr);

    void recordVisit(const QUrl &url, const QString &title);
    QVector<HistoryEntry> recentEntries(int limit = 300) const;
    QVector<HistoryEntry> search(const QString &query, int limit = 8) const;
    void clear();

signals:
    void changed();

private:
    void load();
    void save() const;
    QString storagePath() const;

    QVector<HistoryEntry> m_entries;
};
