#pragma once

#include <QWidget>
#include <QUrl>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QTimer;
class QNetworkAccessManager;
class HistoryManager;
class BookmarkManager;
class OmniboxBridge;

// Chrome/Brave-style autocomplete dropdown for the address bar: opens the
// instant it's focused — even with nothing typed yet, showing local
// "Frequently visited" history — and on every keystroke shows the
// configured search provider's suggestions first, then matching
// history/bookmarks second. Keyboard focus always stays on the QLineEdit;
// this widget only paints the list and reacts to clicks, with
// arrow/enter/escape handled via an event filter installed on the line edit.
class AddressSuggestionPopup : public QWidget
{
    Q_OBJECT
public:
    AddressSuggestionPopup(QLineEdit *addressBar, HistoryManager *history, BookmarkManager *bookmarks,
                            QNetworkAccessManager *networkManager, QWidget *anchor);

    // Forces the popup closed regardless of focus state. Call this whenever
    // the address bar's text is set programmatically (i.e. a real
    // navigation happened, not the user typing) — textEdited/FocusOut alone
    // don't cover that case, which otherwise left a stale suggestion list
    // (from whatever was typed before navigating) floating over the address
    // bar's new, unrelated URL.
    void closeNow();

signals:
    void urlChosen(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // Rebuilds the whole list for the address bar's current text: the
    // empty-state "Frequently visited" list when blank, otherwise a direct-
    // URL row (if it looks like one), the search section (literal text plus
    // whatever remote completions have arrived so far for this exact text),
    // then history/bookmark matches.
    void rebuild(const QString &text);
    void addHeading(const QString &text);
    void addRow(const QString &text, const QUrl &url, const QString &tooltip = QString());
    void showPopup();
    void hidePopup();
    void acceptRow(int row);
    void moveSelection(int delta);
    bool isSelectableRow(int row) const;

    QLineEdit *m_addressBar;
    HistoryManager *m_history;
    BookmarkManager *m_bookmarks;
    QWidget *m_anchor;
    QListWidget *m_list;

    OmniboxBridge *m_omniboxBridge;
    QTimer *m_debounceTimer;
    QString m_remoteQuery; // text the current m_remoteSuggestions answer
    QStringList m_remoteSuggestions;
};
