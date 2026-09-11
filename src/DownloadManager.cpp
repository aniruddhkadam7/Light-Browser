#include "DownloadManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEngineDownloadRequest>

#include <functional>

namespace {

const QColor kIconColor(0xc8, 0xc8, 0xc9);

QPen linePen(const QColor &color, qreal width = 1.4)
{
    return QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

QIcon paintIcon(int size, const std::function<void(QPainter &, qreal)> &draw)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter, static_cast<qreal>(size));
    painter.end();
    return QIcon(pixmap);
}

QIcon iconFolder(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.2));
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(s * 0.12, s * 0.28);
        path.lineTo(s * 0.4, s * 0.28);
        path.lineTo(s * 0.48, s * 0.4);
        path.lineTo(s * 0.88, s * 0.4);
        path.lineTo(s * 0.88, s * 0.8);
        path.lineTo(s * 0.12, s * 0.8);
        path.closeSubpath();
        p.drawPath(path);
    });
}

QIcon iconSearch(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(s * 0.14, s * 0.14, s * 0.52, s * 0.52));
        p.drawLine(QPointF(s * 0.62, s * 0.62), QPointF(s * 0.88, s * 0.88));
    });
}

QIcon iconDots(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal r = s * 0.07;
        for (qreal y : {s * 0.22, s * 0.5, s * 0.78})
            p.drawEllipse(QPointF(s * 0.5, y), r, r);
    });
}

QIcon iconTrash(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(s * 0.24, s * 0.32, s * 0.52, s * 0.54));
        p.drawLine(QPointF(s * 0.14, s * 0.32), QPointF(s * 0.86, s * 0.32));
        p.drawLine(QPointF(s * 0.38, s * 0.32), QPointF(s * 0.42, s * 0.16));
        p.drawLine(QPointF(s * 0.42, s * 0.16), QPointF(s * 0.58, s * 0.16));
        p.drawLine(QPointF(s * 0.58, s * 0.16), QPointF(s * 0.62, s * 0.32));
    });
}

QIcon iconFile(const QColor &fg, const QColor &bg)
{
    return paintIcon(36, [fg, bg](QPainter &p, qreal s) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(0, 0, s, s), s * 0.16, s * 0.16);
        p.setPen(linePen(fg, 1.4));
        p.setBrush(Qt::NoBrush);
        QPainterPath doc;
        const qreal m = s * 0.28, w = s * 0.44, h = s * 0.5, fold = s * 0.12;
        doc.moveTo(m, s * 0.25);
        doc.lineTo(m + w - fold, s * 0.25);
        doc.lineTo(m + w, s * 0.25 + fold);
        doc.lineTo(m + w, s * 0.25 + h);
        doc.lineTo(m, s * 0.25 + h);
        doc.closeSubpath();
        p.drawPath(doc);
        p.drawLine(QPointF(m + w - fold, s * 0.25), QPointF(m + w - fold, s * 0.25 + fold));
        p.drawLine(QPointF(m + w - fold, s * 0.25 + fold), QPointF(m + w, s * 0.25 + fold));
    });
}

// A clickable, underline-on-hover blue link label, matching Edge's
// "Open file" row action (styled text rather than a button).
class LinkLabel : public QLabel
{
public:
    explicit LinkLabel(const QString &text, QWidget *parent = nullptr) : QLabel(text, parent)
    {
        setObjectName("downloadOpenLink");
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && clicked)
            clicked();
        QLabel::mouseReleaseEvent(event);
    }

public:
    // A plain callback instead of a Qt signal: this small helper class isn't
    // moc'd, so it can't declare one of its own.
    std::function<void()> clicked;
};

} // namespace

