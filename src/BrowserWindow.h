#pragma once

#include <QMainWindow>
#include <QWebEnginePage>
#include <QUrl>

class QTabBar;
class QStackedWidget;
class QLineEdit;
class QToolButton;
class QLabel;
class QWebEngineView;
class DownloadManager;
class HistoryManager;
class BookmarkManager;
class AddressSuggestionPopup;
class UrlRedirectManager;
class ActualUrlBar;

class BrowserWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit BrowserWindow(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void closeTab(int index);
    void currentTabChanged(int index);
    void navigateToAddress();
    void toggleReloadStop();

private:
    QWebEngineView *currentView() const;
    QWebEngineView *createTabView();
    QWebEngineView *addNewTab(const QUrl &url, bool focusAddressBar = true);
    QWebEnginePage *handleNewWindowRequest(QWebEnginePage::WebWindowType type);
    // Routes through UrlRedirectManager: if `url` matches a mapping rule's
    // source pattern, actually navigates to the mapped target instead.
    void navigateViewTo(QWebEngineView *view, const QUrl &url);

    QWidget *buildTabStrip();
    QWidget *buildToolbar();
    void setupShortcuts();
    void setupFindBar();
    void applyEdgeTheme();
    void updateMaximizeButtonIcon();
    void attachTabCloseButton(int index);
    void updateFavoriteButtonIcon();
    void toggleFavoriteForCurrentTab();
    void showExtensionsMenu();
    void showProfileMenu();
    // The real local Windows account name, or a user-chosen override
    // persisted via QSettings — replaces the previous hardcoded "A".
    QString profileDisplayName() const;
    void setProfileDisplayName(const QString &name);
    void promptRenameProfile();
    void updateProfileAvatar();
    void showMainMenu();
    void showHistoryPage();
    void showUrlMappingSettings();
    void toggleDownloadsPopup();
    // Clicking a toolbar toggle button while its own popup is open lands
    // that click "outside" the popup, which dismisses it a moment before the
    // button's own clicked()/mouse-release handler fires — so without this
    // check, the same click that should close a popup instead closes it and
    // immediately reopens it. Called right before showing any of the popups
    // above; true means "this click was actually the one dismissing it,
    // don't reopen." Each popup records its own close time via
    // recordPopupClosed() (from QMenu::exec() returning, or DownloadManager
    // hiding).
    bool popupJustClosed() const;
    void recordPopupClosed();
    // What should be shown/recorded as "the URL" for this tab: whatever was
    // last explicitly requested via navigateViewTo, falling back to a
    // mapping-rule reverse-match, then to the tab's real current URL.
    QUrl displayUrlFor(QWebEngineView *view) const;

    void updateNavigationActions();
    void updateUrlBar(const QUrl &url);
    void updateReloadStopAction();
    void updateWindowTitle(const QString &pageTitle);

    QUrl resolveInput(const QString &text) const;

    QWidget *m_outerFrame = nullptr;
    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QToolButton *m_newTabButton = nullptr;

    QToolButton *m_minButton = nullptr;
    QToolButton *m_maxButton = nullptr;
    QToolButton *m_closeButton = nullptr;

    QLineEdit *m_addressBar = nullptr;
    QLabel *m_lockLabel = nullptr;
    QToolButton *m_dualUrlToggle = nullptr;
    ActualUrlBar *m_actualUrlBar = nullptr;
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
    QToolButton *m_reloadStopButton = nullptr;
    QToolButton *m_favoriteButton = nullptr;
    QToolButton *m_downloadsButton = nullptr;
    QToolButton *m_extensionsButton = nullptr;
    QToolButton *m_menuButton = nullptr;
    QLabel *m_profileAvatar = nullptr;

    DownloadManager *m_downloadManager = nullptr;
    HistoryManager *m_history = nullptr;
    BookmarkManager *m_bookmarks = nullptr;
    AddressSuggestionPopup *m_suggestions = nullptr;
    UrlRedirectManager *m_redirectManager = nullptr;

    QWidget *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;

    qint64 m_lastPopupCloseMs = 0;
};
