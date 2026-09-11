#include "ActualUrlBar.h"
#include "AddressSuggestionPopup.h"
#include "BookmarkManager.h"
#include "BrowserWindow.h"
#include "DownloadManager.h"
#include "HistoryManager.h"
#include "UrlMappingSettingsDialog.h"
#include "TabView.h"
#include "UrlRedirectManager.h"
#include "WebPage.h"
#include "WebView.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProxyStyle>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QDateTime>
#include <QStandardPaths>
#include <QStyleOptionTab>
#include <QTabBar>
#include <QToolButton>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWindow>
#include <QtMath>
#include <functional>

namespace {
const QUrl kHomeUrl("https://www.google.com");

// Google's own <title> for a search results page is "<query> - Google
// Search"; the tab should instead show just "<query> - Search" — this is
// the one deliberate override of the page's real title, tab title only.
QString googleSearchTabTitle(const QUrl &url)
{
    if (!url.host().contains(QLatin1String("google.")) || url.path() != QLatin1String("/search"))
        return QString();
    QString query = QUrlQuery(url).queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded);
    query.replace(QLatin1Char('+'), QLatin1Char(' '));
    query = query.trimmed();
    return query.isEmpty() ? QString() : query + QStringLiteral(" - Search");
}

// Hand-drawn monoline icons instead of emoji glyphs: emoji render in full
// color via the system emoji font (a colorful lock/puzzle piece looks out of
// place on a flat dark toolbar), and their exact glyph shape isn't ours to
// control across systems. Painting them ourselves keeps every icon a single
// flat color and pixel-crisp at the sizes we use.
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

QPen linePen(const QColor &color, qreal width = 1.5)
{
    return QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

QIcon iconChevronLeft(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c));
        QPolygonF poly{QPointF(s * 0.62, s * 0.22), QPointF(s * 0.34, s * 0.5), QPointF(s * 0.62, s * 0.78)};
        p.drawPolyline(poly);
    });
}

QIcon iconChevronRight(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c));
        QPolygonF poly{QPointF(s * 0.38, s * 0.22), QPointF(s * 0.66, s * 0.5), QPointF(s * 0.38, s * 0.78)};
        p.drawPolyline(poly);
    });
}

QIcon iconReload(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c));
        p.setBrush(Qt::NoBrush);
        const QRectF rect(s * 0.22, s * 0.22, s * 0.56, s * 0.56);
        p.drawArc(rect, 20 * 16, 300 * 16);

        const qreal angleDeg = 320.0;
        const qreal angleRad = qDegreesToRadians(angleDeg);
        const QPointF center = rect.center();
        const qreal r = rect.width() / 2.0;
        const QPointF tip(center.x() + r * qCos(angleRad), center.y() - r * qSin(angleRad));
        QPolygonF arrow{tip + QPointF(-s * 0.16, -s * 0.02), tip + QPointF(s * 0.03, s * 0.10),
                        tip + QPointF(-s * 0.06, s * 0.18)};
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(arrow);
    });
}

QIcon iconTabClose(const QColor &c)
{
    return paintIcon(12, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.1));
        p.drawLine(QPointF(s * 0.22, s * 0.22), QPointF(s * 0.78, s * 0.78));
        p.drawLine(QPointF(s * 0.78, s * 0.22), QPointF(s * 0.22, s * 0.78));
    });
}

QIcon iconStop(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c));
        p.drawLine(QPointF(s * 0.28, s * 0.28), QPointF(s * 0.72, s * 0.72));
        p.drawLine(QPointF(s * 0.72, s * 0.28), QPointF(s * 0.28, s * 0.72));
    });
}

QIcon iconLock(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.3));
        p.setBrush(Qt::NoBrush);
        const QRectF shackle(s * 0.28, s * 0.06, s * 0.44, s * 0.5);
        p.drawArc(shackle, 0, 180 * 16);

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const QRectF body(s * 0.18, s * 0.44, s * 0.64, s * 0.46);
        p.drawRoundedRect(body, 2, 2);
    });
}

QPolygonF starPolygon(qreal s)
{
    QPolygonF star;
    const qreal cx = s / 2.0, cy = s / 2.0;
    const qreal rOuter = s * 0.42, rInner = s * 0.18;
    for (int i = 0; i < 10; ++i) {
        const qreal angle = -M_PI / 2 + i * M_PI / 5;
        const qreal r = (i % 2 == 0) ? rOuter : rInner;
        star << QPointF(cx + r * qCos(angle), cy + r * qSin(angle));
    }
    return star;
}

QIcon iconStar(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(starPolygon(s));
    });
}

QIcon iconStarFilled(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(starPolygon(s));
    });
}

QIcon iconPuzzle(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.3));
        p.setBrush(Qt::NoBrush);
        const qreal m = s * 0.22, w = s * 0.56;
        p.drawRoundedRect(QRectF(m, m, w, w), 2, 2);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(s * 0.5, m), s * 0.09, s * 0.09);
    });
}

QIcon iconMenuDots(const QColor &c)
{
    return paintIcon(18, [c](QPainter &p, qreal s) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        const qreal r = s * 0.06;
        const qreal y = s * 0.5;
        for (qreal x : {s * 0.28, s * 0.5, s * 0.72})
            p.drawEllipse(QPointF(x, y), r, r);
    });
}

// The whole avatar badge — filled circle plus a generic person glyph —
// painted as one pixmap rather than relying on a QSS background behind a
// separate icon, since the two didn't reliably composite the same way on
// every setup.
QIcon iconProfileAvatar(const QColor &bg, const QColor &fg)
{
    return paintIcon(24, [bg, fg](QPainter &p, qreal s) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawEllipse(QRectF(0, 0, s, s));
        p.setBrush(fg);
        p.drawEllipse(QPointF(s * 0.5, s * 0.38), s * 0.16, s * 0.16);
        QPainterPath shoulders;
        shoulders.moveTo(s * 0.22, s * 0.88);
        shoulders.cubicTo(s * 0.22, s * 0.62, s * 0.78, s * 0.62, s * 0.78, s * 0.88);
        p.drawPath(shoulders);
    });
}

// Toolbar download glyph (tray + down arrow), with an optional small red dot
// in the corner mirroring Edge's "new download" notification indicator.
QIcon iconDownload(const QColor &c, bool showDot = false)
{
    return paintIcon(18, [c, showDot](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(s * 0.5, s * 0.12), QPointF(s * 0.5, s * 0.56));
        p.drawLine(QPointF(s * 0.3, s * 0.38), QPointF(s * 0.5, s * 0.58));
        p.drawLine(QPointF(s * 0.7, s * 0.38), QPointF(s * 0.5, s * 0.58));
        QPainterPath tray;
        tray.moveTo(s * 0.18, s * 0.68);
        tray.lineTo(s * 0.18, s * 0.84);
        tray.lineTo(s * 0.82, s * 0.84);
        tray.lineTo(s * 0.82, s * 0.68);
        p.drawPath(tray);
        if (showDot) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xe8, 0x3b, 0x3b));
            p.drawEllipse(QPointF(s * 0.82, s * 0.18), s * 0.13, s * 0.13);
        }
    });
}

// Placeholder icon for a tab with no page loaded yet (before urlChanged /
// iconChanged deliver a real favicon), matching Edge's blank-window glyph on
// a fresh "New tab" instead of leaving the tab textless.
QIcon iconNewTabPage(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.2));
        p.setBrush(Qt::NoBrush);
        const QRectF rect(s * 0.14, s * 0.2, s * 0.72, s * 0.62);
        p.drawRoundedRect(rect, 2, 2);
        p.drawLine(QPointF(rect.left(), rect.top() + rect.height() * 0.32),
                   QPointF(rect.right(), rect.top() + rect.height() * 0.32));
    });
}