DownloadManager::DownloadManager(QWidget *parent)
    : QWidget(parent), m_emptyLabel(nullptr)
{
    // Popup: behaves like a menu flyout — shows without stealing the whole
    // app's focus but closes automatically on an outside click, matching how
    // Edge's downloads button behaves rather than being a separate window
    // the user has to remember to close.
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setObjectName("downloadsPopup");
    setFixedWidth(360);

    m_downloadsDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *header = new QWidget(this);
    header->setObjectName("downloadsHeader");
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 14, 10, 10);

    auto *title = new QLabel(tr("Downloads"), header);
    title->setObjectName("downloadsTitle");

    auto *folderBtn = new QToolButton(header);
    folderBtn->setObjectName("downloadsHeaderButton");
    folderBtn->setIcon(iconFolder(kIconColor));
    folderBtn->setToolTip(tr("Open downloads folder"));
    connect(folderBtn, &QToolButton::clicked, this, &DownloadManager::openDownloadsFolder);

    auto *searchBtn = new QToolButton(header);
    searchBtn->setObjectName("downloadsHeaderButton");
    searchBtn->setIcon(iconSearch(kIconColor));
    searchBtn->setToolTip(tr("Search downloads"));

    auto *searchEdit = new QLineEdit(header);
    searchEdit->setObjectName("downloadsSearchEdit");
    searchEdit->setPlaceholderText(tr("Search downloads"));
    searchEdit->hide();

    auto *dotsBtn = new QToolButton(header);
    dotsBtn->setObjectName("downloadsHeaderButton");
    dotsBtn->setIcon(iconDots(kIconColor));
    dotsBtn->setToolTip(tr("More options"));
    connect(dotsBtn, &QToolButton::clicked, this, [this, dotsBtn] {
        QMenu menu(this);
        menu.setObjectName("chromeMenu");
        menu.addAction(tr("Open downloads folder"), this, &DownloadManager::openDownloadsFolder);
        menu.addAction(tr("Clear list"), this, [this] {
            for (int i = m_listLayout->count() - 1; i >= 0; --i) {
                QWidget *w = m_listLayout->itemAt(i)->widget();
                if (w && w->objectName() == QLatin1String("downloadRow")) {
                    delete m_listLayout->takeAt(i);
                    delete w;
                }
            }
            if (m_emptyLabel)
                m_emptyLabel->setVisible(true);
            if (isVisible())
                adjustSize();
        });
        menu.exec(dotsBtn->mapToGlobal(QPoint(0, dotsBtn->height())));
    });

    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(searchEdit, 1);
    headerLayout->addWidget(folderBtn);
    headerLayout->addWidget(searchBtn);
    headerLayout->addWidget(dotsBtn);

    connect(searchBtn, &QToolButton::clicked, this, [searchEdit] {
        searchEdit->setVisible(!searchEdit->isVisible());
        if (searchEdit->isVisible())
            searchEdit->setFocus();
        else
            searchEdit->clear();
    });
    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_listLayout->count(); ++i) {
            QWidget *row = m_listLayout->itemAt(i)->widget();
            if (!row || row->objectName() != QLatin1String("downloadRow"))
                continue;
            const QString name = row->property("fileName").toString();
            row->setVisible(text.isEmpty() || name.contains(text, Qt::CaseInsensitive));
        }
    });

    auto *scroll = new QScrollArea(this);
    scroll->setObjectName("downloadsScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    // Cap growth once there are enough downloads to actually need
    // scrolling; below that, the popup shrinks to fit the rows it has
    // instead of leaving a big empty area like a fixed-height panel would.
    scroll->setMaximumHeight(360);

    auto *listHost = new QWidget(scroll);
    m_listLayout = new QVBoxLayout(listHost);
    m_listLayout->setContentsMargins(6, 4, 6, 4);
    m_listLayout->setSpacing(2);

    m_emptyLabel = new QLabel(tr("No downloads yet"), listHost);
    m_emptyLabel->setObjectName("downloadsEmpty");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_listLayout->addWidget(m_emptyLabel);
    // Without this, QScrollArea (setWidgetResizable) stretches listHost to
    // fill the popup's full height even when there's only one short row,
    // and QVBoxLayout then distributes that leftover space by spreading
    // rows (and each row's own icon/name/link) apart instead of collecting
    // it below everything, like a real Edge/Chrome downloads list would.
    m_listLayout->addStretch();

    scroll->setWidget(listHost);

    auto *seeMore = new QPushButton(tr("See more"), this);
    seeMore->setObjectName("downloadsSeeMore");
    connect(seeMore, &QPushButton::clicked, this, &DownloadManager::openDownloadsFolder);

    outer->addWidget(header);
    outer->addWidget(scroll, 1);
    outer->addWidget(seeMore);

    setStyleSheet(R"(
        QWidget#downloadsPopup {
            background: #2b2b2c;
            border: 1px solid #3a3a3c;
            border-radius: 10px;
        }
        QWidget#downloadsHeader {
            background: transparent;
        }
        QLabel#downloadsTitle {
            color: #f2f2f2;
            font-size: 15px;
            font-weight: 600;
        }
        QToolButton#downloadsHeaderButton {
            background: transparent;
            border: none;
            border-radius: 6px;
            padding: 5px;
        }
        QToolButton#downloadsHeaderButton:hover {
            background: #3a3a3c;
        }
        QLineEdit#downloadsSearchEdit {
            background: #202021;
            border: 1px solid #3a3a3c;
            border-radius: 6px;
            padding: 3px 8px;
            color: #e3e3e3;
        }
        QScrollArea#downloadsScroll, QScrollArea#downloadsScroll > QWidget > QWidget {
            background: transparent;
        }
        QLabel#downloadsEmpty {
            color: #9c9c9c;
            font-size: 12px;
            padding: 30px 0;
        }
        QWidget#downloadRow {
            background: transparent;
            border-radius: 8px;
        }
        QWidget#downloadRow:hover {
            background: #353536;
        }
        QLabel#downloadName {
            color: #f2f2f2;
            font-size: 13px;
        }
        QLabel#downloadOpenLink {
            color: #6cb4ff;
            font-size: 12px;
        }
        QLabel#downloadOpenLink:hover {
            text-decoration: underline;
        }
        QLabel#downloadStatus {
            color: #9c9c9c;
            font-size: 12px;
        }
        QToolButton#downloadRowButton {
            background: transparent;
            border: none;
            border-radius: 6px;
            padding: 4px;
        }
        QToolButton#downloadRowButton:hover {
            background: #444445;
        }
        QProgressBar#downloadProgress {
            background: #1b1b1c;
            border: none;
            border-radius: 3px;
            max-height: 4px;
        }
        QProgressBar#downloadProgress::chunk {
            background: #3a8bd8;
            border-radius: 3px;
        }
        QPushButton#downloadsSeeMore {
            background: transparent;
            border: none;
            border-top: 1px solid #3a3a3c;
            color: #6cb4ff;
            padding: 10px;
            font-size: 13px;
        }
        QPushButton#downloadsSeeMore:hover {
            background: #353536;
        }
        QMenu#chromeMenu {
            background: #2b2b2c;
            border: 1px solid #3a3a3c;
            color: #e3e3e3;
            padding: 4px;
        }
        QMenu#chromeMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu#chromeMenu::item:selected {
            background: #3a6ea5;
        }
    )");
}

