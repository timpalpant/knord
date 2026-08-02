#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

/*!
 * Helpers for reading the `Key: Value` blocks that the nordvpn CLI prints.
 */
namespace NordParse {

struct Document {
    //! "Kill Switch" -> "disabled"
    QMap<QString, QString> values;
    //! "Allowlisted ports" -> {"22 (TCP)"}: indented lines below a bare key.
    QMap<QString, QStringList> blocks;

    QString value(const QString &key) const;
    bool contains(const QString &key) const;
};

Document parse(const QString &text);

/*! Reads enabled/disabled, on/off, true/false. Returns \a fallback otherwise. */
bool toBool(const QString &value, bool fallback = false);
bool isBoolLike(const QString &value);

/*! "20 hours 19 minutes 44 seconds" -> 73184 */
qint64 parseUptime(const QString &value);

/*! "507.05 GiB received, 17.07 GiB sent" -> {"507.05 GiB", "17.07 GiB"} */
void parseTransfer(const QString &value, QString *received, QString *sent);

/*! True when the CLI reports it cannot talk to nordvpnd. */
bool isDaemonUnreachable(const QString &output);

/*! True when the CLI reports that no account is logged in. */
bool isLoggedOut(const QString &output);

/*! Best-effort single-line error message from a failed command's output. */
QString errorSummary(const QString &output);

} // namespace NordParse
