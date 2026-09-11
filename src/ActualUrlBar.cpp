#include "ActualUrlBar.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

ActualUrlBar::ActualUrlBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("actualUrlBar");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 4, 14, 4);
    layout->setSpacing(8);

    auto *label = new QLabel(tr("Actual:"), this);
    label->setObjectName("actualUrlLabel");

    m_edit = new QLineEdit(this);
    m_edit->setObjectName("actualUrlEdit");
    m_edit->setReadOnly(true);
    m_edit->setFrame(false);

    auto *copyButton = new QToolButton(this);
    copyButton->setObjectName("actualUrlCopyButton");
    copyButton->setText(tr("Copy"));
    copyButton->setToolTip(tr("Copy actual URL"));
    connect(copyButton, &QToolButton::clicked, this, [this] {
        QGuiApplication::clipboard()->setText(m_edit->text());
    });

    layout->addWidget(label);
    layout->addWidget(m_edit, 1);
    layout->addWidget(copyButton);
}

void ActualUrlBar::setActualUrl(const QUrl &url)
{
    m_edit->setText(url.toString());
    m_edit->setCursorPosition(0);
}