// Generic globe favicon for a loaded page that has no favicon of its own,
// matching Edge's fallback (rather than leaving the tab with a blank icon
// slot once a real page has loaded).
QIcon iconGlobe(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.1));
        p.setBrush(Qt::NoBrush);
        const QRectF circle(s * 0.1, s * 0.1, s * 0.8, s * 0.8);
        p.drawEllipse(circle);
        p.drawLine(QPointF(circle.left(), circle.center().y()), QPointF(circle.right(), circle.center().y()));
        p.drawEllipse(circle.center(), circle.width() * 0.22, circle.height() * 0.5);
    });
}

// Tab icon for a Google search results page — a magnifying glass, matching
// the "<query> - Search" title override, instead of Google's own favicon.
QIcon iconSearchGlyph(const QColor &c)
{
    return paintIcon(16, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(s * 0.14, s * 0.14, s * 0.5, s * 0.5));
        p.drawLine(QPointF(s * 0.58, s * 0.58), QPointF(s * 0.86, s * 0.86));
    });
}

// The one deliberate departure from this file's flat-monoline-icon rule:
// the address bar shows the real (simplified) Google "G" mark in place of
// the lock icon on a blank/new tab, matching Edge's own default — a generic
// line icon wouldn't read as "this searches Google" the way the real mark
// does.
QIcon iconGoogleG()
{
    return paintIcon(16, [](QPainter &p, qreal s) {
        const QRectF outer(s * 0.05, s * 0.05, s * 0.9, s * 0.9);
        const qreal thickness = s * 0.24;
        const QRectF inner = outer.adjusted(thickness, thickness, -thickness, -thickness);
        QPainterPath ring;
        ring.addEllipse(outer);
        QPainterPath hole;
        hole.addEllipse(inner);
        ring = ring.subtracted(hole);

        p.save();
        p.setClipPath(ring);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xea, 0x43, 0x35));
        p.drawPie(outer, 45 * 16, 90 * 16);
        p.setBrush(QColor(0xfb, 0xbc, 0x05));
        p.drawPie(outer, 135 * 16, 90 * 16);
        p.setBrush(QColor(0x34, 0xa8, 0x53));
        p.drawPie(outer, 225 * 16, 90 * 16);
        p.setBrush(QColor(0x42, 0x85, 0xf4));
        p.drawPie(outer, 315 * 16, 90 * 16);
        p.restore();

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x42, 0x85, 0xf4));
        p.drawRect(QRectF(outer.center().x() - thickness * 0.15, outer.center().y() - thickness / 2,
                           outer.right() - (outer.center().x() - thickness * 0.15), thickness));
    });
}

QIcon iconMinimizeGlyph(const QColor &c)
{
    return paintIcon(14, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.2));
        p.drawLine(QPointF(s * 0.18, s * 0.7), QPointF(s * 0.82, s * 0.7));
    });
}

QIcon iconMaximizeGlyph(const QColor &c)
{
    return paintIcon(14, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(s * 0.22, s * 0.22, s * 0.56, s * 0.56));
    });
}

QIcon iconRestoreGlyph(const QColor &c)
{
    return paintIcon(14, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(s * 0.16, s * 0.30, s * 0.5, s * 0.5));
        p.drawRect(QRectF(s * 0.34, s * 0.16, s * 0.5, s * 0.5));
    });
}

QIcon iconCloseGlyph(const QColor &c)
{
    return paintIcon(14, [c](QPainter &p, qreal s) {
        p.setPen(linePen(c, 1.2));
        p.drawLine(QPointF(s * 0.2, s * 0.2), QPointF(s * 0.8, s * 0.8));
        p.drawLine(QPointF(s * 0.8, s * 0.2), QPointF(s * 0.2, s * 0.8));
    });
}

const QColor kIconColor(0xe3, 0xe3, 0xe3);
const QColor kMutedIconColor(0xc8, 0xc8, 0xc9);
constexpr int kResizeMargin = 6;

// The frameless window has no OS-drawn border, so this thin wrapper around the
// real content detects presses in its margin and hands them to the OS's own
// resize loop (QWindow::startSystemResize), giving native resize + snapping
// without a native title bar.
class ResizeFrame : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            const Qt::Edges edges = edgesAt(event->pos());
            if (edges != Qt::Edges() ) {
                if (QWindow *wh = window()->windowHandle()) {
                    wh->startSystemResize(edges);
                    event->accept();
                    return;
                }
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!window()->isMaximized()) {
            switch (edgesAt(event->pos())) {
            case Qt::TopEdge:
            case Qt::BottomEdge:
                setCursor(Qt::SizeVerCursor);
                break;
            case Qt::LeftEdge:
            case Qt::RightEdge:
                setCursor(Qt::SizeHorCursor);
                break;
            case Qt::TopEdge | Qt::LeftEdge:
            case Qt::BottomEdge | Qt::RightEdge:
                setCursor(Qt::SizeFDiagCursor);
                break;
            case Qt::TopEdge | Qt::RightEdge:
            case Qt::BottomEdge | Qt::LeftEdge:
                setCursor(Qt::SizeBDiagCursor);
                break;
            default:
                unsetCursor();
                break;
            }
        }
        QWidget::mouseMoveEvent(event);
    }

private:
    Qt::Edges edgesAt(const QPoint &pos) const
    {
        Qt::Edges edges;
        if (pos.x() <= kResizeMargin)
            edges |= Qt::LeftEdge;
        else if (pos.x() >= width() - kResizeMargin)
            edges |= Qt::RightEdge;
        if (pos.y() <= kResizeMargin)
            edges |= Qt::TopEdge;
        else if (pos.y() >= height() - kResizeMargin)
            edges |= Qt::BottomEdge;
        return edges;
    }
};

