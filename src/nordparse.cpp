#include "nordparse.h"

#include <QRegularExpression>

namespace NordParse {

QString Document::value(const QString &key) const
{
    return values.value(key);
}

bool Document::contains(const QString &key) const
{
    return values.contains(key);
}

Document parse(const QString &text)
{
    Document doc;
    QString lastKey;

    const QStringList lines = text.split(u'\n');
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }

        const bool indented = line.startsWith(u' ') || line.startsWith(u'\t');
        if (indented) {
            if (!lastKey.isEmpty()) {
                doc.blocks[lastKey].append(line.trimmed());
            }
            continue;
        }

        const int colon = line.indexOf(u':');
        if (colon <= 0) {
            lastKey.clear();
            continue;
        }

        const QString key = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();

        // Guard against prose and URLs being mistaken for keys. The account
        // footer carries lines like "Terms of Service - https://nordvpn.com/x",
        // where the first colon belongs to the URL scheme; a value starting
        // "//" is the giveaway.
        if (key.isEmpty() || key.length() > 40 || key.contains(u'/') || value.startsWith(QLatin1String("//"))) {
            lastKey.clear();
            continue;
        }

        doc.values.insert(key, value);
        lastKey = key;
    }

    return doc;
}

bool isBoolLike(const QString &value)
{
    const QString v = value.trimmed().toLower();
    return v == QLatin1String("enabled") || v == QLatin1String("disabled") || v == QLatin1String("on") || v == QLatin1String("off")
           || v == QLatin1String("true") || v == QLatin1String("false");
}

bool toBool(const QString &value, bool fallback)
{
    const QString v = value.trimmed().toLower();
    if (v == QLatin1String("enabled") || v == QLatin1String("on") || v == QLatin1String("true")) {
        return true;
    }
    if (v == QLatin1String("disabled") || v == QLatin1String("off") || v == QLatin1String("false")) {
        return false;
    }
    return fallback;
}

qint64 parseUptime(const QString &value)
{
    static const QRegularExpression unit(QStringLiteral("(\\d+)\\s*(day|days|hour|hours|minute|minutes|second|seconds)"));

    qint64 total = 0;
    auto it = unit.globalMatch(value);
    while (it.hasNext()) {
        const auto match = it.next();
        const qint64 amount = match.captured(1).toLongLong();
        const QString name = match.captured(2);
        if (name.startsWith(QLatin1String("day"))) {
            total += amount * 86400;
        } else if (name.startsWith(QLatin1String("hour"))) {
            total += amount * 3600;
        } else if (name.startsWith(QLatin1String("minute"))) {
            total += amount * 60;
        } else {
            total += amount;
        }
    }
    return total;
}

void parseTransfer(const QString &value, QString *received, QString *sent)
{
    static const QRegularExpression re(QStringLiteral("^(.*?)\\s+received,\\s*(.*?)\\s+sent$"));

    const auto match = re.match(value.trimmed());
    if (!match.hasMatch()) {
        return;
    }
    if (received) {
        *received = match.captured(1);
    }
    if (sent) {
        *sent = match.captured(2);
    }
}

bool isDaemonUnreachable(const QString &output)
{
    return output.contains(QLatin1String("Cannot reach System Daemon"), Qt::CaseInsensitive)
           || output.contains(QLatin1String("daemon is not running"), Qt::CaseInsensitive);
}

bool isLoggedOut(const QString &output)
{
    return output.contains(QLatin1String("not logged in"), Qt::CaseInsensitive);
}

QString errorSummary(const QString &output)
{
    const QStringList lines = output.split(u'\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        // The CLI appends marketing links to some errors; those are never the
        // useful part of the message.
        if (trimmed.startsWith(QLatin1String("http"))) {
            continue;
        }
        return trimmed;
    }
    return output.trimmed();
}

} // namespace NordParse
