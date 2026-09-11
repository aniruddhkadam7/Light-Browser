#include "WebPage.h"

#include "UrlRedirectManager.h"

#include <QTimer>

WebPage::WebPage(QWebEngineProfile *profile, UrlRedirectManager *redirectManager,
                  NewWindowCallback callback, QObject *parent)
    : QWebEnginePage(profile, parent), m_redirectManager(redirectManager), m_callback(std::move(callback))
{
}

QWebEnginePage *WebPage::createWindow(QWebEnginePage::WebWindowType type)
{
    if (m_callback)
        return m_callback(type);
    return QWebEnginePage::createWindow(type);
}

QWebEnginePage *WebPage::openInNewTab(const QUrl &url)
{
    QWebEnginePage *newPage = createWindow(QWebEnginePage::WebBrowserTab);
    if (newPage)
        newPage->setUrl(url);
    return newPage;
}

bool WebPage::acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type, bool isMainFrame)
{
    Q_UNUSED(type);
    if (isMainFrame && m_redirectManager) {
        QUrl mapped;
        if (m_redirectManager->resolve(url, &mapped) && mapped != url) {
            // Can't call setUrl() from inside acceptNavigationRequest itself
            // (this navigation hasn't finished being rejected yet); defer to
            // the next event-loop turn.
            QTimer::singleShot(0, this, [this, mapped] { setUrl(mapped); });
            return false;
        }
    }
    return true;
}
