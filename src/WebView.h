#pragma once

#include <QWebEngineView>

// QWebEngineView's own contextMenuEvent() builds Qt's default menu, which is
// a bare, unstyled handful of actions (Back/Forward/Reload/Save page/View
// page source with no separators or per-context items) — nothing like what
// a real browser shows for a link, image, selection, or editable field.
// This subclass builds that menu itself from lastContextMenuRequest(),
// matching real-browser context menus (per-context item groups) and reusing
// the app's existing QMenu#chromeMenu styling via QSS cascade.
class WebView : public QWebEngineView
{
    Q_OBJECT
public:
    explicit WebView(QWidget *parent = nullptr);

signals:
    // The actual DevTools pane is owned by this tab's TabView (docked
    // side-by-side in the same window, like Edge/Chrome) rather than by the
    // page view itself, so opening it — whether from the context menu or a
    // keyboard shortcut — just asks upward for it via this signal.
    void inspectRequested();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};
