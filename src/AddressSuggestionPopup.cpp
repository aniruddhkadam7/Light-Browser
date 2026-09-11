#include "AddressSuggestionPopup.h"
#include "BookmarkManager.h"
#include "HistoryManager.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace {
constexpr int kMaxSuggestions = 8;
}

AddressSuggestionPopup::AddressSuggestionPopup(QLineEdit *addressBar, HistoryManager *history,
                                                 BookmarkManager *bookmarks, QWidget *anchor)
    : QWidget(anchor->window())
    , m_addressBar(addressBar)
    , m_history(history)
    , m_bookmarks(bookmarks)
    , m_anchor(anchor)
{
    // ToolTip (not Popup): Popup grabs input and auto-closes on any focus
    // change, which would steal focus away from the address bar the moment
    // this widget shows. WA_ShowWithoutActivating keeps focus on the line
    // edit so the user keeps typing/arrow-navigating uninterrupted.
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setObjectName("suggestionPopup");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_list = new QListWidget(this);
    m_list->setObjectName("suggestionList");
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setUniformItemSizes(true);
    m_list->setMouseTracking(true);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        acceptRow(m_list->row(item));
    });

    setStyleSheet(R"(
        QWidget#suggestionPopup {
            background: #2b2b2c;
            border: 1px solid #3a3a3c;
        }
        QListWidget#suggestionList {
            background: transparent;
            border: none;
            color: #e3e3e3;
            outline: none;
            font-size: 13px;
        }
        QListWidget#suggestionList::item {
            padding: 6px 10px;
            border-radius: 4px;
        }
        QListWidget#suggestionList::item:selected {
            background: #3a6ea5;
            color: white;
        }
        QListWidget#suggestionList::item:hover {
            background: #3a3a3c;
        }
    )");

    m_addressBar->installEventFilter(this);
    connect(m_addressBar, &QLineEdit::textEdited, this, &AddressSuggestionPopup::updateSuggestions);
}

void AddressSuggestionPopup::updateSuggestions(const QString &text)
{
    m_list->clear();

    if (text.trimmed().isEmpty()) {
        hidePopup();
        return;
    }

    QVector<HistoryEntry> historyMatches = m_history->search(text, kMaxSuggestions);
    const QVector<BookmarkEntry> bookmarkMatches = m_bookmarks->all();

    const QString needle = text.toLower();
    int shown = 0;
    for (const BookmarkEntry &b : bookmarkMatches) {
        if (shown >= kMaxSuggestions)
            break;
        if (!b.url.toString().toLower().contains(needle) && !b.title.toLower().contains(needle))
            continue;
        auto *item = new QListWidgetItem(QStringLiteral("★ ") +
                                          (b.title.isEmpty() ? b.url.toString() : b.title));
        item->setData(Qt::UserRole, b.url);
        item->setToolTip(b.url.toString());
        m_list->addItem(item);
        ++shown;
    }
    for (const HistoryEntry &h : historyMatches) {
        if (shown >= kMaxSuggestions)
            break;
        auto *item = new QListWidgetItem(h.title.isEmpty() ? h.url.toString() : h.title);
        item->setData(Qt::UserRole, h.url);
        item->setToolTip(h.url.toString());
        m_list->addItem(item);
        ++shown;
    }

    if (m_list->count() == 0) {
        hidePopup();
        return;
    }
    // No suggestion is "current" just because the list was (re)populated —
    // only an explicit arrow-key press should ever cause Enter to navigate
    // to a suggestion instead of literally whatever was typed.
    m_list->setCurrentRow(-1);
    showPopup();
}

void AddressSuggestionPopup::showPopup()
{
    const QPoint below = m_anchor->mapToGlobal(QPoint(0, m_anchor->height() + 4));
    setGeometry(below.x(), below.y(), m_anchor->width(), m_list->sizeHintForRow(0) * m_list->count() + 8);
    m_list->clearSelection();
    show();
    raise();
}

void AddressSuggestionPopup::hidePopup()
{
    hide();
}

void AddressSuggestionPopup::moveSelection(int delta)
{
    if (m_list->count() == 0)
        return;
    int row = m_list->currentRow();
    row = (row < 0) ? (delta > 0 ? 0 : m_list->count() - 1) : (row + delta + m_list->count()) % m_list->count();
    m_list->setCurrentRow(row);
}

void AddressSuggestionPopup::acceptRow(int row)
{
    if (row < 0 || row >= m_list->count())
        return;
    const QUrl url = m_list->item(row)->data(Qt::UserRole).toUrl();
    hidePopup();
    emit urlChosen(url);
}

bool AddressSuggestionPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_addressBar)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::FocusOut) {
        hidePopup();
    } else if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (!isVisible())
            return false;
        switch (keyEvent->key()) {
        case Qt::Key_Down:
            moveSelection(1);
            return true;
        case Qt::Key_Up:
            moveSelection(-1);
            return true;
        case Qt::Key_Escape:
            hidePopup();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_list->currentRow() >= 0) {
                acceptRow(m_list->currentRow());
                return true;
            }
            hidePopup();
            return false;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}
