#include "HistoryManager.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
constexpr int kMaxStoredEntries = 5000;
}

HistoryManager::HistoryManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QString HistoryManager::storagePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/history.json";
}

void HistoryManager::load()
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
        HistoryEntry entry;
        entry.url = QUrl(o.value("url").toString());
        entry.title = o.value("title").toString();
        entry.lastVisited = static_cast<qint64>(o.value("lastVisited").toDouble());
        entry.visitCount = o.value("visitCount").toInt(1);
        if (!entry.url.isEmpty())
            m_entries.append(entry);
    }
}

void HistoryManager::save() const
{
    QJsonArray arr;
    for (const HistoryEntry &entry : m_entries) {
        QJsonObject o;
        o["url"] = entry.url.toString();
        o["title"] = entry.title;
        o["lastVisited"] = static_cast<double>(entry.lastVisited);
        o["visitCount"] = entry.visitCount;
        arr.append(o);
    }

    QFile file(storagePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void HistoryManager::recordVisit(const QUrl &url, const QString &title)
{
    if (url.isEmpty() || url.scheme() == "about" || url.scheme() == "chrome")
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    const int idx = std::find_if(m_entries.begin(), m_entries.end(),
                                  [&url](const HistoryEntry &e) { return e.url == url; })
        - m_entries.begin();

    if (idx < m_entries.size()) {
        HistoryEntry entry = m_entries.takeAt(idx);
        entry.title = title.isEmpty() ? entry.title : title;
        entry.lastVisited = now;
        entry.visitCount += 1;
        m_entries.prepend(entry);
    } else {
        HistoryEntry entry;
        entry.url = url;
        entry.title = title;
        entry.lastVisited = now;
        entry.visitCount = 1;
        m_entries.prepend(entry);
        if (m_entries.size() > kMaxStoredEntries)
            m_entries.resize(kMaxStoredEntries);
    }

    save();
    emit changed();
}

QVector<HistoryEntry> HistoryManager::recentEntries(int limit) const
{
    if (m_entries.size() <= limit)
        return m_entries;
    return m_entries.mid(0, limit);
}

QVector<HistoryEntry> HistoryManager::search(const QString &query, int limit) const
{
    QVector<HistoryEntry> results;
    if (query.isEmpty())
        return results;

    const QString needle = query.toLower();
    for (const HistoryEntry &entry : m_entries) {
        if (entry.url.toString().toLower().contains(needle)
            || entry.title.toLower().contains(needle)) {
            results.append(entry);
            if (results.size() >= limit)
                break;
        }
    }
    return results;
}

void HistoryManager::clear()
{
    m_entries.clear();
    save();
    emit changed();
}