void DownloadManager::showBelow(QWidget *anchor)
{
    adjustSize();
    const QPoint topRight = anchor->mapToGlobal(QPoint(anchor->width(), anchor->height() + 6));
    move(topRight.x() - width(), topRight.y());
    show();
    raise();
}

void DownloadManager::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit closed();
}

void DownloadManager::openDownloadsFolder() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadsDir));
}

QString DownloadManager::uniqueFilePath(const QString &dir, const QString &fileName) const
{
    QFileInfo fi(fileName);
    const QString base = fi.completeBaseName();
    const QString ext = fi.suffix();

    QString candidate = fileName;
    int n = 1;
    while (QFile::exists(dir + "/" + candidate)) {
        candidate = ext.isEmpty() ? QString("%1 (%2)").arg(base).arg(n)
                                   : QString("%1 (%2).%3").arg(base).arg(n).arg(ext);
        ++n;
    }
    return dir + "/" + candidate;
}

void DownloadManager::handleDownload(QWebEngineDownloadRequest *download)
{
    QDir().mkpath(m_downloadsDir);

    const QString uniquePath = uniqueFilePath(m_downloadsDir, download->downloadFileName());
    const QFileInfo fi(uniquePath);

    download->setDownloadDirectory(fi.absolutePath());
    download->setDownloadFileName(fi.fileName());
    download->accept();

    addRow(uniquePath, download);
    emit downloadStarted();
}

