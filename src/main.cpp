#include "BrowserWindow.h"

#include <QApplication>
#include <QIcon>

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
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--ignore-gpu-blocklist --enable-gpu-rasterization");

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

    BrowserWindow window;
    window.showMaximized();

    return app.exec();
}
