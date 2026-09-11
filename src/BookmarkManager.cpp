#include "BookmarkManager.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

BookmarkManager::BookmarkManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QString BookmarkManager::storagePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/bookmarks.json";
}

void BookmarkManager::load()
{
    QFile file(storagePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray())
        return;

    m_entries.clear();
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        BookmarkEntry entry;
        entry.url = QUrl(o.value("url").toString());
        entry.title = o.value("title").toString();
        if (!entry.url.isEmpty())
            m_entries.append(entry);
    }
}

void BookmarkManager::save() const
{
    QJsonArray arr;
    for (const BookmarkEntry &entry : m_entries) {
        QJsonObject o;
        o["url"] = entry.url.toString();
        o["title"] = entry.title;
        arr.append(o);
    }

    QFile file(storagePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void BookmarkManager::add(const QUrl &url, const QString &title)
{
    if (url.isEmpty() || isBookmarked(url))
        return;
    m_entries.prepend({url, title});
    save();
    emit changed();
}

void BookmarkManager::remove(const QUrl &url)
{
    const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                                  [&url](const BookmarkEntry &e) { return e.url == url; });
    if (it == m_entries.end())
        return;
    m_entries.erase(it);
    save();
    emit changed();
}

bool BookmarkManager::isBookmarked(const QUrl &url) const
{
    return std::any_of(m_entries.begin(), m_entries.end(),
                        [&url](const BookmarkEntry &e) { return e.url == url; });
}

QVector<BookmarkEntry> BookmarkManager::all() const
{
    return m_entries;
}