// Qt's default tab style lays the label out in whatever space is left after
// the icon and close button, then centers the text inside that leftover
// rect — with our tabs now a fixed width, a short title like "Google" ends
// up floating in the middle instead of sitting right after the icon like a
// real browser tab. Shrinking the text rect to the label's own width turns
// that "centered in leftover space" into "left-aligned next to the icon"
// without touching the rest of the tab's layout.
// Any QSS rule targeting QTabBar#tabBar (even just background:transparent on
// the bar itself, let alone ::tab) makes Qt's QStyleSheetStyle take over
// painting that widget completely — shape *and* label — for every control
// element, not just the ones a rule mentions. That silently swallowed every
// earlier attempt at custom label painting here, since QStyleSheetStyle
// never delegated to this class's overrides at all. The tab bar now carries
// no QSS rules whatsoever (see applyEdgeTheme); this class owns 100% of its
// painting and sizing instead.
class EdgeTabBarStyle : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                            const QWidget *widget) const override
    {
        QSize s = QProxyStyle::sizeFromContents(type, option, size, widget);
        if (type == QStyle::CT_TabBarTab) {
            s.setWidth(200);
            // No QSS padding on the tab bar any more (see class comment), so
            // this replaces the vertical breathing room "padding: 9px 16px"
            // used to give each tab.
            s.setHeight(qMax(s.height(), 34));
        }
        return s;
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                      const QWidget *widget) const override
    {
        const auto *tabOpt = qstyleoption_cast<const QStyleOptionTab *>(option);

        if (element == QStyle::CE_TabBarTabShape && tabOpt) {
            QColor bg;
            if (tabOpt->state & QStyle::State_Selected)
                bg = QColor(0x30, 0x30, 0x32);
            else if (tabOpt->state & QStyle::State_MouseOver)
                bg = QColor(0x2a, 0x2a, 0x2b);
            if (bg.isValid()) {
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                const QRectF r = tabOpt->rect.adjusted(2, 0, -2, 0);
                QPainterPath path;
                path.moveTo(r.left(), r.bottom());
                path.lineTo(r.left(), r.top() + 8);
                path.quadTo(r.left(), r.top(), r.left() + 8, r.top());
                path.lineTo(r.right() - 8, r.top());
                path.quadTo(r.right(), r.top(), r.right(), r.top() + 8);
                path.lineTo(r.right(), r.bottom());
                path.closeSubpath();
                painter->fillPath(path, bg);
                painter->restore();
            }
            return;
        }

        // QCommonStyle's own CE_TabBarTabLabel painting computes the
        // icon/text split from the tab's content rect and centers the text
        // within it; painting the label ourselves, forcing left alignment,
        // is what pins the title flush next to the icon like a real browser
        // tab instead of floating in the middle of a short title's leftover
        // space.
        if (element == QStyle::CE_TabBarTabLabel && tabOpt) {
            const int hpad = qMax(proxy()->pixelMetric(QStyle::PM_TabBarTabHSpace, tabOpt, widget) / 2, 8);
            QRect content = tabOpt->rect.adjusted(hpad, 0, -hpad, 0);

            if (!tabOpt->icon.isNull()) {
                const QSize iconSize = tabOpt->iconSize.isValid() ? tabOpt->iconSize : QSize(16, 16);
                const QPixmap pix = tabOpt->icon.pixmap(
                    iconSize, 1, (tabOpt->state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled,
                    (tabOpt->state & QStyle::State_Selected) ? QIcon::On : QIcon::Off);
                const QRect iconRect(content.left(), content.top() + (content.height() - iconSize.height()) / 2,
                                      iconSize.width(), iconSize.height());
                proxy()->drawItemPixmap(painter, iconRect, Qt::AlignCenter, pix);
                content.setLeft(iconRect.right() + hpad);
            }

            QPalette pal = tabOpt->palette;
            pal.setColor(QPalette::WindowText,
                         (tabOpt->state & QStyle::State_Selected) ? QColor(0xff, 0xff, 0xff)
                                                                   : QColor(0xc8, 0xc8, 0xc9));
            const QString elided = tabOpt->fontMetrics.elidedText(tabOpt->text, Qt::ElideRight, content.width());
            proxy()->drawItemText(painter, content, Qt::AlignLeft | Qt::AlignVCenter, pal,
                                   tabOpt->state & QStyle::State_Enabled, elided, QPalette::WindowText);
            return;
        }

        QProxyStyle::drawControl(element, option, painter, widget);
    }
};

// Empty space in the tab row acts as the window's drag handle (like clicking
// blank title-bar space in Edge/Chrome): left-drag moves the window via the
// OS move loop (so Aero Snap still works), double-click toggles maximize.
class TabRowWidget : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        // Only claim the drag when the press lands on genuinely empty space.
        // Without this check, a press Qt bubbles up from a child (e.g. a tab
        // whose hit region didn't fully consume it) starts a window-drag
        // instead of reaching the tab bar, silently breaking tab clicks.
        if (event->button() == Qt::LeftButton && childAt(event->pos()) == nullptr) {
            if (QWindow *wh = window()->windowHandle()) {
                wh->startSystemMove();
                event->accept();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && childAt(event->pos()) == nullptr) {
            QWidget *w = window();
            if (w->isMaximized())
                w->showNormal();
            else
                w->showMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }
};

// Real Edge swaps its close-button glyph to white on hover for contrast
// against the red highlight; a baked QIcon pixmap won't do that on its own,
// so swap between two pre-rendered icons on enter/leave instead.
class CaptionButton : public QToolButton
{
public:
    using QToolButton::QToolButton;

    void setHoverAwareIcons(const QIcon &normal, const QIcon &hovered)
    {
        m_normalIcon = normal;
        m_hoveredIcon = hovered;
        setIcon(m_normalIcon);
    }

protected:
    void enterEvent(QEnterEvent *event) override
    {
        if (!m_hoveredIcon.isNull())
            setIcon(m_hoveredIcon);
        QToolButton::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        if (!m_normalIcon.isNull())
            setIcon(m_normalIcon);
        QToolButton::leaveEvent(event);
    }

private:
    QIcon m_normalIcon;
    QIcon m_hoveredIcon;
};
}

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Edge/Chrome fold the title bar into the tab strip, so there is no
    // separate OS-drawn caption row above it. Go frameless and draw our own
    // min/maximize/close controls in the tab row instead.
    setWindowFlag(Qt::FramelessWindowHint);
    resize(1280, 800);
    setWindowTitle(tr("LightBrowser"));

    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();

    // QtWebEngine's default UA advertises "QtWebEngine/x.y.z", which sites like
    // Google flag as non-standard and respond to with repeated captcha challenges.
    // Stripping that token leaves an accurate Chrome/<version> UA for the
    // Chromium actually embedded, without claiming a fake browser identity.
    QString ua = profile->httpUserAgent();
    ua.remove(QRegularExpression(R"(\s*QtWebEngine/\S+)"));
    // QtWebEngine misreports the OS as "Windows NT 6.2" (Windows 8) on this
    // build; correct it to match the real, current Windows version.
    ua.replace(QRegularExpression(R"(Windows NT [\d.]+)"), "Windows NT 10.0");
    profile->setHttpUserAgent(ua);

    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    // Match Edge/Chrome: open PDFs in the built-in viewer instead of forcing
    // them through downloadRequested like any other file.
    profile->settings()->setAttribute(QWebEngineSettings::PdfViewerEnabled, true);

    // The default profile has no storage path configured, which leaves it
    // running off-the-record: no cookies survive a restart, so every launch
    // looks like a brand-new anonymous session to Google's abuse detection
    // and triggers repeated "unusual traffic" captchas.
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    profile->setPersistentStoragePath(dataDir + "/profile");
    profile->setCachePath(dataDir + "/cache");

    m_downloadManager = new DownloadManager(this);
    connect(profile, &QWebEngineProfile::downloadRequested,
            m_downloadManager, &DownloadManager::handleDownload);
    connect(m_downloadManager, &DownloadManager::downloadStarted, this, [this] {
        if (m_downloadsButton)
            m_downloadsButton->setIcon(iconDownload(kIconColor, true));
    });
    connect(m_downloadManager, &DownloadManager::closed, this, &BrowserWindow::recordPopupClosed);

    m_history = new HistoryManager(this);
    m_bookmarks = new BookmarkManager(this);
    m_redirectManager = new UrlRedirectManager(this);

    m_stack = new QStackedWidget(this);

    setupFindBar();

    QWidget *tabRow = buildTabStrip();
    QWidget *navToolbar = buildToolbar();

    m_suggestions = new AddressSuggestionPopup(m_addressBar, m_history, m_bookmarks, m_addressBar);
    connect(m_suggestions, &AddressSuggestionPopup::urlChosen, this, [this](const QUrl &url) {
        navigateViewTo(currentView(), url);
    });

    m_actualUrlBar = new ActualUrlBar(this);
    m_actualUrlBar->hide();
    connect(m_dualUrlToggle, &QToolButton::toggled, m_actualUrlBar, &QWidget::setVisible);

    auto *central = new QWidget(this);
    central->setObjectName("centralWidget");
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tabRow);
    layout->addWidget(navToolbar);
    layout->addWidget(m_actualUrlBar);
    layout->addWidget(m_findBar);
    layout->addWidget(m_stack, 1);

    // central sits inside a thin resize-margin frame that stands in for the
    // native window border the frameless flag removed.
    m_outerFrame = new ResizeFrame(this);
    m_outerFrame->setObjectName("outerFrame");
    auto *frameLayout = new QVBoxLayout(m_outerFrame);
    frameLayout->setContentsMargins(kResizeMargin, kResizeMargin, kResizeMargin, kResizeMargin);
    frameLayout->setSpacing(0);
    frameLayout->addWidget(central);
    setCentralWidget(m_outerFrame);

    setupShortcuts();
    applyEdgeTheme();

    addNewTab(kHomeUrl, /*focusAddressBar=*/false);
}

