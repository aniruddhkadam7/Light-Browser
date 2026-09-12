#include "BrowserWindow.h"

#include <QApplication>
#include <QIcon>
#include <QRegularExpression>
#include <QWebEngineProfile>

int main(int argc, char *argv[])
{
    // Chromium's own internal GPU blocklist silently disables hardware
    // compositing for GPUs/drivers it doesn't recognize as safe, dropping to
    // a software compositor with no error — exactly the "Software only"
    // fallback described below, and the actual root cause of it on a lot of
    // real machines. --ignore-gpu-blocklist forces acceleration on anyway;
    // --enable-gpu-rasterization makes Chromium actually rasterize on the
    // GPU rather than just compositing already-software-drawn tiles. Must be
    // set before QApplication exists — Chromium reads it at engine init.
    // --disable-features=UserAgentClientHint turns off Chromium's separate
    // "User-Agent Client Hints" headers (Sec-CH-UA / Sec-CH-UA-Full-Version-
    // List). Those carry their own embedder brand entry (QtWebEngine) that a
    // plain httpUserAgent() override further down doesn't touch — sites can
    // still read it there even after the classic User-Agent string is
    // cleaned up, which is exactly what kept Google's sign-in flow blocking
    // this browser as an unrecognized embedded WebView. This turns the
    // extra header channel off rather than fabricating a fake brand for it.
    // --enable-zero-copy avoids an extra texture copy in the rasterization
    // path. --disable-features=CalculateNativeWinOcclusion turns off
    // Chromium's window-occlusion detection, which misfires on non-standard
    // (frameless/custom-chrome) top-level windows like this one's and can
    // skip painting a visible frame — a documented cause of a page
    // appearing to blink.
    //
    // Deliberately NOT forcing --use-angle=d3d11 / --force_high_performance_gpu
    // here: chrome://gpu showed those made things worse, not better — with
    // them, "Direct Rendering Display Compositor" (the DirectComposition
    // path that hands frames straight to the Windows compositor — the main
    // thing that makes scrolling actually smooth) came back Disabled even
    // though Compositing/Rasterization both still reported "Hardware
    // accelerated". Chromium's own auto-detection picks the right backend
    // more often than a hardcoded one on a system with drivers from more
    // than one GPU vendor loaded, which this one has.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            "--ignore-gpu-blocklist --enable-gpu-rasterization --enable-zero-copy "
            "--disable-features=UserAgentClientHint,CalculateNativeWinOcclusion");

    // QtWebEngine derives its default persistent-profile storage path from
    // these; without them the default profile silently runs off-the-record,
    // so cookies (including Google's trust/history cookies) never survive a
    // restart and every launch looks like a brand-new anonymous session.
    QCoreApplication::setOrganizationName("LightBrowser");
    QCoreApplication::setApplicationName("LightBrowser");

    // Required by Qt for QWebEngineView to get a properly GPU-accelerated
    // compositor; without it chrome://gpu shows rasterization/WebGL as
    // hardware-accelerated but the actual frame compositor silently falls
    // back to "Software only", producing janky, non-smooth scrolling.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // QWebEngineView is a *native* child window; Qt/Chromium composite it
    // independently of the rest of the (non-native) widget tree around it.
    // Without this, any non-native sibling overlapping or surrounding it —
    // the tab bar, the resize-margin frame, and now the DevTools QSplitter —
    // fights the native surface for paint order, producing visible flicker
    // and stuttery scrolling. This is Qt's own documented fix for exactly
    // that native/non-native mixing case.
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/app.ico"));

    // Google's sign-in flow (and some other sites) refuses to complete OAuth
    // at all for any browser whose User-Agent advertises "QtWebEngine/x.y.z"
    // — "This browser or app may not be secure" — treating it as an
    // unrecognized embedded WebView. Stripping just that one vendor token
    // (keeping the real Chrome/xxx version Qt's own Chromium build already
    // reports, not a fabricated one) is enough to pass that check.
    QWebEngineProfile *defaultProfile = QWebEngineProfile::defaultProfile();
    QString userAgent = defaultProfile->httpUserAgent();
    static const QRegularExpression qtWebEngineToken(QStringLiteral(" QtWebEngine/\\S+"));
    userAgent.remove(qtWebEngineToken);
    defaultProfile->setHttpUserAgent(userAgent);

    BrowserWindow window;
    window.showMaximized();

    return app.exec();
}
