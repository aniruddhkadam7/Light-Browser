#include "WebView.h"
#include "WebPage.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QWebEngineContextMenuRequest>
#include <QWebEnginePage>

namespace {
// Wraps an existing QWebEnginePage::WebAction (whose own text/shortcut we
// don't want) in a menu item with our own label, while still triggering the
// real action and mirroring its current enabled state.
QAction *addProxyAction(QMenu &menu, const QString &text, QAction *action)
{
    QAction *item = menu.addAction(text);
    item->setEnabled(action->isEnabled());
    QObject::connect(item, &QAction::triggered, action, &QAction::trigger);
    return item;
}
}

WebView::WebView(QWidget *parent) : QWebEngineView(parent)
{
}

void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    QWebEngineContextMenuRequest *req = lastContextMenuRequest();
    QWebEnginePage *p = page();
    if (!req || !p) {
        QWebEngineView::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);
    menu.setObjectName("chromeMenu");

    if (!req->linkUrl().isEmpty()) {
        addProxyAction(menu, tr("Open link in new tab"), p->action(QWebEnginePage::OpenLinkInNewTab));
        addProxyAction(menu, tr("Open link in new window"), p->action(QWebEnginePage::OpenLinkInNewWindow));
        addProxyAction(menu, tr("Copy link address"), p->action(QWebEnginePage::CopyLinkToClipboard));
        menu.addSeparator();
    }

    if (req->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage) {
        const QUrl mediaUrl = req->mediaUrl();
        menu.addAction(tr("Open image in new tab"), this, [p, mediaUrl] {
            if (auto *webPage = qobject_cast<WebPage *>(p))
                webPage->openInNewTab(mediaUrl);
        });
        addProxyAction(menu, tr("Save image as"), p->action(QWebEnginePage::DownloadImageToDisk));
        addProxyAction(menu, tr("Copy image"), p->action(QWebEnginePage::CopyImageToClipboard));
        addProxyAction(menu, tr("Copy image link"), p->action(QWebEnginePage::CopyImageUrlToClipboard));
        menu.addSeparator();
    }

    if (req->isContentEditable()) {
        addProxyAction(menu, tr("Cut"), p->action(QWebEnginePage::Cut));
        addProxyAction(menu, tr("Copy"), p->action(QWebEnginePage::Copy));
        addProxyAction(menu, tr("Paste"), p->action(QWebEnginePage::Paste));
        addProxyAction(menu, tr("Select all"), p->action(QWebEnginePage::SelectAll));
        menu.addSeparator();
    } else if (!req->selectedText().isEmpty()) {
        addProxyAction(menu, tr("Copy"), p->action(QWebEnginePage::Copy));
        menu.addSeparator();
    }

    addProxyAction(menu, tr("Back"), p->action(QWebEnginePage::Back));
    addProxyAction(menu, tr("Forward"), p->action(QWebEnginePage::Forward));
    addProxyAction(menu, tr("Reload"), p->action(QWebEnginePage::Reload));
    menu.addSeparator();
    addProxyAction(menu, tr("Save page as"), p->action(QWebEnginePage::SavePage));
    addProxyAction(menu, tr("View page source"), p->action(QWebEnginePage::ViewSource));
    menu.addSeparator();
    menu.addAction(tr("Inspect"), this, &WebView::inspectRequested);

    menu.exec(event->globalPos());
}
