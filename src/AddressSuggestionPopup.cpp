#include "AddressSuggestionPopup.h"
#include "BookmarkManager.h"
#include "HistoryManager.h"
#include "OmniboxBridge.h"
#include "SearchProvider.h"

#include <QEvent>
#include <QFocusEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kMaxSuggestions = 8;
constexpr int kMaxHistoryRows = 3; // secondary section, kept small on purpose
constexpr int kMaxSearchRows = 5;
constexpr int kDebounceMs = 250;

// Mirrors BrowserWindow::resolveInput()'s own heuristics (display/dispatch
// purposes only — Enter still goes through the real resolveInput()).
bool looksLikeUrlText(const QString &trimmed)
{
    static const QRegularExpression hostLike(R"(^([\w-]+\.)+[a-zA-Z]{2,}(:\d+)?(/.*)?$)");
    static const QRegularExpression ipLike(R"(^(\d{1,3}\.){3}\d{1,3}(:\d+)?(/.*)?$)");
    const QUrl asUrl(trimmed);
    if (asUrl.scheme() == QLatin1String("http") || asUrl.scheme() == QLatin1String("https")
        || asUrl.scheme() == QLatin1String("file") || asUrl.scheme() == QLatin1String("ftp"))
        return true;
    return !trimmed.contains(QLatin1Char(' '))
        && (hostLike.match(trimmed).hasMatch() || ipLike.match(trimmed).hasMatch()
            || trimmed == QLatin1String("localhost") || trimmed.startsWith(QLatin1String("localhost:")));
}
}