QWebEngineView *BrowserWindow::currentView() const
{
    if (auto *tab = qobject_cast<TabView *>(m_stack->currentWidget()))
        return tab->webView();
    return nullptr;
}

QWebEngineView *BrowserWindow::createTabView()
{
    auto *view = new WebView(this);
    auto *page = new WebPage(QWebEngineProfile::defaultProfile(), m_redirectManager,
                              [this](QWebEnginePage::WebWindowType type) {
                                  return handleNewWindowRequest(type);
                              },
                              view);
    view->setPage(page);
    view->setProperty("loading", false);

    // Shared by iconChanged/urlChanged/loadFinished below so all three agree
    // on the same rule: a Google search tab always shows the magnifying
    // glass, regardless of which signal fires last.
    auto refreshTabIcon = [this, view](const QIcon &realIcon) {
        const int idx = m_stack->indexOf(view->parentWidget());
        if (idx < 0)
            return;
        if (!googleSearchTabTitle(view->url()).isEmpty())
            m_tabBar->setTabIcon(idx, iconSearchGlyph(QColor(0x4a, 0x9e, 0xf0)));
        else
            m_tabBar->setTabIcon(idx, realIcon.isNull() ? iconGlobe(kMutedIconColor) : realIcon);
    };

    connect(view, &QWebEngineView::titleChanged, this, [this, view, refreshTabIcon](const QString &title) {
        const int idx = m_stack->indexOf(view->parentWidget());
        if (idx >= 0) {
            const QString searchTitle = googleSearchTabTitle(view->url());
            m_tabBar->setTabText(idx, !searchTitle.isEmpty() ? searchTitle
                                      : title.isEmpty()      ? tr("New Tab")
                                                              : title);
        }
        refreshTabIcon(view->icon());
        if (view == currentView())
            updateWindowTitle(title);
    });

    connect(view, &QWebEngineView::iconChanged, this, [refreshTabIcon](const QIcon &icon) {
        refreshTabIcon(icon);
    });

    connect(view, &QWebEngineView::urlChanged, this, [this, view, refreshTabIcon](const QUrl &url) {
        refreshTabIcon(view->icon());
        if (view == currentView()) {
            updateUrlBar(url);
            updateFavoriteButtonIcon();
            if (m_actualUrlBar)
                m_actualUrlBar->setActualUrl(url);
        }
    });

    connect(view, &QWebEngineView::loadStarted, this, [this, view] {
        view->setProperty("loading", true);
        if (view == currentView())
            updateReloadStopAction();
    });

    connect(view, &QWebEngineView::loadFinished, this, [this, view, refreshTabIcon](bool ok) {
        view->setProperty("loading", false);
        if (view == currentView()) {
            updateReloadStopAction();
            updateNavigationActions();
        }
        // Some pages finish loading without ever emitting iconChanged (no
        // favicon reference at all), which would otherwise leave the tab
        // stuck on the pre-navigation placeholder icon indefinitely.
        refreshTabIcon(view->icon());
        if (ok) {
            m_history->recordVisit(displayUrlFor(view), view->title());
        }
    });

    auto *tabView = new TabView(view, m_stack);
    m_stack->addWidget(tabView);

    return view;
}

QWebEnginePage *BrowserWindow::handleNewWindowRequest(QWebEnginePage::WebWindowType type)
{
    QWebEngineView *view = createTabView();
    const int tabIdx = m_tabBar->addTab(tr("New Tab"));
    m_tabBar->setTabIcon(tabIdx, iconNewTabPage(kMutedIconColor));
    attachTabCloseButton(tabIdx);
    if (type != QWebEnginePage::WebBrowserBackgroundTab)
        m_tabBar->setCurrentIndex(tabIdx);
    return view->page();
}

QWebEngineView *BrowserWindow::addNewTab(const QUrl &url, bool focusAddressBar)
{
    QWebEngineView *view = createTabView();
    const int tabIdx = m_tabBar->addTab(tr("New Tab"));
    m_tabBar->setTabIcon(tabIdx, iconNewTabPage(kMutedIconColor));
    attachTabCloseButton(tabIdx);
    m_tabBar->setCurrentIndex(tabIdx);
    navigateViewTo(view, url);

    if (focusAddressBar) {
        // Focus once the address bar actually reflects the new tab's URL
        // (SingleShotConnection) rather than racing the async urlChanged
        // update against selectAll() by calling both synchronously here.
        connect(view, &QWebEngineView::urlChanged, this, [this](const QUrl &) {
            m_addressBar->setFocus();
            m_addressBar->selectAll();
        }, Qt::SingleShotConnection);
    }
    return view;
}

void BrowserWindow::closeTab(int index)
{
    if (m_tabBar->count() <= 1) {
        close();
        return;
    }
    QWidget *w = m_stack->widget(index);
    m_stack->removeWidget(w);
    m_tabBar->removeTab(index);
    w->deleteLater();
}

void BrowserWindow::currentTabChanged(int index)
{
    m_stack->setCurrentIndex(index);
    QWebEngineView *view = currentView();
    if (!view)
        return;
    updateUrlBar(view->url());
    updateWindowTitle(view->title());
    updateNavigationActions();
    updateReloadStopAction();
    if (m_actualUrlBar)
        m_actualUrlBar->setActualUrl(view->url());
    updateFavoriteButtonIcon();
}

void BrowserWindow::attachTabCloseButton(int index)
{
    // tabsClosable(true) makes QTabBar auto-generate a close button per tab,
    // but its icon comes from the native QStyle (a blocky, colored "close
    // window" glyph on this style) rather than a plain thin x. Replacing it
    // with our own flat button matches the subtle hover-highlighted x Edge
    // uses on its tabs.
    auto *button = new QToolButton(m_tabBar);
    button->setObjectName("tabCloseButton");
    button->setIcon(iconTabClose(kMutedIconColor));
    button->setIconSize(QSize(11, 11));
    button->setCursor(Qt::ArrowCursor);
    button->setToolTip(tr("Close tab"));
    connect(button, &QToolButton::clicked, this, [this, button] {
        for (int i = 0; i < m_tabBar->count(); ++i) {
            if (m_tabBar->tabButton(i, QTabBar::RightSide) == button) {
                closeTab(i);
                return;
            }
        }
    });
    m_tabBar->setTabButton(index, QTabBar::RightSide, button);
}

