#include "HistoryManager.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {
constexpr int kMaxStoredEntries = 5000;

// Reduces a host to its registrable domain (last two labels) so
// "accounts.google.com" and "www.google.com" aggregate into one site
// instead of two near-identical Most Visited tiles.
QString registrableDomain(const QString &host)
{
    const QStringList parts = host.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() <= 2)
        return host.toLower();
    return (parts.at(parts.size() - 2) + QLatin1Char('.') + parts.last()).toLower();
}
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
        entry.favicon = QByteArray::fromBase64(o.value("favicon").toString().toLatin1());
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
        if (!entry.favicon.isEmpty())
            o["favicon"] = QString::fromLatin1(entry.favicon.toBase64());
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

void HistoryManager::setFavicon(const QUrl &url, const QByteArray &pngData)
{
    if (pngData.isEmpty())
        return;

    const int idx = std::find_if(m_entries.begin(), m_entries.end(),
                                  [&url](const HistoryEntry &e) { return e.url == url; })
        - m_entries.begin();
    if (idx >= m_entries.size() || m_entries[idx].favicon == pngData)
        return;

    m_entries[idx].favicon = pngData;
    save();
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

QVector<HistoryEntry> HistoryManager::mostVisited(int limit) const
{
    // Aggregate by registrable domain so a site visited via many different
    // pages/subdomains still counts as one tile, weighted by its total
    // visits; the tile links to that site's most recently visited page.
    QHash<QString, HistoryEntry> byDomain;
    QHash<QString, int> domainVisitCount;
    QHash<QString, QByteArray> domainFavicon;
    for (const HistoryEntry &entry : m_entries) {
        const QString host = entry.url.host();
        if (host.isEmpty())
            continue;
        const QString domain = registrableDomain(host);
        domainVisitCount[domain] += entry.visitCount;
        if (!entry.favicon.isEmpty())
            domainFavicon[domain] = entry.favicon;
        const auto it = byDomain.constFind(domain);
        if (it == byDomain.constEnd() || entry.lastVisited > it->lastVisited)
            byDomain[domain] = entry;
    }

    QVector<HistoryEntry> results = byDomain.values().toVector();
    // A site's favicon may have been captured on a different page visit than
    // the most-recent one picked as its representative tile.
    for (HistoryEntry &entry : results) {
        if (entry.favicon.isEmpty())
            entry.favicon = domainFavicon.value(registrableDomain(entry.url.host()));
    }
    std::sort(results.begin(), results.end(), [&domainVisitCount](const HistoryEntry &a, const HistoryEntry &b) {
        const int countA = domainVisitCount.value(registrableDomain(a.url.host()));
        const int countB = domainVisitCount.value(registrableDomain(b.url.host()));
        if (countA != countB)
            return countA > countB;
        return a.lastVisited > b.lastVisited;
    });

    if (results.size() > limit)
        results.resize(limit);
    return results;
}

void HistoryManager::clear()
{
    m_entries.clear();
    save();
    emit changed();
}
