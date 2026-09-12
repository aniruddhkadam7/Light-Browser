#pragma once

#include <QMainWindow>
#include <QSet>
#include <QWebEnginePage>
#include <QUrl>

class QTabBar;
class QStackedWidget;
class QLineEdit;
class QToolButton;
class QLabel;
class QWebEngineView;
class QWebEngineProfile;
class QNetworkAccessManager;
class QTimer;
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
    // incognito is purely cosmetic here: title/badge only. It's the exact
    // same profile, history, cookies and everything else as a regular
    // window — nothing is actually isolated or left unrecorded.
    explicit BrowserWindow(QWidget *parent = nullptr, bool incognito = false);
    ~BrowserWindow() override;

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
    // An empty/invalid url (the default) opens the local New Tab page
    // instead of navigating anywhere.
    QWebEngineView *addNewTab(const QUrl &url = QUrl(), bool focusAddressBar = true);
    // Loads the local, static New Tab page into view — matching Most
    // Visited tiles from local history, backfilling any missing site icons
    // in the background (see fetchMissingFavicons()).
    void showNewTabPage(QWebEngineView *view);
    // One tiny favicon.ico request per site missing a cached icon (not a
    // full page load) so Most Visited tiles show real site logos, Chrome/
    // Brave-style, instead of staying on the letter-avatar fallback forever.
    // Skipped entirely for sites already cached.
    void fetchMissingFavicons();
    void refreshOpenNewTabPages();
    // Lazily-created, shared by favicon backfill and by each tab's omnibox
    // suggestion bridge — a plain HTTP client, nothing WebEngine-specific.
    QNetworkAccessManager *networkManager();
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
    // Opens another regular BrowserWindow, just labeled "Incognito" — see
    // the constructor comment for what that does and doesn't mean.
    void openIncognitoWindow();
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

    QNetworkAccessManager *m_networkManager = nullptr;
    QSet<QString> m_faviconFetchesInFlight; // registrable domains, to dedupe
    QSet<QWebEngineView *> m_newTabViews; // views currently on the New Tab page

    // Drives the animated tab-loading spinner (see iconSpinner() in
    // BrowserWindow.cpp): only runs while at least one tab is loading, and
    // stops itself once none are, rather than ticking constantly.
    QTimer *m_spinnerTimer = nullptr;
    int m_spinnerAngle = 0;

    const bool m_incognito;
    QToolButton *m_incognitoBadge = nullptr;
    // Regular windows use the shared QWebEngineProfile::defaultProfile();
    // Incognito windows get their own off-the-record instance instead. Not
    // for isolation (this incognito mode is still cosmetic-only) but for
    // stability: Qt WebEngine does not cleanly support two independent
    // top-level windows tearing down QWebEngineView/Page children that
    // share one live profile object — destroying one window's pages while
    // the other's are still active was crashing the whole process. A
    // separate profile per window sidesteps that entirely.
    QWebEngineProfile *m_profile = nullptr;
};