QWidget *BrowserWindow::buildTabStrip()
{
    m_tabBar = new QTabBar(this);
    m_tabBar->setObjectName("tabBar");
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setDrawBase(false);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setElideMode(Qt::ElideRight);
    // Real favicons come in whatever resolution/padding the site's .ico
    // provides; without a fixed icon size, Qt scales each one differently
    // and they end up sitting at slightly different vertical positions next
    // to the tab text. Locking every tab to one icon box keeps them level.
    m_tabBar->setIconSize(QSize(16, 16));
    // Parented to the tab bar so Qt deletes it along with the widget.
    auto *tabStyle = new EdgeTabBarStyle();
    tabStyle->setParent(m_tabBar);
    m_tabBar->setStyle(tabStyle);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &BrowserWindow::closeTab);
    connect(m_tabBar, &QTabBar::currentChanged, this, &BrowserWindow::currentTabChanged);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *page = m_stack->widget(from);
        m_stack->removeWidget(page);
        m_stack->insertWidget(to, page);
    });

    m_newTabButton = new QToolButton(this);
    m_newTabButton->setObjectName("newTabButton");
    m_newTabButton->setText(QStringLiteral("+"));
    m_newTabButton->setToolTip(tr("New tab"));
    connect(m_newTabButton, &QToolButton::clicked, this, [this] { addNewTab(kHomeUrl); });

    m_minButton = new QToolButton(this);
    m_minButton->setObjectName("captionButton");
    m_minButton->setIcon(iconMinimizeGlyph(kMutedIconColor));
    m_minButton->setIconSize(QSize(14, 14));
    m_minButton->setToolTip(tr("Minimize"));
    connect(m_minButton, &QToolButton::clicked, this, &BrowserWindow::showMinimized);

    m_maxButton = new QToolButton(this);
    m_maxButton->setObjectName("captionButton");
    m_maxButton->setIconSize(QSize(14, 14));
    m_maxButton->setToolTip(tr("Maximize"));
    connect(m_maxButton, &QToolButton::clicked, this, [this] {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });

    auto *closeButton = new CaptionButton(this);
    closeButton->setObjectName("closeCaptionButton");
    closeButton->setHoverAwareIcons(iconCloseGlyph(kMutedIconColor), iconCloseGlyph(Qt::white));
    closeButton->setIconSize(QSize(14, 14));
    closeButton->setToolTip(tr("Close"));
    connect(closeButton, &QToolButton::clicked, this, &BrowserWindow::close);
    m_closeButton = closeButton;

    updateMaximizeButtonIcon();

    // A plain (non-drag-handled) QWidget for tabRow would swallow clicks in
    // its empty space; TabRowWidget turns that empty space into the window's
    // drag handle instead, same as blank title-bar space in a real browser.
    // The row has a fixed height so every child can be aligned against it
    // deliberately: the tab bar and "+" button float centered with a gap
    // above (like real tabs), while the caption buttons fill the row edge
    // to edge with no gap, exactly like a native title bar's controls.
    auto *tabRow = new TabRowWidget(this);
    tabRow->setObjectName("tabRow");
    tabRow->setFixedHeight(40);
    auto *tabRowLayout = new QHBoxLayout(tabRow);
    tabRowLayout->setContentsMargins(8, 0, 0, 0);
    tabRowLayout->setSpacing(4);
    auto *tabSeparator = new QWidget(tabRow);
    tabSeparator->setObjectName("tabSeparator");
    tabSeparator->setFixedSize(1, 20);

    tabRowLayout->addWidget(m_tabBar, 0, Qt::AlignVCenter);
    tabRowLayout->addWidget(tabSeparator, 0, Qt::AlignVCenter);
    tabRowLayout->addWidget(m_newTabButton, 0, Qt::AlignVCenter);
    tabRowLayout->addStretch(1);
    tabRowLayout->addWidget(m_minButton, 0, Qt::AlignVCenter);
    tabRowLayout->addWidget(m_maxButton, 0, Qt::AlignVCenter);
    tabRowLayout->addWidget(m_closeButton, 0, Qt::AlignVCenter);

    return tabRow;
}

void BrowserWindow::setupFindBar()
{
    m_findBar = new QWidget(this);
    m_findBar->hide();

    auto *l = new QHBoxLayout(m_findBar);
    l->setContentsMargins(4, 4, 4, 4);

    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText(tr("Find in page..."));
    auto *prevButton = new QPushButton(tr("Prev"), m_findBar);
    auto *nextButton = new QPushButton(tr("Next"), m_findBar);
    auto *closeButton = new QPushButton(tr("X"), m_findBar);
    closeButton->setFixedWidth(28);

    l->addWidget(m_findEdit);
    l->addWidget(prevButton);
    l->addWidget(nextButton);
    l->addWidget(closeButton);

    auto doFind = [this](bool backward) {
        QWebEngineView *view = currentView();
        if (!view)
            return;
        QWebEnginePage::FindFlags flags;
        if (backward)
            flags |= QWebEnginePage::FindBackward;
        view->findText(m_findEdit->text(), flags);
    };

    connect(m_findEdit, &QLineEdit::returnPressed, this, [doFind] { doFind(false); });
    connect(nextButton, &QPushButton::clicked, this, [doFind] { doFind(false); });
    connect(prevButton, &QPushButton::clicked, this, [doFind] { doFind(true); });
    connect(closeButton, &QPushButton::clicked, this, [this] {
        if (currentView())
            currentView()->findText(QString());
        m_findBar->hide();
        if (currentView())
            currentView()->setFocus();
    });

    auto *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), m_findBar);
    escShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escShortcut, &QShortcut::activated, closeButton, &QPushButton::click);
}

QWidget *BrowserWindow::buildToolbar()
{
    auto *navToolbar = new QWidget(this);
    navToolbar->setObjectName("navToolbar");
    auto *navLayout = new QHBoxLayout(navToolbar);
    navLayout->setContentsMargins(10, 6, 10, 6);
    navLayout->setSpacing(4);

    m_backButton = new QToolButton(navToolbar);
    m_backButton->setObjectName("navButton");
    m_backButton->setIcon(iconChevronLeft(kIconColor));
    m_backButton->setIconSize(QSize(18, 18));
    m_backButton->setToolTip(tr("Back"));
    connect(m_backButton, &QToolButton::clicked, this, [this] {
        if (currentView())
            currentView()->back();
    });

    m_forwardButton = new QToolButton(navToolbar);
    m_forwardButton->setObjectName("navButton");
    m_forwardButton->setIcon(iconChevronRight(kIconColor));
    m_forwardButton->setIconSize(QSize(18, 18));
    m_forwardButton->setToolTip(tr("Forward"));
    connect(m_forwardButton, &QToolButton::clicked, this, [this] {
        if (currentView())
            currentView()->forward();
    });

    m_reloadStopButton = new QToolButton(navToolbar);
    m_reloadStopButton->setObjectName("navButton");
    m_reloadStopButton->setIcon(iconReload(kIconColor));
    m_reloadStopButton->setIconSize(QSize(18, 18));
    m_reloadStopButton->setToolTip(tr("Reload"));
    connect(m_reloadStopButton, &QToolButton::clicked, this, &BrowserWindow::toggleReloadStop);

    navLayout->addWidget(m_backButton);
    navLayout->addWidget(m_forwardButton);
    navLayout->addWidget(m_reloadStopButton);

    auto *addressBar = new QWidget(navToolbar);
    addressBar->setObjectName("addressBar");
    auto *addressLayout = new QHBoxLayout(addressBar);
    addressLayout->setContentsMargins(10, 0, 10, 0);
    addressLayout->setSpacing(6);

    m_lockLabel = new QLabel(addressBar);
    m_lockLabel->setObjectName("lockLabel");
    m_lockLabel->setPixmap(iconLock(kMutedIconColor).pixmap(13, 13));

    m_addressBar = new QLineEdit(addressBar);
    m_addressBar->setObjectName("addressEdit");
    m_addressBar->setFrame(false);
    m_addressBar->setPlaceholderText(tr("Ask Google or type a URL"));
    connect(m_addressBar, &QLineEdit::returnPressed, this, &BrowserWindow::navigateToAddress);

    m_dualUrlToggle = new QToolButton(addressBar);
    m_dualUrlToggle->setObjectName("dualUrlToggle");
    m_dualUrlToggle->setText(QStringLiteral("\U0001F441"));
    m_dualUrlToggle->setCheckable(true);
    m_dualUrlToggle->setToolTip(tr("Show actual URL"));
    m_dualUrlToggle->setVisible(false);
    // ActualUrlBar (m_actualUrlBar) is constructed after buildToolbar()
    // returns, since it needs `this` as its parent; the visibility toggle is
    // wired to it back in the constructor.

    addressLayout->addWidget(m_lockLabel);
    addressLayout->addWidget(m_addressBar, 1);
    addressLayout->addWidget(m_dualUrlToggle);
    addressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    navLayout->addWidget(addressBar, 1);

    m_favoriteButton = new QToolButton(navToolbar);
    m_favoriteButton->setObjectName("chromeButton");
    m_favoriteButton->setIcon(iconStar(kIconColor));
    m_favoriteButton->setIconSize(QSize(18, 18));
    m_favoriteButton->setToolTip(tr("Favorite this page"));
    connect(m_favoriteButton, &QToolButton::clicked, this, &BrowserWindow::toggleFavoriteForCurrentTab);

    m_downloadsButton = new QToolButton(navToolbar);
    m_downloadsButton->setObjectName("chromeButton");
    m_downloadsButton->setIcon(iconDownload(kIconColor));
    m_downloadsButton->setIconSize(QSize(18, 18));
    m_downloadsButton->setToolTip(tr("Downloads"));
    connect(m_downloadsButton, &QToolButton::clicked, this, &BrowserWindow::toggleDownloadsPopup);

    m_extensionsButton = new QToolButton(navToolbar);
    m_extensionsButton->setObjectName("chromeButton");
    m_extensionsButton->setIcon(iconPuzzle(kIconColor));
    m_extensionsButton->setIconSize(QSize(18, 18));
    m_extensionsButton->setToolTip(tr("Extensions"));
    connect(m_extensionsButton, &QToolButton::clicked, this, &BrowserWindow::showExtensionsMenu);

    m_profileAvatar = new QLabel(navToolbar);
    m_profileAvatar->setObjectName("profileAvatar");
    m_profileAvatar->setAlignment(Qt::AlignCenter);
    m_profileAvatar->setCursor(Qt::PointingHandCursor);
    m_profileAvatar->installEventFilter(this);
    updateProfileAvatar();

    m_menuButton = new QToolButton(navToolbar);
    m_menuButton->setObjectName("chromeButton");
    m_menuButton->setIcon(iconMenuDots(kIconColor));
    m_menuButton->setIconSize(QSize(18, 18));
    m_menuButton->setToolTip(tr("Settings and more"));
    connect(m_menuButton, &QToolButton::clicked, this, &BrowserWindow::showMainMenu);

    navLayout->addWidget(m_favoriteButton);
    navLayout->addWidget(m_downloadsButton);
    navLayout->addWidget(m_extensionsButton);
    navLayout->addWidget(m_profileAvatar);
    navLayout->addWidget(m_menuButton);

    return navToolbar;
}