void DownloadManager::addRow(const QString &filePath, QWebEngineDownloadRequest *download)
{
    if (m_emptyLabel)
        m_emptyLabel->setVisible(false);

    const QFileInfo fi(filePath);

    auto *row = new QWidget(this);
    row->setObjectName("downloadRow");
    row->setProperty("fileName", fi.fileName());
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(8, 8, 8, 8);
    rowLayout->setSpacing(10);

    // The real per-file-type icon (installer, PDF, image, archive, ...) from
    // the OS shell, same as Explorer/Edge show, instead of one generic
    // document glyph for every file.
    static QFileIconProvider iconProvider;
    QIcon shellIcon = iconProvider.icon(fi);
    if (shellIcon.isNull())
        shellIcon = iconFile(QColor(0xf2, 0xf2, 0xf2), QColor(0x3a, 0x6e, 0xa5));

    auto *iconLabel = new QLabel(row);
    iconLabel->setPixmap(shellIcon.pixmap(32, 32));
    iconLabel->setFixedSize(36, 36);
    iconLabel->setAlignment(Qt::AlignCenter);

    auto *textCol = new QVBoxLayout();
    textCol->setSpacing(2);

    auto *nameLabel = new QLabel(fi.fileName(), row);
    nameLabel->setObjectName("downloadName");
    nameLabel->setWordWrap(true);

    auto *progress = new QProgressBar(row);
    progress->setObjectName("downloadProgress");
    progress->setRange(0, 100);
    progress->setTextVisible(false);

    auto *openLink = new LinkLabel(tr("Open file"), row);
    openLink->hide();
    openLink->clicked = [filePath] { QDesktopServices::openUrl(QUrl::fromLocalFile(filePath)); };

    auto *statusLabel = new QLabel(row);
    statusLabel->setObjectName("downloadStatus");
    statusLabel->hide();

    textCol->addWidget(nameLabel);
    textCol->addWidget(progress);
    textCol->addWidget(openLink);
    textCol->addWidget(statusLabel);

    auto *folderButton = new QToolButton(row);
    folderButton->setObjectName("downloadRowButton");
    folderButton->setIcon(iconFolder(kIconColor));
    folderButton->setToolTip(tr("Open downloads folder"));
    connect(folderButton, &QToolButton::clicked, this,
            [this] { openDownloadsFolder(); });

    auto *trashButton = new QToolButton(row);
    trashButton->setObjectName("downloadRowButton");
    trashButton->setIcon(iconTrash(kIconColor));
    trashButton->setToolTip(tr("Delete"));
    connect(trashButton, &QToolButton::clicked, this, [row, filePath, this] {
        QFile::remove(filePath);
        m_listLayout->removeWidget(row);
        row->deleteLater();
        bool anyRowLeft = false;
        for (int i = 0; i < m_listLayout->count(); ++i) {
            QWidget *w = m_listLayout->itemAt(i)->widget();
            if (w && w->objectName() == QLatin1String("downloadRow")) {
                anyRowLeft = true;
                break;
            }
        }
        if (!anyRowLeft && m_emptyLabel)
            m_emptyLabel->setVisible(true);
        if (isVisible())
            adjustSize();
    });

    rowLayout->addWidget(iconLabel);
    rowLayout->addLayout(textCol, 1);
    rowLayout->addWidget(folderButton);
    rowLayout->addWidget(trashButton);

    m_listLayout->insertWidget(m_listLayout->count() - 1, row);
    if (isVisible())
        adjustSize();

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, row, [download, progress] {
        const qint64 total = download->totalBytes();
        if (total > 0)
            progress->setValue(static_cast<int>(download->receivedBytes() * 100 / total));
    });

    connect(download, &QWebEngineDownloadRequest::stateChanged, row,
            [progress, openLink, statusLabel](QWebEngineDownloadRequest::DownloadState state) {
        switch (state) {
        case QWebEngineDownloadRequest::DownloadCompleted:
            progress->hide();
            openLink->show();
            break;
        case QWebEngineDownloadRequest::DownloadCancelled:
            progress->hide();
            statusLabel->setText(DownloadManager::tr("Cancelled"));
            statusLabel->show();
            break;
        case QWebEngineDownloadRequest::DownloadInterrupted:
            progress->hide();
            statusLabel->setText(DownloadManager::tr("Failed"));
            statusLabel->show();
            break;
        default:
            break;
        }
    });
}
