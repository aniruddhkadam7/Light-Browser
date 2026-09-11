#pragma once

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QWebEngineDownloadRequest;

// Edge-style downloads flyout: a Qt::Popup anchored under the toolbar
// download button (auto-closes on outside click, like a menu), listing
// completed/in-progress downloads with per-row "Open file"/folder/delete
// actions and a "See more" footer that opens the downloads folder.
class DownloadManager : public QWidget
{
    Q_OBJECT
public:
    explicit DownloadManager(QWidget *parent = nullptr);

    void showBelow(QWidget *anchor);

signals:
    // Emitted when a new download begins, so BrowserWindow can light up the
    // toolbar button's notification dot the way Edge does.
    void downloadStarted();
    // Emitted whenever this popup closes, for any reason — an explicit
    // hide() call or (being a Qt::Popup) Qt auto-closing it on an outside
    // click. BrowserWindow needs the latter case specifically: clicking the
    // toolbar button again while this is open lands that click "outside",
    // which closes the popup a moment before the button's own click handler
    // runs — without knowing a close *just* happened, that handler would
    // treat it as "not open" and immediately reopen it.
    void closed();

public slots:
    void handleDownload(QWebEngineDownloadRequest *download);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    QString uniqueFilePath(const QString &dir, const QString &fileName) const;
    void addRow(const QString &filePath, QWebEngineDownloadRequest *download);
    void openDownloadsFolder() const;

    QVBoxLayout *m_listLayout;
    QLabel *m_emptyLabel;
    QString m_downloadsDir;
};