void BrowserWindow::setupShortcuts()
{
    new QShortcut(QKeySequence("Ctrl+L"), this, [this] {
        m_addressBar->setFocus();
        m_addressBar->selectAll();
    });

    new QShortcut(QKeySequence("Ctrl+T"), this, [this] { addNewTab(kHomeUrl); });

    new QShortcut(QKeySequence("Ctrl+W"), this, [this] { closeTab(m_tabBar->currentIndex()); });

    new QShortcut(QKeySequence("Ctrl+R"), this, [this] {
        if (currentView())
            currentView()->reload();
    });
    new QShortcut(QKeySequence::Refresh, this, [this] {
        if (currentView())
            currentView()->reload();
    });

    new QShortcut(QKeySequence("Ctrl+Shift+R"), this, [this] {
        if (currentView())
            currentView()->triggerPageAction(QWebEnginePage::ReloadAndBypassCache);
    });

    new QShortcut(QKeySequence("Ctrl+Tab"), this, [this] {
        if (m_tabBar->count() > 0)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % m_tabBar->count());
    });
    new QShortcut(QKeySequence("Ctrl+Shift+Tab"), this, [this] {
        if (m_tabBar->count() > 0)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + m_tabBar->count() - 1) % m_tabBar->count());
    });

    new QShortcut(QKeySequence("Ctrl+F"), this, [this] {
        m_findBar->show();
        m_findEdit->setFocus();
        m_findEdit->selectAll();
    });

    new QShortcut(QKeySequence("Ctrl+H"), this, [this] { showHistoryPage(); });

    auto openDevTools = [this] {
        if (auto *tab = qobject_cast<TabView *>(m_stack->currentWidget()))
            tab->openDevTools();
    };
    new QShortcut(QKeySequence("F12"), this, openDevTools);
    new QShortcut(QKeySequence("Ctrl+Shift+I"), this, openDevTools);

    auto zoomIn = [this] {
        if (QWebEngineView *v = currentView())
            v->setZoomFactor(qMin(v->zoomFactor() + 0.1, 5.0));
    };
    auto zoomOut = [this] {
        if (QWebEngineView *v = currentView())
            v->setZoomFactor(qMax(v->zoomFactor() - 0.1, 0.25));
    };
    auto zoomReset = [this] {
        if (QWebEngineView *v = currentView())
            v->setZoomFactor(1.0);
    };

    new QShortcut(QKeySequence::ZoomIn, this, zoomIn);
    new QShortcut(QKeySequence("Ctrl+="), this, zoomIn);
    new QShortcut(QKeySequence::ZoomOut, this, zoomOut);
    new QShortcut(QKeySequence("Ctrl+0"), this, zoomReset);
}

void BrowserWindow::navigateToAddress()
{
    QWebEngineView *view = currentView();
    if (!view)
        return;
    const QUrl url = resolveInput(m_addressBar->text());
    if (url.isEmpty())
        return;
    navigateViewTo(view, url);
    view->setFocus();
}

void BrowserWindow::navigateViewTo(QWebEngineView *view, const QUrl &url)
{
    if (!view || url.isEmpty())
        return;
    QUrl mapped;
    const bool ruleMatched = m_redirectManager->resolve(url, &mapped);

    // Only freeze the primary address bar on the requested URL when a
    // configured mapping rule actually applies to this navigation. With no
    // matching rule, clear any leftover request from a prior navigation so
    // the tab behaves like a completely normal browser — the primary bar
    // tracks the real URL (including ordinary site redirects) via urlChanged
    // as usual, and the lower ActualUrlBar just mirrors the same value.
    view->setProperty("requestedUrl", ruleMatched ? QVariant(url) : QVariant());

    view->setUrl(ruleMatched ? mapped : url);

    if (view == currentView())
        updateUrlBar(view->url());
}

void BrowserWindow::toggleReloadStop()
{
    QWebEngineView *view = currentView();
    if (!view)
        return;
    if (view->property("loading").toBool())
        view->stop();
    else
        view->reload();
}

void BrowserWindow::updateNavigationActions()
{
    QWebEngineView *view = currentView();
    const bool hasView = view != nullptr;
    m_backButton->setEnabled(hasView && view->history()->canGoBack());
    m_forwardButton->setEnabled(hasView && view->history()->canGoForward());
}

void BrowserWindow::updateUrlBar(const QUrl &actualUrl)
{
    Q_UNUSED(actualUrl);
    // The primary bar always shows what was actually requested for the
    // current tab (set in navigateViewTo), not wherever the navigation
    // happened to land — that real destination is what ActualUrlBar is for.
    // Pages that never went through navigateViewTo (e.g. a JS-opened popup)
    // have no recorded request, so fall back to the real URL for those.
    const QUrl display = displayUrlFor(currentView());
    // A blank/new tab (nothing ever navigated to) shows an empty bar with
    // the "Ask Google or type a URL" placeholder and Google's own mark,
    // matching Edge's default new-tab address bar, instead of a raw
    // "about:blank" and a padlock that implies a real, secure page.
    const bool blank = display.isEmpty() || display.scheme() == QLatin1String("about");
    m_lockLabel->setPixmap(blank ? iconGoogleG().pixmap(16, 16) : iconLock(kMutedIconColor).pixmap(13, 13));
    m_addressBar->setText(blank ? QString() : display.toString());
    // QLineEdit::setText() leaves the cursor (and visible scroll position) at
    // the end of the text; for a long URL that scrolls the domain out of
    // view, showing garbled tracking-parameter tail characters instead. Real
    // address bars always show the start of the URL first.
    m_addressBar->setCursorPosition(0);
}

