#include "NewTabPage.h"

#include "HistoryManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace {

// "accounts.google.com" / "www.apple.com" -> "Google" / "Apple": drop a
// leading "www", then take the registrable-domain label so a tile reads as
// the site's name instead of a truncated full hostname.
QString brandName(const QString &host)
{
    if (host.isEmpty())
        return QStringLiteral("?");

    QStringList parts = host.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() > 1 && parts.first().compare(QLatin1String("www"), Qt::CaseInsensitive) == 0)
        parts.removeFirst();

    const QString base = parts.size() >= 2 ? parts.at(parts.size() - 2)
                          : !parts.isEmpty() ? parts.first()
                                              : host;
    if (base.isEmpty())
        return host;
    return base.at(0).toUpper() + base.mid(1);
}

QString tileHtml(const HistoryEntry &entry)
{
    const QString host = entry.url.host();
    const QString name = brandName(host);
    const QString tooltip = entry.title.isEmpty() ? host : entry.title;

    const QString favicon = entry.favicon.isEmpty()
        ? QStringLiteral("<span class=\"favicon-fallback\">%1</span>").arg(QString(name.at(0)).toHtmlEscaped())
        : QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"24\" height=\"24\">")
              .arg(QString::fromLatin1(entry.favicon.toBase64()));

    return QStringLiteral(
               "<a class=\"tile\" href=\"%1\" title=\"%2\">"
               "<span class=\"favicon\">%3</span>"
               "<span class=\"label\">%4</span>"
               "</a>")
        .arg(entry.url.toString().toHtmlEscaped(), tooltip.toHtmlEscaped(), favicon, name.toHtmlEscaped());
}

// Serialized once per page build and matched against locally, in JS, as the
// user types — no round trip back into C++ and no network request, so
// history/URL suggestions are instant and stay entirely local.
QString historyJson(const QVector<HistoryEntry> &entries)
{
    QJsonArray arr;
    for (const HistoryEntry &entry : entries) {
        QJsonObject o;
        o["url"] = entry.url.toString();
        o["title"] = entry.title;
        arr.append(o);
    }
    QString json = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    // Defuse a "</script>" that could appear inside a visited page's title
    // or URL from prematurely closing our embedding <script> block.
    json.replace(QLatin1String("</"), QLatin1String("<\\/"));
    return json;
}

// Safe to drop into a single-quoted JS string literal in the template below.
QString jsStringLiteralEscape(QString text)
{
    text.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    text.replace(QLatin1Char('\''), QLatin1String("\\'"));
    text.replace(QLatin1String("</"), QLatin1String("<\\/"));
    return text;
}

} // namespace

