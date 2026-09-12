#include "WebPage.h"

#include "OmniboxBridge.h"
#include "UrlRedirectManager.h"

#include <QTimer>
#include <QWebChannel>

WebPage::WebPage(QWebEngineProfile *profile, UrlRedirectManager *redirectManager,
                  NewWindowCallback callback, SearchCallback searchCallback, QObject *parent)
    : QWebEnginePage(profile, parent), m_redirectManager(redirectManager), m_callback(std::move(callback)),
      m_searchCallback(std::move(searchCallback))
{
}

QWebEnginePage *WebPage::createWindow(QWebEnginePage::WebWindowType type)
{
    if (m_callback)
        return m_callback(type);
    return QWebEnginePage::createWindow(type);
}

void WebPage::attachOmniboxChannel(QNetworkAccessManager *networkManager)
{
    if (!m_omniboxChannel) {
        m_omniboxBridge = new OmniboxBridge(networkManager, this);
        m_omniboxChannel = new QWebChannel(this);
        m_omniboxChannel->registerObject(QStringLiteral("omnibox"), m_omniboxBridge);
    }
    setWebChannel(m_omniboxChannel);
}

void WebPage::detachOmniboxChannel()
{
    setWebChannel(nullptr);
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
    if (isMainFrame && url.scheme() == QLatin1String("ltnav")) {
        if (m_searchCallback) {
            // Not a hierarchical URL, so Qt leaves the encoded text in
            // path() with no host; percent-decode it back to plain text.
            const QString encoded = url.path().isEmpty() ? url.toString().mid(6) : url.path();
            QTimer::singleShot(0, this, [this, encoded] {
                m_searchCallback(QUrl::fromPercentEncoding(encoded.toUtf8()));
            });
        }
        return false;
    }
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
