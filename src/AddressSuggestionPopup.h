#pragma once

#include <QWidget>
#include <QUrl>

class QLineEdit;
class QListWidget;
class HistoryManager;
class BookmarkManager;

// Autocomplete dropdown for the address bar: shown under it while typing,
// populated from history + bookmark matches. Keyboard focus always stays on
// the QLineEdit; this widget only paints the list and reacts to clicks, with
// arrow/enter/escape handled via an event filter installed on the line edit.
class AddressSuggestionPopup : public QWidget
{
    Q_OBJECT
public:
    AddressSuggestionPopup(QLineEdit *addressBar, HistoryManager *history,
                            BookmarkManager *bookmarks, QWidget *anchor);

signals:
    void urlChosen(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateSuggestions(const QString &text);
    void showPopup();
    void hidePopup();
    void acceptRow(int row);
    void moveSelection(int delta);

    QLineEdit *m_addressBar;
    HistoryManager *m_history;
    BookmarkManager *m_bookmarks;
    QWidget *m_anchor;
    QListWidget *m_list;
};