QString NewTabPage::build(const QVector<HistoryEntry> &mostVisited, const QVector<HistoryEntry> &recentHistory,
                           const QString &searchProviderName)
{
    QString tiles;
    for (const HistoryEntry &entry : mostVisited)
        tiles += tileHtml(entry);

    // Entirely self-contained: no <link>/<script src> to any external host,
    // no web fonts, no images — just system fonts and inline SVG/CSS. The
    // page never issues a network request on its own; the omnibox-style
    // suggestion list below is matched against @@HISTORY_JSON@@, which is
    // just this profile's own local history, already embedded at build time.
    QString html = QStringLiteral(R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>New Tab</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  html, body {
    margin: 0; height: 100%;
    background: #202124;
    color: #e3e3e3;
    font-family: -apple-system, "Segoe UI", Roboto, Arial, sans-serif;
  }
  body {
    display: flex;
    flex-direction: column;
    align-items: center;
    padding-top: 22vh;
  }
  form { width: 100%; max-width: 560px; padding: 0 24px; }
  .search-wrap { position: relative; }
  .search-box {
    display: flex;
    align-items: center;
    gap: 12px;
    width: 100%;
    height: 48px;
    padding: 0 18px;
    border-radius: 24px;
    background: #303134;
    border: 1px solid #3c4043;
    position: relative;
    z-index: 2;
    transition: border-radius 0.1s;
  }
  .search-box:focus-within { border-color: #8ab4f8; }
  .search-wrap.open .search-box {
    border-bottom-left-radius: 6px;
    border-bottom-right-radius: 6px;
    border-bottom-color: #3c4043;
  }
  .search-box svg { flex: none; opacity: 0.7; }
  .search-box input {
    flex: 1;
    border: none;
    outline: none;
    background: transparent;
    color: #e3e3e3;
    font-size: 16px;
    min-width: 0;
  }
  .search-box input::placeholder { color: #9aa0a6; }
  .suggestions {
    position: absolute;
    left: 0; right: 0;
    top: 47px;
    padding: 8px 0;
    background: #303134;
    border: 1px solid #3c4043;
    border-top: none;
    border-radius: 0 0 24px 24px;
    box-shadow: 0 16px 32px rgba(0,0,0,0.4);
    overflow: hidden;
    z-index: 1;
  }
  .suggestion-row {
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 10px 20px;
    cursor: pointer;
  }
  .suggestion-row.selected, .suggestion-row:hover { background: #3c4043; }
  .suggestion-row .icon {
    flex: none;
    width: 30px; height: 30px;
    border-radius: 50%;
    display: flex; align-items: center; justify-content: center;
    background: #3c4043;
  }
  .suggestion-row.type-search .icon { background: rgba(138,180,248,0.16); }
  .suggestion-row.type-url .icon { background: rgba(129,201,149,0.16); }
  .suggestion-row .text {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    justify-content: center;
  }
  .suggestion-row .primary {
    font-size: 14.5px;
    color: #e3e3e3;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .suggestion-row .secondary {
    font-size: 12px;
    color: #9aa0a6;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    margin-top: 1px;
  }
  .suggestion-heading {
    padding: 10px 20px 6px;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: #80868b;
  }
  .suggestion-heading:not(:first-child) {
    margin-top: 4px;
    border-top: 1px solid #3c4043;
    padding-top: 12px;
  }
  .most-visited {
    margin-top: 44px;
    display: grid;
    grid-template-columns: repeat(4, 88px);
    gap: 20px 16px;
    justify-content: center;
    max-width: 560px;
    padding: 0 24px;
  }
  .tile {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 8px;
    text-decoration: none;
    color: inherit;
  }
  .tile .favicon {
    width: 44px; height: 44px;
    border-radius: 50%;
    background: #303134;
    color: #e3e3e3;
    display: flex;
    align-items: center;
    justify-content: center;
    overflow: hidden;
  }
  .tile .favicon-fallback { font-size: 18px; font-weight: 600; }
  .tile:hover .favicon { background: #3c4043; }
  .tile .label {
    font-size: 12px;
    color: #bdc1c6;
    max-width: 88px;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
</style>
</head>
<body>
  <form id="searchForm" autocomplete="off">
    <div class="search-wrap">
      <div class="search-box">
        <svg width="20" height="20" viewBox="0 0 48 48">
          <path fill="#4285F4" d="M45.12 24.5c0-1.56-.14-3.06-.4-4.5H24v8.51h11.84c-.51 2.75-2.06 5.08-4.39 6.64v5.52h7.11c4.16-3.83 6.56-9.47 6.56-16.17z"/>
          <path fill="#34A853" d="M24 46c5.94 0 10.92-1.97 14.56-5.33l-7.11-5.52c-1.97 1.32-4.49 2.1-7.45 2.1-5.73 0-10.58-3.87-12.31-9.07H4.34v5.7C7.96 41.07 15.4 46 24 46z"/>
          <path fill="#FBBC05" d="M11.69 28.18C11.25 26.86 11 25.45 11 24s.25-2.86.69-4.18v-5.7H4.34C2.85 17.09 2 20.45 2 24s.85 6.91 2.34 9.88l7.35-5.7z"/>
          <path fill="#EA4335" d="M24 10.75c3.23 0 6.13 1.11 8.41 3.29l6.31-6.31C34.91 4.18 29.93 2 24 2 15.4 2 7.96 6.93 4.34 14.12l7.35 5.7c1.73-5.2 6.58-9.07 12.31-9.07z"/>
        </svg>
        <input id="searchInput" type="text" placeholder="Search or enter URL" autocomplete="off" spellcheck="false">
        <svg class="search-box-trailing-icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#9aa0a6" stroke-width="2">
          <circle cx="11" cy="11" r="7"/>
          <line x1="21" y1="21" x2="16.65" y2="16.65"/>
        </svg>
      </div>
      <div id="suggestions" class="suggestions" hidden></div>
    </div>
  </form>
  <div class="most-visited">@@TILES@@</div>
)HTML") + QStringLiteral(R"HTML(
  <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
  <script>
  (function () {
    var historyData = @@HISTORY_JSON@@;
    var providerName = '@@PROVIDER_NAME_JS@@';

    var input = document.getElementById('searchInput');
    var box = document.getElementById('suggestions');
    var items = [];
    var selectedIndex = -1;
    var remoteSuggestions = []; // completions for `remoteQuery`, from the configured provider
    var remoteQuery = null;
    var debounceTimer = null;
    var omnibox = null; // wired up below once/if the QWebChannel connects

    var hostLike = /^([\w-]+\.)+[a-zA-Z]{2,}(:\d+)?(\/.*)?$/;
    var ipLike = /^(\d{1,3}\.){3}\d{1,3}(:\d+)?(\/.*)?$/;

    function looksLikeUrl(text) {
      if (/^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(text))
        return true;
      if (text.indexOf(' ') !== -1)
        return false;
      return hostLike.test(text) || ipLike.test(text) || text === 'localhost' || /^localhost:/.test(text);
    }

    function escapeHtml(s) {
      return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    }

    var globeIcon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#81c995" stroke-width="1.8">'
      + '<circle cx="12" cy="12" r="9"/><path d="M3 12h18"/>'
      + '<path d="M12 3c2.6 2.4 2.6 15.6 0 18M12 3c-2.6 2.4 -2.6 15.6 0 18"/></svg>';
    var searchIcon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#8ab4f8" stroke-width="2">'
      + '<circle cx="11" cy="11" r="7"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>';
    var clockIcon = '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="#9aa0a6" stroke-width="1.8">'
      + '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3.5 2" stroke-linecap="round"/></svg>';

    // Search suggestions come first (the literal typed text, then remote
    // completions from the configured provider when available), history
    // matches are secondary and capped smaller, and a direct-URL match (when
    // the text looks like a domain/address) leads everything — matching how
    // Chrome's own omnibox orders a recognizable address above search.
    function buildSuggestions(query) {
      var list = [];
      var trimmed = query.trim();
      if (trimmed.length === 0)
        return list;

      var isUrl = looksLikeUrl(trimmed);
      if (isUrl) {
        var target = /^[a-zA-Z][a-zA-Z0-9+.-]*:\/\//.test(trimmed) ? trimmed : ('https://' + trimmed);
        list.push({ type: 'url', label: target, sub: 'Visit this address', value: 'url:' + target, icon: globeIcon });
      }

      // Searching a provider for a literal URL is redundant with the direct-
      // address row above, so skip the literal text as a search entry then.
      var searchTexts = isUrl ? [] : [trimmed];
      if (remoteQuery === trimmed) {
        for (var r = 0; r < remoteSuggestions.length; r++) {
          var s = remoteSuggestions[r];
          if (s && searchTexts.indexOf(s) === -1)
            searchTexts.push(s);
        }
      }
      searchTexts.slice(0, 5).forEach(function (text) {
        list.push({
          type: 'search',
          label: text,
          sub: '',
          value: 'search:' + text,
          icon: searchIcon
        });
      });

      var q = trimmed.toLowerCase();
      var scored = [];
      for (var i = 0; i < historyData.length; i++) {
        var e = historyData[i];
        var url = e.url || '';
        var title = e.title || '';
        var urlIdx = url.toLowerCase().indexOf(q);
        var titleIdx = title.toLowerCase().indexOf(q);
        if (urlIdx === -1 && titleIdx === -1)
          continue;
        var score = (urlIdx === 0 || titleIdx === 0) ? 2 : 1;
        scored.push({ entry: e, score: score, order: i });
      }
      scored.sort(function (a, b) {
        if (b.score !== a.score) return b.score - a.score;
        return a.order - b.order; // historyData is most-recent-first already
      });
      for (var j = 0; j < Math.min(3, scored.length); j++) {
        var entry = scored[j].entry;
        list.push({
          type: 'history',
          label: entry.title && entry.title.length ? entry.title : entry.url,
          sub: entry.url,
          value: entry.url,
          icon: clockIcon
        });
      }

      return list;
    }

    // Shown the moment the box gets focus, before any typing — Chrome/Brave
    // both do this from local history too (no query text yet to search
    // for, and no network request either).
    function buildEmptySuggestions() {
      var list = [];
      for (var i = 0; i < Math.min(8, historyData.length); i++) {
        var entry = historyData[i];
        list.push({
          type: 'recent',
          label: entry.title && entry.title.length ? entry.title : entry.url,
          sub: entry.url,
          value: entry.url,
          icon: clockIcon
        });
      }
      return list;
    }

    function sectionHeading(type) {
      if (type === 'search') return 'Search ' + providerName;
      if (type === 'history') return 'From history';
      if (type === 'recent') return 'Frequently visited';
      return null;
    }

    function updateSelectionClasses() {
      var rows = box.querySelectorAll('.suggestion-row');
      for (var i = 0; i < rows.length; i++)
        rows[i].classList.toggle('selected', i === selectedIndex);
    }

    var wrap = document.querySelector('.search-wrap');

    function render() {
      if (items.length === 0) {
        box.hidden = true;
        box.innerHTML = '';
        wrap.classList.remove('open');
        return;
      }
      wrap.classList.add('open');
      var html = '';
      var lastType = null;
      for (var i = 0; i < items.length; i++) {
        var it = items[i];
        if (it.type !== lastType) {
          var heading = sectionHeading(it.type);
          if (heading)
            html += '<div class="suggestion-heading">' + heading + '</div>';
          lastType = it.type;
        }
        html += '<div class="suggestion-row type-' + it.type + '" data-index="' + i + '">'
          + '<span class="icon">' + it.icon + '</span>'
          + '<span class="text">'
          + '<span class="primary">' + escapeHtml(it.label) + '</span>'
          + (it.sub ? '<span class="secondary">' + escapeHtml(it.sub) + '</span>' : '')
          + '</span>'
          + '</div>';
      }
      box.innerHTML = html;
      box.hidden = false;

      var rows = box.querySelectorAll('.suggestion-row');
      rows.forEach(function (row) {
        row.addEventListener('mouseenter', function () {
          selectedIndex = parseInt(row.getAttribute('data-index'), 10);
          updateSelectionClasses();
        });
        // mousedown (not click) fires before the input's blur handler would
        // close the dropdown, and preventDefault keeps focus in the input.
        row.addEventListener('mousedown', function (e) {
          e.preventDefault();
          var idx = parseInt(row.getAttribute('data-index'), 10);
          commit(items[idx]);
        });
      });
    }

    function closeSuggestions() {
      items = [];
      selectedIndex = -1;
      box.hidden = true;
      box.innerHTML = '';
      wrap.classList.remove('open');
    }

    function commit(item) {
      closeSuggestions();
      location.href = 'ltnav:' + encodeURIComponent(item.value);
    }

    function refresh() {
      var trimmed = input.value.trim();
      items = trimmed.length === 0 ? buildEmptySuggestions() : buildSuggestions(input.value);
      selectedIndex = -1;
      render();
    }

    // QWebChannel connects asynchronously and only exists at all while this
    // is the New Tab page (see WebPage::attachOmniboxChannel/detach); if it
    // never connects (channel torn down, transport missing) remote
    // suggestions just never arrive and buildSuggestions() above already
    // falls back to the literal query + local history alone.
    if (typeof qt !== 'undefined' && qt.webChannelTransport) {
      new QWebChannel(qt.webChannelTransport, function (channel) {
        omnibox = channel.objects.omnibox;
        omnibox.suggestionsReady.connect(function (query, suggestions) {
          if (query !== input.value.trim())
            return; // stale reply for text the user has since changed/cleared
          remoteQuery = query;
          remoteSuggestions = suggestions || [];
          refresh();
        });
      });
    }

    // Chrome/Brave-style: suggestions appear the instant the user actually
    // clicks the box — but never just because the New Tab page loaded.
    // Deliberately keyed off 'mousedown' rather than 'focus': there's no
    // <input autofocus> and nothing here calls .focus() on load, but
    // QtWebEngine can still hand the page's first focusable element DOM
    // focus purely because the containing view itself gained widget focus
    // (no click involved at all) — a plain 'focus' listener pops the
    // dropdown open for that too. A real click always fires 'mousedown'
    // immediately before 'focus'; that automatic assignment never does.
    input.addEventListener('mousedown', function () {
      refresh();
    });

    input.addEventListener('input', function () {
      refresh();

      if (debounceTimer)
        clearTimeout(debounceTimer);
      var value = input.value;
      var trimmedValue = value.trim();
      // Fetching search completions for a literal URL isn't useful and is
      // exactly the kind of unnecessary background request to avoid.
      if (!omnibox || trimmedValue.length === 0 || looksLikeUrl(trimmedValue))
        return;
      // The debounce delay alone (no minimum-length gate) is what keeps this
      // to roughly one request per pause in typing rather than one per
      // keystroke — aborting any still-in-flight request (see
      // OmniboxBridge::requestSuggestions) covers the rest.
      debounceTimer = setTimeout(function () {
        if (input.value === value)
          omnibox.requestSuggestions(value.trim());
      }, 250);
    });

    input.addEventListener('keydown', function (e) {
      if (e.key === 'ArrowDown') {
        if (items.length === 0) return;
        e.preventDefault();
        selectedIndex = (selectedIndex + 1) % items.length;
        updateSelectionClasses();
      } else if (e.key === 'ArrowUp') {
        if (items.length === 0) return;
        e.preventDefault();
        selectedIndex = (selectedIndex - 1 + items.length) % items.length;
        updateSelectionClasses();
      } else if (e.key === 'Escape') {
        if (items.length > 0) {
          e.preventDefault();
          e.stopPropagation();
          closeSuggestions();
        }
      } else if (e.key === 'Enter') {
        if (selectedIndex >= 0 && items[selectedIndex]) {
          e.preventDefault();
          commit(items[selectedIndex]);
        }
        // else: no selection — let the form's submit handler below run the
        // plain-text search/URL, same as before this feature existed.
      }
    });

    input.addEventListener('blur', function () {
      // Deferred so a mousedown on a suggestion row still lands first.
      setTimeout(closeSuggestions, 150);
    });

    document.getElementById('searchForm').addEventListener('submit', function (e) {
      e.preventDefault();
      var value = input.value.trim();
      if (value.length > 0)
        location.href = 'ltnav:' + encodeURIComponent(value);
      closeSuggestions();
    });
  })();
  </script>
</body>
</html>
)HTML");

    html.replace(QLatin1String("@@TILES@@"), tiles);
    html.replace(QLatin1String("@@HISTORY_JSON@@"), historyJson(recentHistory));
    html.replace(QLatin1String("@@PROVIDER_NAME_JS@@"), jsStringLiteralEscape(searchProviderName));
    return html;
}

QString NewTabPage::buildIncognito(const QString &searchProviderName)
{
    // Same page, just with nothing local to show it — no Most Visited
    // tiles, no history-derived suggestions in the omnibox (search
    // suggestions from the configured provider still work).
    QString html = build({}, {}, searchProviderName);

    // Chrome/Brave's own incognito framing — glasses-and-hat icon, heading,
    // and the "what this does/doesn't do" line — is the strongest,
    // most recognizable signal that this is an incognito tab, so it's
    // inserted right above the (otherwise identical) search box.
    const QString incognitoHeader = QStringLiteral(R"HTML(  <div class="incognito-header">
    <svg width="60" height="60" viewBox="0 0 64 64" fill="none">
      <rect x="4" y="26" width="56" height="8" rx="4" fill="#c7b8ec"/>
      <path d="M20 26 C22 12 42 12 44 26" stroke="#c7b8ec" stroke-width="4" fill="none" stroke-linecap="round"/>
      <circle cx="19" cy="41" r="9" stroke="#c7b8ec" stroke-width="3"/>
      <circle cx="45" cy="41" r="9" stroke="#c7b8ec" stroke-width="3"/>
      <line x1="28" y1="41" x2="36" y2="41" stroke="#c7b8ec" stroke-width="3"/>
    </svg>
    <h1>You've gone Incognito</h1>
    <p>Pages you view here won't be saved to your history or stick around after you close this window.</p>
  </div>
  )HTML");

    html.replace(QStringLiteral("<form id=\"searchForm\""), incognitoHeader + QStringLiteral("<form id=\"searchForm\""));

    const QString incognitoCss = QStringLiteral(R"CSS(
  .incognito-header { display:flex; flex-direction:column; align-items:center; text-align:center; max-width:520px; padding:0 24px; margin-bottom:8px; }
  .incognito-header h1 { font-size:22px; font-weight:600; margin:14px 0 12px; color:#ece6f7; }
  .incognito-header p { font-size:13px; line-height:1.6; color:#bdb4d1; margin:0; }
</style>)CSS");
    html.replace(QStringLiteral("</style>"), incognitoCss);

    // Distinct near-black, purple-tinted background — same trick Chrome/
    // Brave use so an incognito tab is unmistakable from a regular one.
    html.replace(QStringLiteral("    background: #202124;"), QStringLiteral("    background: #150e20;"));

    return html;
}
