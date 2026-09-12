#pragma once

#include <QWebEnginePage>
#include <functional>

class UrlRedirectManager;
class QWebChannel;
class QNetworkAccessManager;
class OmniboxBridge;

class WebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    using NewWindowCallback = std::function<QWebEnginePage *(QWebEnginePage::WebWindowType)>;
    // Called with the decoded text when the local New Tab page's search box
    // navigates to the internal "ltnav:" scheme (see NewTabPage.cpp) — lets
    // BrowserWindow resolve it exactly like address-bar input.
    using SearchCallback = std::function<void(const QString &)>;

    WebPage(QWebEngineProfile *profile, UrlRedirectManager *redirectManager,
            NewWindowCallback callback, SearchCallback searchCallback = nullptr,
            QObject *parent = nullptr);

    // Used by WebView's context menu for actions with no built-in WebAction
    // (e.g. "Open image in new tab") — createWindow() itself is protected,
    // so this is the one narrow public door into it.
    QWebEnginePage *openInNewTab(const QUrl &url);

    // Wires up (creating on first use) the "omnibox" QWebChannel object the
    // New Tab page's search box calls into for remote search suggestions.
    // Only attached while the New Tab page is actually showing — detach on
    // any real navigation so no page loaded afterward in this tab can reach
    // it (see BrowserWindow::showNewTabPage()/navigateViewTo()).
    void attachOmniboxChannel(QNetworkAccessManager *networkManager);
    void detachOmniboxChannel();

protected:
    QWebEnginePage *createWindow(QWebEnginePage::WebWindowType type) override;
    // Catches navigations WebEngine drives itself — link clicks, form
    // submits, and JS-triggered (e.g. button onclick) location changes —
    // none of which go through BrowserWindow::navigateViewTo, so without
    // this a mapping rule only ever applied to address-bar/suggestion/
    // favorite navigations and never to in-page ones.
    bool acceptNavigationRequest(const QUrl &url, QWebEnginePage::NavigationType type,
                                  bool isMainFrame) override;

private:
    UrlRedirectManager *m_redirectManager;
    NewWindowCallback m_callback;
    SearchCallback m_searchCallback;
    QWebChannel *m_omniboxChannel = nullptr;
    OmniboxBridge *m_omniboxBridge = nullptr;
};
