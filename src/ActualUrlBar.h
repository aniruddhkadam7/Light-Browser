#pragma once

#include <QWidget>
#include <QUrl>

class QLineEdit;

// Read-only second row shown under the address bar (toggled by an eye
// button in the toolbar) displaying the real QWebEngineView::url() —
// distinct from the primary address bar, which may show a source-style
// alias when a URL mapping rule is active. Hidden by default.
class ActualUrlBar : public QWidget
{
    Q_OBJECT
public:
    explicit ActualUrlBar(QWidget *parent = nullptr);

    void setActualUrl(const QUrl &url);

private:
    QLineEdit *m_edit;
};