QUrl BrowserWindow::displayUrlFor(QWebEngineView *view) const
{
    if (!view)
        return QUrl();
    const QUrl requested = view->property("requestedUrl").toUrl();
    if (requested.isValid())
        return requested;
    QUrl sourceStyleUrl;
    return m_redirectManager->reverseResolve(view->url(), &sourceStyleUrl) ? sourceStyleUrl : view->url();
}

void BrowserWindow::updateReloadStopAction()
{
    QWebEngineView *view = currentView();
    const bool loading = view && view->property("loading").toBool();
    m_reloadStopButton->setIcon(loading ? iconStop(kIconColor) : iconReload(kIconColor));
    m_reloadStopButton->setToolTip(loading ? tr("Stop") : tr("Reload"));
}

void BrowserWindow::updateFavoriteButtonIcon()
{
    if (!m_favoriteButton)
        return;
    QWebEngineView *view = currentView();
    const bool favorited = view && m_bookmarks->isBookmarked(displayUrlFor(view));
    m_favoriteButton->setIcon(favorited ? iconStarFilled(QColor(0xff, 0xd4, 0x3b)) : iconStar(kIconColor));
    m_favoriteButton->setToolTip(favorited ? tr("Edit favorite") : tr("Favorite this page"));
}

void BrowserWindow::toggleFavoriteForCurrentTab()
{
    QWebEngineView *view = currentView();
    if (!view)
        return;
    const QUrl bookmarkUrl = displayUrlFor(view);
    if (m_bookmarks->isBookmarked(bookmarkUrl))
        m_bookmarks->remove(bookmarkUrl);
    else
        m_bookmarks->add(bookmarkUrl, view->title());
    updateFavoriteButtonIcon();
}

bool BrowserWindow::popupJustClosed() const
{
    return QDateTime::currentMSecsSinceEpoch() - m_lastPopupCloseMs < 200;
}

void BrowserWindow::recordPopupClosed()
{
    m_lastPopupCloseMs = QDateTime::currentMSecsSinceEpoch();
}

void BrowserWindow::showExtensionsMenu()
{
    if (popupJustClosed())
        return;
    QMenu menu(this);
    menu.setObjectName("chromeMenu");
    QAction *empty = menu.addAction(tr("No extensions installed"));
    empty->setEnabled(false);
    menu.exec(m_extensionsButton->mapToGlobal(QPoint(0, m_extensionsButton->height())));
    recordPopupClosed();
}

QString BrowserWindow::profileDisplayName() const
{
    QSettings settings;
    const QString stored = settings.value("Profile/DisplayName").toString();
    if (!stored.isEmpty())
        return stored;

    // No name chosen yet — default to the real local Windows account name
    // instead of a fake placeholder.
    QString osUser = qEnvironmentVariable("USERNAME");
    if (osUser.isEmpty())
        osUser = qEnvironmentVariable("USER");
    return osUser.isEmpty() ? tr("You") : osUser;
}

void BrowserWindow::setProfileDisplayName(const QString &name)
{
    QSettings settings;
    settings.setValue("Profile/DisplayName", name);
    updateProfileAvatar();
}

void BrowserWindow::updateProfileAvatar()
{
    m_profileAvatar->setPixmap(iconProfileAvatar(QColor(0x6a, 0x4f, 0xd6), Qt::white).pixmap(24, 24));
    m_profileAvatar->setToolTip(profileDisplayName());
}

void BrowserWindow::promptRenameProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Profile name"), tr("Display name:"),
                                                 QLineEdit::Normal, profileDisplayName(), &ok);
    if (ok && !name.trimmed().isEmpty())
        setProfileDisplayName(name.trimmed());
}

void BrowserWindow::showProfileMenu()
{
    if (popupJustClosed())
        return;
    QMenu menu(this);
    menu.setObjectName("chromeMenu");
    QAction *whoami = menu.addAction(profileDisplayName());
    whoami->setEnabled(false);
    menu.addAction(tr("Edit name..."), this, &BrowserWindow::promptRenameProfile);
    menu.addSeparator();

    const QVector<BookmarkEntry> favorites = m_bookmarks->all();
    if (favorites.isEmpty()) {
        QAction *empty = menu.addAction(tr("No favorites yet"));
        empty->setEnabled(false);
    } else {
        for (const BookmarkEntry &entry : favorites) {
            const QString label = entry.title.isEmpty() ? entry.url.toString() : entry.title;
            const QUrl url = entry.url;
            menu.addAction(label, this, [this, url] { navigateViewTo(currentView(), url); });
        }
    }
    menu.exec(m_profileAvatar->mapToGlobal(QPoint(0, m_profileAvatar->height())));
    recordPopupClosed();
}

void BrowserWindow::showMainMenu()
{
    if (popupJustClosed())
        return;
    QMenu menu(this);
    menu.setObjectName("chromeMenu");

    menu.addAction(tr("New tab\tCtrl+T"), this, [this] { addNewTab(kHomeUrl); });
    menu.addAction(tr("Close tab\tCtrl+W"), this, [this] { closeTab(m_tabBar->currentIndex()); });
    menu.addSeparator();

    QAction *findAction = menu.addAction(tr("Find on page\tCtrl+F"));
    connect(findAction, &QAction::triggered, this, [this] {
        m_findBar->show();
        m_findEdit->setFocus();
        m_findEdit->selectAll();
    });

    QMenu *zoomMenu = menu.addMenu(tr("Zoom"));
    zoomMenu->setObjectName("chromeMenu");
    zoomMenu->addAction(tr("Zoom in\tCtrl++"), this, [this] {
        if (QWebEngineView *v = currentView())
            v->setZoomFactor(qMin(v->zoomFactor() + 0.1, 5.0));
    });
    zoomMenu->addAction(tr("Zoom out\tCtrl+-"), this, [this] {
        if (QWebEngineView *v = currentView())
            v->setZoomFactor(qMax(v->zoomFactor() - 0.1, 0.25));
    });
    zoomMenu->addAction(tr("Reset zoom\tCtrl+0"), this, [this] {
        if (QWebEngineView *v = currentView())
            v->setZoomFactor(1.0);
    });

    menu.addSeparator();
    // Anchors under the toolbar download button (and clears its badge),
    // instead of a bare show() that would leave the popup positioned
    // wherever it last was — off in a corner rather than near this menu.
    menu.addAction(tr("Downloads"), this, &BrowserWindow::toggleDownloadsPopup);
    menu.addAction(tr("History\tCtrl+H"), this, &BrowserWindow::showHistoryPage);
    menu.addAction(tr("Settings"), this, &BrowserWindow::showUrlMappingSettings);
    menu.addSeparator();
    menu.addAction(tr("About LightBrowser"), this, [this] {
        QMessageBox::about(this, tr("About LightBrowser"),
                            tr("LightBrowser\nA lightweight QtWebEngine browser."));
    });

    menu.exec(m_menuButton->mapToGlobal(QPoint(menu.sizeHint().width() - m_menuButton->width(),
                                                m_menuButton->height())));
    recordPopupClosed();
}

void BrowserWindow::showHistoryPage()
{
    QWebEngineView *view = currentView();
    if (!view)
        return;

    QString html = QStringLiteral(
        "<html><head><title>History</title><style>"
        "body{background:#1b1b1c;color:#e3e3e3;font-family:sans-serif;padding:24px;}"
        "a{color:#8ab4f8;text-decoration:none;} a:hover{text-decoration:underline;}"
        "li{margin-bottom:10px;} h1{font-size:20px;} .url{color:#9c9c9c;font-size:12px;}"
        "</style></head><body><h1>History</h1><ul>");

    const QVector<HistoryEntry> entries = m_history->recentEntries();
    if (entries.isEmpty()) {
        html += QStringLiteral("<p class=\"url\">No history yet.</p>");
    } else {
        for (const HistoryEntry &entry : entries) {
            const QString title = entry.title.isEmpty() ? entry.url.toString() : entry.title;
            html += QStringLiteral("<li><a href=\"%1\">%2</a><br><span class=\"url\">%1</span></li>")
                        .arg(entry.url.toString().toHtmlEscaped(), title.toHtmlEscaped());
        }
    }
    html += QStringLiteral("</ul></body></html>");

    view->setHtml(html, QUrl("about:blank"));
}

