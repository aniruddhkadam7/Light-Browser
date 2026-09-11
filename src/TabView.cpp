#include "TabView.h"
#include "WebView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineView>

TabView::TabView(WebView *webView, QWidget *parent)
    : QSplitter(Qt::Horizontal, parent), m_webView(webView)
{
    setObjectName("tabView");
    setChildrenCollapsible(false);
    setHandleWidth(4);
    addWidget(webView);

    connect(webView, &WebView::inspectRequested, this, &TabView::openDevTools);
}

void TabView::openDevTools()
{
    if (!m_devToolsPane) {
        auto *pane = new QWidget(this);
        pane->setObjectName("devToolsPane");
        auto *paneLayout = new QVBoxLayout(pane);
        paneLayout->setContentsMargins(0, 0, 0, 0);
        paneLayout->setSpacing(0);

        auto *header = new QWidget(pane);
        header->setObjectName("devToolsHeader");
        auto *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(10, 4, 4, 4);

        auto *title = new QLabel(tr("DevTools"), header);
        title->setObjectName("devToolsTitle");

        auto *closeButton = new QToolButton(header);
        closeButton->setObjectName("devToolsCloseButton");
        closeButton->setText(QStringLiteral("✕"));
        connect(closeButton, &QToolButton::clicked, this, &TabView::closeDevTools);

        headerLayout->addWidget(title);
        headerLayout->addStretch();
        headerLayout->addWidget(closeButton);

        m_devToolsView = new QWebEngineView(pane);

        paneLayout->addWidget(header);
        paneLayout->addWidget(m_devToolsView, 1);

        pane->setStyleSheet(R"(
            QWidget#devToolsHeader {
                background: #202021;
                border-bottom: 1px solid #3a3a3c;
            }
            QLabel#devToolsTitle {
                color: #c8c8c9;
                font-size: 12px;
                font-weight: 600;
            }
            QToolButton#devToolsCloseButton {
                background: transparent;
                border: none;
                border-radius: 4px;
                color: #c8c8c9;
                padding: 3px 7px;
            }
            QToolButton#devToolsCloseButton:hover {
                background: #3a3a3c;
            }
        )");

        m_devToolsPane = pane;
        addWidget(pane);
        const int total = qMax(width(), 900);
        setSizes({ total * 6 / 10, total * 4 / 10 });
    }

    m_webView->page()->setDevToolsPage(m_devToolsView->page());
    m_webView->page()->triggerAction(QWebEnginePage::InspectElement);
    m_devToolsPane->show();
}

void TabView::closeDevTools()
{
    if (m_devToolsPane)
        m_devToolsPane->hide();
}