AddressSuggestionPopup::AddressSuggestionPopup(QLineEdit *addressBar, HistoryManager *history,
                                                 BookmarkManager *bookmarks, QNetworkAccessManager *networkManager,
                                                 QWidget *anchor)
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
    m_list->setUniformItemSizes(false);
    m_list->setMouseTracking(true);
    layout->addWidget(m_list);

    // itemPressed (mouse *press*), not itemClicked (mouse *release*):
    // clicking the popup can trigger a FocusOut on the address bar in
    // between press and release (its window briefly taking activation),
    // which calls hidePopup() and would cancel a release-triggered click
    // before it ever fires. Acting on the press instead means the row is
    // already committed before that race has a chance to happen.
    connect(m_list, &QListWidget::itemPressed, this, [this](QListWidgetItem *item) {
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

    m_omniboxBridge = new OmniboxBridge(networkManager, this);
    connect(m_omniboxBridge, &OmniboxBridge::suggestionsReady, this,
            [this](const QString &query, const QStringList &suggestions) {
                if (query != m_addressBar->text().trimmed())
                    return; // stale reply for text the user has since changed/cleared
                m_remoteQuery = query;
                m_remoteSuggestions = suggestions;
                rebuild(m_addressBar->text());
            });

    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(kDebounceMs);
    connect(m_debounceTimer, &QTimer::timeout, this, [this] {
        const QString text = m_addressBar->text().trimmed();
        // Fetching search completions for a literal URL isn't useful and is
        // exactly the kind of unnecessary background request to avoid.
        if (!text.isEmpty() && !looksLikeUrlText(text))
            m_omniboxBridge->requestSuggestions(text);
    });

    m_addressBar->installEventFilter(this);
    connect(m_addressBar, &QLineEdit::textEdited, this, [this](const QString &text) {
        rebuild(text);
        m_debounceTimer->start(); // restarts if already running — one request per pause in typing
    });
}

void AddressSuggestionPopup::closeNow()
{
    hidePopup();
}

void AddressSuggestionPopup::rebuild(const QString &textIn)
{
    m_list->clear();
    const QString trimmed = textIn.trimmed();

    if (trimmed.isEmpty()) {
        const QVector<HistoryEntry> recent = m_history->recentEntries(kMaxSuggestions);
        if (!recent.isEmpty()) {
            addHeading(tr("Frequently visited"));
            for (const HistoryEntry &h : recent)
                addRow(h.title.isEmpty() ? h.url.toString() : h.title, h.url, h.url.toString());
        }
        if (m_list->count() == 0) {
            hidePopup();
            return;
        }
        m_list->setCurrentRow(-1);
        showPopup();
        return;
    }

    // Direct-address row — shown first, like Chrome's own omnibox leading
    // with a recognizable address above search.
    const bool looksLikeUrl = looksLikeUrlText(trimmed);
    if (looksLikeUrl) {
        const QUrl asUrl(trimmed);
        const QUrl directUrl = (asUrl.scheme() == QLatin1String("http") || asUrl.scheme() == QLatin1String("https")
                                 || asUrl.scheme() == QLatin1String("file") || asUrl.scheme() == QLatin1String("ftp"))
            ? asUrl
            : QUrl(QStringLiteral("https://") + trimmed);
        addRow(directUrl.toString(), directUrl, tr("Visit this address"));
    }

    // Search section: the literal typed text first (skipped when it's
    // already the direct-address row above — searching a provider for a
    // literal URL is redundant, not a second option), then remote
    // completions from the configured provider (once they've arrived — see
    // the suggestionsReady connection above), ahead of history/bookmarks.
    QStringList searchTexts;
    if (!looksLikeUrl)
        searchTexts << trimmed;
    if (m_remoteQuery == trimmed) {
        for (const QString &s : qAsConst(m_remoteSuggestions)) {
            if (!s.isEmpty() && !searchTexts.contains(s, Qt::CaseInsensitive))
                searchTexts << s;
        }
    }
    if (!searchTexts.isEmpty()) {
        addHeading(tr("Search %1").arg(SearchProviders::current().displayName));
        for (const QString &s : searchTexts.mid(0, kMaxSearchRows))
            addRow(s, SearchProviders::buildSearchUrl(SearchProviders::current(), s));
    }

    // History/bookmark matches — secondary, capped small so search stays
    // the dominant source of suggestions.
    const QVector<HistoryEntry> historyMatches = m_history->search(trimmed, kMaxSuggestions);
    const QVector<BookmarkEntry> allBookmarks = m_bookmarks->all();
    const QString needle = trimmed.toLower();
    QVector<BookmarkEntry> bookmarkMatches;
    for (const BookmarkEntry &b : allBookmarks) {
        if (bookmarkMatches.size() >= kMaxHistoryRows)
            break;
        if (b.url.toString().toLower().contains(needle) || b.title.toLower().contains(needle))
            bookmarkMatches << b;
    }

    if (!bookmarkMatches.isEmpty() || !historyMatches.isEmpty()) {
        addHeading(tr("From history"));
        int shown = 0;
        for (const BookmarkEntry &b : qAsConst(bookmarkMatches)) {
            if (shown >= kMaxHistoryRows)
                break;
            addRow(QStringLiteral("★ ") + (b.title.isEmpty() ? b.url.toString() : b.title), b.url,
                   b.url.toString());
            ++shown;
        }
        for (const HistoryEntry &h : historyMatches) {
            if (shown >= kMaxHistoryRows)
                break;
            addRow(h.title.isEmpty() ? h.url.toString() : h.title, h.url, h.url.toString());
            ++shown;
        }
    }

    if (m_list->count() == 0) {
        hidePopup();
        return;
    }
    // No suggestion is "current" just because the list was (re)built — only
    // an explicit arrow-key press should ever cause Enter to navigate to a
    // suggestion instead of literally whatever was typed.
    m_list->setCurrentRow(-1);
    showPopup();
}

void AddressSuggestionPopup::addHeading(const QString &text)
{
    auto *item = new QListWidgetItem(text.toUpper());
    item->setFlags(Qt::NoItemFlags);
    QFont f = item->font();
    f.setPointSizeF(f.pointSizeF() * 0.82);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    item->setFont(f);
    item->setForeground(QColor(0x80, 0x86, 0x8b));
    m_list->addItem(item);
}

void AddressSuggestionPopup::addRow(const QString &text, const QUrl &url, const QString &tooltip)
{
    auto *item = new QListWidgetItem(text);
    item->setData(Qt::UserRole, url);
    item->setToolTip(tooltip.isEmpty() ? url.toString() : tooltip);
    m_list->addItem(item);
}

void AddressSuggestionPopup::showPopup()
{
    const QPoint below = m_anchor->mapToGlobal(QPoint(0, m_anchor->height() + 4));
    int height = 8;
    for (int i = 0; i < m_list->count(); ++i)
        height += m_list->sizeHintForRow(i);
    setGeometry(below.x(), below.y(), m_anchor->width(), height);
    m_list->clearSelection();
    show();
    raise();
}

void AddressSuggestionPopup::hidePopup()
{
    hide();
}

bool AddressSuggestionPopup::isSelectableRow(int row) const
{
    if (row < 0 || row >= m_list->count())
        return false;
    return m_list->item(row)->flags() & Qt::ItemIsSelectable;
}

void AddressSuggestionPopup::moveSelection(int delta)
{
    if (m_list->count() == 0)
        return;
    int row = m_list->currentRow();
    int guard = m_list->count();
    do {
        row = (row < 0) ? (delta > 0 ? 0 : m_list->count() - 1) : (row + delta + m_list->count()) % m_list->count();
        --guard;
    } while (!isSelectableRow(row) && guard > 0);
    if (isSelectableRow(row))
        m_list->setCurrentRow(row);
}

void AddressSuggestionPopup::acceptRow(int row)
{
    if (!isSelectableRow(row))
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
    } else if (event->type() == QEvent::FocusIn) {
        // Chrome/Brave-style: suggestions appear the instant the address
        // bar is focused, even before typing anything — but only for a
        // focus a user actually caused (a click, Tab, or Ctrl+L). Clicking
        // a suggestion hides this popup (a separate top-level window),
        // which hands activation back to the main window and re-fires
        // FocusIn on whatever had focus before — the address bar — with no
        // click involved at all. Without this check, that reopened the
        // dropdown right after every navigation, permanently covering
        // whatever page had just loaded.
        const auto reason = static_cast<QFocusEvent *>(event)->reason();
        if (reason != Qt::ActiveWindowFocusReason && reason != Qt::PopupFocusReason)
            rebuild(m_addressBar->text());
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
            if (isSelectableRow(m_list->currentRow())) {
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
