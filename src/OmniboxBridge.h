#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

// Exposed to the New Tab page's JS via QWebChannel as "omnibox". The page
// debounces on its own side (see NewTabPage.cpp) and calls
// requestSuggestions() at most once per pause in typing; this additionally
// aborts any still-in-flight request first, so at most one suggestion
// request per tab is ever on the wire at a time.
//
// Only wired up while the New Tab page itself is showing (WebPage::
// attachOmniboxChannel()/detachOmniboxChannel()) — once the tab navigates to
// real content the channel is detached, so no other page loaded later in
// that tab can reach this object.
class OmniboxBridge : public QObject
{
    Q_OBJECT
public:
    explicit OmniboxBridge(QNetworkAccessManager *networkManager, QObject *parent = nullptr);

public slots:
    void requestSuggestions(const QString &query);

signals:
    // Always emitted, even on failure/timeout/non-JSON response (Google's
    // own CAPTCHA page, for instance) — with an empty list, so the page can
    // gracefully fall back to local history alone instead of hanging.
    void suggestionsReady(const QString &query, const QStringList &suggestions);

private:
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_pendingReply = nullptr;
};
