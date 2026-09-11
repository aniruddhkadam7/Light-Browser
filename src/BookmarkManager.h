#pragma once

#include <QObject>
#include <QUrl>
#include <QVector>

struct BookmarkEntry
{
    QUrl url;
    QString title;
};

// Persisted (JSON, app data directory) list of favorited pages.
class BookmarkManager : public QObject
{
    Q_OBJECT
public:
    explicit BookmarkManager(QObject *parent = nullptr);

    void add(const QUrl &url, const QString &title);
    void remove(const QUrl &url);
    bool isBookmarked(const QUrl &url) const;
    QVector<BookmarkEntry> all() const;

signals:
    void changed();

private:
    void load();
    void save() const;
    QString storagePath() const;

    QVector<BookmarkEntry> m_entries;
};
