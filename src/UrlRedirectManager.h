#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct RedirectRule
{
    QString id;
    QString source; // e.g. https://app.company.com/*
    QString target; // e.g. https://staging.company.com/*
    bool enabled = true;
};

// Developer/enterprise URL mapping: navigating to a configured source
// pattern actually loads the mapped target (real navigation, normal
// cookies/origin/security). Rules persist via QSettings.
class UrlRedirectManager : public QObject
{
    Q_OBJECT
public:
    explicit UrlRedirectManager(QObject *parent = nullptr);

    QVector<RedirectRule> rules() const;
    void addRule(const QString &source, const QString &target, bool enabled = true);
    void updateRule(const QString &id, const QString &source, const QString &target, bool enabled);
    void removeRule(const QString &id);
    void setEnabled(const QString &id, bool enabled);

    // Forward: does `input` match a rule's source pattern? If so, *mapped is
    // the real URL that should actually be loaded.
    bool resolve(const QUrl &input, QUrl *mapped) const;

    // Reverse: does `actual` (where the browser really is) match a rule's
    // target pattern? If so, *displaySource is the source-style URL to show
    // in the UI, clearly labeled as mapped rather than silently substituted.
    bool reverseResolve(const QUrl &actual, QUrl *displaySource) const;

signals:
    void rulesChanged();

private:
    void load();
    void save() const;
    static bool matchAndSubstitute(const QString &input, const QString &pattern,
                                    const QString &outputPattern, QString *result);
    // Fills in a scheme (https://) and an implicit trailing "/*" for patterns
    // typed as a bare domain (e.g. "smallbird.in"), so casual rule entry
    // matches the whole site the way a user expects, not just one exact URL.
    static QString normalizePattern(const QString &raw);

    QVector<RedirectRule> m_rules;
};