void BrowserWindow::showUrlMappingSettings()
{
    UrlMappingSettingsDialog dlg(m_redirectManager, this);
    dlg.exec();
    // Rules may have changed (added/edited/deleted/toggled) — re-derive the
    // current tab's address-bar display against them.
    if (QWebEngineView *view = currentView())
        updateUrlBar(view->url());
}

void BrowserWindow::toggleDownloadsPopup()
{
    if (m_downloadManager->isVisible()) {
        m_downloadManager->hide();
        return;
    }
    if (popupJustClosed())
        return;
    m_downloadManager->showBelow(m_downloadsButton);
    // Opening the flyout is the same "seen it" moment Edge clears its
    // notification dot on.
    m_downloadsButton->setIcon(iconDownload(kIconColor, false));
}

void BrowserWindow::updateWindowTitle(const QString &pageTitle)
{
    setWindowTitle(pageTitle.isEmpty() ? tr("LightBrowser") : pageTitle + tr(" - LightBrowser"));
}

QUrl BrowserWindow::resolveInput(const QString &textIn) const
{
    const QString text = textIn.trimmed();
    if (text.isEmpty())
        return QUrl();

    const QUrl asUrl(text);
    if (asUrl.scheme() == "http" || asUrl.scheme() == "https" || asUrl.scheme() == "file"
        || asUrl.scheme() == "ftp" || asUrl.scheme() == "about" || asUrl.scheme() == "chrome") {
        return asUrl;
    }

    static const QRegularExpression hostLike(R"(^([\w-]+\.)+[a-zA-Z]{2,}(:\d+)?(/.*)?$)");
    static const QRegularExpression ipLike(R"(^(\d{1,3}\.){3}\d{1,3}(:\d+)?(/.*)?$)");

    if (!text.contains(' ')
        && (hostLike.match(text).hasMatch() || ipLike.match(text).hasMatch()
            || text == "localhost" || text.startsWith("localhost:"))) {
        return QUrl("https://" + text);
    }

    QUrl searchUrl("https://www.google.com/search");
    QUrlQuery query;
    query.addQueryItem("q", text);
    searchUrl.setQuery(query);
    return searchUrl;
}

void BrowserWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::WindowStateChange || !m_outerFrame)
        return;

    // Maximized: the window already fills the screen, so the resize-margin
    // border would just clip content off-screen — collapse it to 0.
    const int margin = isMaximized() ? 0 : kResizeMargin;
    if (auto *frameLayout = qobject_cast<QVBoxLayout *>(m_outerFrame->layout()))
        frameLayout->setContentsMargins(margin, margin, margin, margin);

    updateMaximizeButtonIcon();
}

bool BrowserWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_profileAvatar && event->type() == QEvent::MouseButtonRelease) {
        showProfileMenu();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void BrowserWindow::updateMaximizeButtonIcon()
{
    if (!m_maxButton)
        return;
    if (isMaximized()) {
        m_maxButton->setIcon(iconRestoreGlyph(kMutedIconColor));
        m_maxButton->setToolTip(tr("Restore"));
    } else {
        m_maxButton->setIcon(iconMaximizeGlyph(kMutedIconColor));
        m_maxButton->setToolTip(tr("Maximize"));
    }
}

void BrowserWindow::applyEdgeTheme()
{
    setStyleSheet(R"(
        QWidget#centralWidget, QStackedWidget, QMainWindow {
            background: #1b1b1c;
        }

        QWidget#outerFrame {
            background: #000000;
        }

        QWidget#tabRow {
            background: #202021;
            border-bottom: 1px solid #101011;
        }

        QToolButton#tabCloseButton {
            background: transparent;
            border: none;
            border-radius: 10px;
            min-width: 20px;
            max-width: 20px;
            min-height: 20px;
            max-height: 20px;
            margin-left: 4px;
        }
        QToolButton#tabCloseButton:hover {
            background: rgba(255, 255, 255, 0.16);
        }

        QWidget#tabSeparator {
            background: rgba(255, 255, 255, 0.18);
        }

        QToolButton#newTabButton {
            background: transparent;
            color: #c8c8c9;
            border: none;
            border-radius: 16px;
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
            font-size: 20px;
        }
        QToolButton#newTabButton:hover {
            background: #2a2a2b;
        }

        QToolButton#captionButton, QToolButton#closeCaptionButton {
            background: transparent;
            color: #c8c8c9;
            border: none;
            border-radius: 0px;
            min-width: 46px;
            max-width: 46px;
            min-height: 40px;
            max-height: 40px;
        }
        QToolButton#captionButton:hover {
            background: #3a3a3c;
        }
        QToolButton#closeCaptionButton:hover {
            background: #e81123;
            color: white;
        }

        QWidget#navToolbar {
            background: #303032;
        }

        QToolButton#navButton, QToolButton#chromeButton {
            background: transparent;
            color: #e3e3e3;
            border: none;
            border-radius: 16px;
            min-width: 32px;
            max-width: 32px;
            min-height: 32px;
            max-height: 32px;
        }
        QToolButton#navButton:hover, QToolButton#chromeButton:hover {
            background: rgba(255, 255, 255, 0.10);
        }
        QToolButton#navButton:disabled {
            color: #6a6a6a;
        }

        QWidget#addressBar {
            background: #202021;
            border-radius: 16px;
            min-height: 30px;
            max-height: 30px;
        }
        QLabel#lockLabel {
            color: #9c9c9c;
            font-size: 12px;
        }
        QToolButton#dualUrlToggle {
            background: transparent;
            border: none;
            border-radius: 12px;
            min-width: 22px;
            max-width: 22px;
            min-height: 22px;
            max-height: 22px;
            font-size: 12px;
        }
        QToolButton#dualUrlToggle:hover {
            background: rgba(255, 255, 255, 0.10);
        }
        QToolButton#dualUrlToggle:checked {
            background: #3a6ea5;
        }
        QWidget#actualUrlBar {
            background: #26262a;
            border-bottom: 1px solid #101011;
        }
        QLabel#actualUrlLabel {
            color: #9c9c9c;
            font-size: 11px;
        }
        QLineEdit#actualUrlEdit {
            background: transparent;
            border: none;
            color: #e3e3e3;
            font-size: 12px;
        }
        QToolButton#actualUrlCopyButton {
            background: transparent;
            color: #c8c8c9;
            border: 1px solid #3a3a3c;
            border-radius: 4px;
            padding: 2px 8px;
            font-size: 11px;
        }
        QToolButton#actualUrlCopyButton:hover {
            background: #3a3a3c;
        }
        QLineEdit#addressEdit {
            background: transparent;
            border: none;
            color: #e3e3e3;
            font-size: 13px;
            selection-background-color: #3a6ea5;
        }

        QLabel#profileAvatar {
            min-width: 24px;
            max-width: 24px;
            min-height: 24px;
            max-height: 24px;
            margin: 0 2px;
        }

        QMenu#chromeMenu {
            background: #2b2b2c;
            color: #e3e3e3;
            border: 1px solid #3a3a3c;
            padding: 4px;
        }
        QMenu#chromeMenu::item {
            padding: 6px 24px;
            border-radius: 4px;
        }
        QMenu#chromeMenu::item:selected {
            background: #3a3a3c;
        }
        QMenu#chromeMenu::item:disabled {
            color: #7a7a7b;
        }
        QMenu#chromeMenu::separator {
            height: 1px;
            background: #3a3a3c;
            margin: 4px 8px;
        }
    )");
}
