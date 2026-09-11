#pragma once

#include <QSplitter>

class WebView;
class QWebEngineView;
class QWidget;

// One tab's content: the page (WebView) plus an optional docked DevTools
// pane opened side-by-side in the same tab — matching Edge/Chrome, where
// DevTools is docked inside the browser window rather than a separate
// top-level window.
class TabView : public QSplitter
{
    Q_OBJECT
public:
    explicit TabView(WebView *webView, QWidget *parent = nullptr);

    WebView *webView() const { return m_webView; }

public slots:
    void openDevTools();
    void closeDevTools();

private:
    WebView *m_webView;
    QWidget *m_devToolsPane = nullptr;
    QWebEngineView *m_devToolsView = nullptr;
};
