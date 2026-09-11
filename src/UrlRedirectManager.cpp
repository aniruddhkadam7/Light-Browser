#include "UrlRedirectManager.h"

#include <QSettings>
#include <QUuid>

UrlRedirectManager::UrlRedirectManager(QObject *parent)
    : QObject(parent)
{
    load();
}

void UrlRedirectManager::load()
{
    QSettings settings;
    const int size = settings.beginReadArray("UrlRedirectRules");
    m_rules.clear();
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        RedirectRule rule;
        rule.id = settings.value("id").toString();
        rule.source = settings.value("source").toString();
        rule.target = settings.value("target").toString();
        rule.enabled = settings.value("enabled", true).toBool();
        if (!rule.id.isEmpty())
            m_rules.append(rule);
    }
    settings.endArray();
}

void UrlRedirectManager::save() const
{
    QSettings settings;
    settings.beginWriteArray("UrlRedirectRules");
    for (int i = 0; i < m_rules.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("id", m_rules[i].id);
        settings.setValue("source", m_rules[i].source);
        settings.setValue("target", m_rules[i].target);
        settings.setValue("enabled", m_rules[i].enabled);
    }
    settings.endArray();
}

QVector<RedirectRule> UrlRedirectManager::rules() const
{
    return m_rules;
}

void UrlRedirectManager::addRule(const QString &source, const QString &target, bool enabled)
{
    RedirectRule rule;
    rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.source = source;
    rule.target = target;
    rule.enabled = enabled;
    m_rules.append(rule);
    save();
    emit rulesChanged();
}

void UrlRedirectManager::updateRule(const QString &id, const QString &source, const QString &target,
                                    bool enabled)
{
    for (RedirectRule &rule : m_rules) {
        if (rule.id == id) {
            rule.source = source;
            rule.target = target;
            rule.enabled = enabled;
            save();
            emit rulesChanged();
            return;
        }
    }
}

void UrlRedirectManager::removeRule(const QString &id)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == id) {
            m_rules.removeAt(i);
            save();
            emit rulesChanged();
            return;
        }
    }
}

void UrlRedirectManager::setEnabled(const QString &id, bool enabled)
{
    for (RedirectRule &rule : m_rules) {
        if (rule.id == id) {
            rule.enabled = enabled;
            save();
            emit rulesChanged();
            return;
        }
    }
}

// Matches `input` against `pattern` (at most one '*' wildcard, capturing an
// arbitrary run of characters — path/query/fragment included). On a match,
// substitutes the captured text into the corresponding '*' position of
// `outputPattern` (or uses it verbatim if it has no wildcard of its own).
bool UrlRedirectManager::matchAndSubstitute(const QString &input, const QString &pattern,
                                             const QString &outputPattern, QString *result)
{
    const int starIdx = pattern.indexOf(QLatin1Char('*'));
    if (starIdx < 0) {
        if (input != pattern)
            return false;
        if (result)
            *result = outputPattern;
        return true;
    }

    const QString prefix = pattern.left(starIdx);
    const QString suffix = pattern.mid(starIdx + 1);

    // A bare-domain navigation like QUrl("https://site.com") serializes with
    // no trailing slash, while a normalized pattern's prefix always ends in
    // one ("https://site.com/*" -> prefix "https://site.com/"). Without this,
    // the most common case — mapping a whole bare domain — would never match.
    QString effectiveInput = input;
    if (!effectiveInput.startsWith(prefix) && prefix.endsWith(QLatin1Char('/'))
        && (effectiveInput + QLatin1Char('/')).startsWith(prefix)) {
        effectiveInput += QLatin1Char('/');
    }

    if (!effectiveInput.startsWith(prefix) || !effectiveInput.endsWith(suffix))
        return false;
    if (effectiveInput.length() < prefix.length() + suffix.length())
        return false;

    const QString captured = effectiveInput.mid(prefix.length(),
                                                  effectiveInput.length() - prefix.length() - suffix.length());

    if (result) {
        const int outStar = outputPattern.indexOf(QLatin1Char('*'));
        *result = (outStar < 0) ? outputPattern
                                 : outputPattern.left(outStar) + captured + outputPattern.mid(outStar + 1);
    }
    return true;
}

QString UrlRedirectManager::normalizePattern(const QString &raw)
{
    QString p = raw.trimmed();
    if (!p.contains(QStringLiteral("://")))
        p.prepend(QStringLiteral("https://"));
    if (!p.contains(QLatin1Char('*'))) {
        if (!p.endsWith(QLatin1Char('/')))
            p += QLatin1Char('/');
        p += QLatin1Char('*');
    }
    return p;
}

bool UrlRedirectManager::resolve(const QUrl &input, QUrl *mapped) const
{
    const QString inputStr = input.toString();
    for (const RedirectRule &rule : m_rules) {
        if (!rule.enabled)
            continue;
        QString out;
        if (matchAndSubstitute(inputStr, normalizePattern(rule.source), normalizePattern(rule.target),
                                &out)) {
            if (mapped)
                *mapped = QUrl(out);
            return true;
        }
    }
    return false;
}

bool UrlRedirectManager::reverseResolve(const QUrl &actual, QUrl *displaySource) const
{
    const QString actualStr = actual.toString();
    for (const RedirectRule &rule : m_rules) {
        if (!rule.enabled)
            continue;
        QString out;
        if (matchAndSubstitute(actualStr, normalizePattern(rule.target), normalizePattern(rule.source),
                                &out)) {
            if (displaySource)
                *displaySource = QUrl(out);
            return true;
        }
    }
    return false;
}
