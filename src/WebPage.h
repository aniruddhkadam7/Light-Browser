#pragma once

#include <QWebEnginePage>
#include <functional>

class UrlRedirectManager;

class WebPage : public QWebEnginePage
{
    Q_OBJECT
public:
    using NewWindowCallback = std::function<QWebEnginePage *(QWebEnginePage::WebWindowType)>;

    WebPage(QWebEngineProfile *profile, UrlRedirectManager *redirectManager,
            NewWindowCallback callback, QObject *parent = nullptr);

    // Used by WebView's context menu for actions with no built-in WebAction
    // (e.g. "Open image in new tab") — createWindow() itself is protected,
    // so this is the one narrow public door into it.
    QWebEnginePage *openInNewTab(const QUrl &url);

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
};
