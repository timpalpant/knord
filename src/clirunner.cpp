#include "clirunner.h"

#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>

namespace {

/*! The NordVPN client lives on the host and talks to a root daemon over
 *  /run/nordvpn/nordvpnd.sock, so inside a Flatpak it has to be reached through
 *  the host portal rather than run directly. */
bool runningInFlatpak()
{
    static const bool sandboxed = QFileInfo::exists(QStringLiteral("/.flatpak-info"));
    return sandboxed;
}

constexpr auto kLocaleEnv = "C.UTF-8";

} // namespace

CliRunner::CliRunner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
    , m_timeout(new QTimer(this))
{
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    // The parsers key off the English labels the CLI prints, so pin the locale
    // instead of inheriting whatever the desktop session uses. When going
    // through flatpak-spawn this is passed as --env too, since the host process
    // does not inherit our environment.
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QLatin1String(kLocaleEnv));
    env.insert(QStringLiteral("LANG"), QLatin1String(kLocaleEnv));
    m_process->setProcessEnvironment(env);

    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        m_timedOut = true;
        m_process->kill();
    });

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_buffer.append(QString::fromUtf8(m_process->readAllStandardOutput()));
        if (m_current.progress) {
            m_current.progress(sanitize(m_buffer));
        }
    });

    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        m_timeout->stop();
        m_buffer.append(QString::fromUtf8(m_process->readAll()));
        const QString output = sanitize(m_buffer);

        if (m_cancelled) {
            m_cancelled = false;
            m_buffer.clear();
            m_running = false;
            m_pty.close();
            m_current = Job{};
            Q_EMIT busyChanged();
            return;
        }

        CliResult result;
        result.exitCode = exitCode;
        result.output = output;

        if (m_timedOut) {
            result.failure = tr("The command timed out.");
        } else if (status != QProcess::NormalExit) {
            result.failure = tr("The nordvpn process crashed.");
        } else if (exitCode != 0) {
            result.failure = output.isEmpty() ? tr("nordvpn exited with code %1.").arg(exitCode) : output;
        } else {
            result.ok = true;
        }

        finishCurrent(result);
    });

    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return; // finished() covers the rest
        }
        m_timeout->stop();
        CliResult result;
        result.failure = runningInFlatpak() ? tr("Could not reach the nordvpn command on the host. Is the NordVPN client installed?")
                                            : tr("Could not run the nordvpn command. Is the NordVPN client installed?");
        finishCurrent(result);
    });
}

bool CliRunner::isBusy() const
{
    return m_running || !m_queue.isEmpty();
}

bool CliRunner::hasQueued(const QString &tag) const
{
    if (tag.isEmpty()) {
        return false;
    }
    if (m_running && m_current.tag == tag) {
        return true;
    }
    return std::any_of(m_queue.cbegin(), m_queue.cend(), [&tag](const Job &job) { return job.tag == tag; });
}

void CliRunner::enqueue(const QStringList &args, Callback callback, int timeoutMs, const QString &tag)
{
    enqueueStreaming(args, std::move(callback), {}, timeoutMs, tag);
}

void CliRunner::enqueueStreaming(const QStringList &args, Callback callback, ProgressCallback progress, int timeoutMs, const QString &tag)
{
    enqueueJob(Job{args, std::move(callback), std::move(progress), timeoutMs, tag, {}});
}

void CliRunner::enqueueWithSecret(const QStringList &args, const QString &secret, Callback callback, int timeoutMs, const QString &tag)
{
    enqueueJob(Job{args, std::move(callback), {}, timeoutMs, tag, secret});
}

void CliRunner::enqueueJob(Job job)
{
    if (hasQueued(job.tag)) {
        return;
    }

    const bool wasBusy = isBusy();
    m_queue.enqueue(std::move(job));
    if (!wasBusy) {
        Q_EMIT busyChanged();
    }
    startNext();
}

void CliRunner::cancelAll()
{
    m_queue.clear();
    if (m_running) {
        m_cancelled = true;
        m_timeout->stop();
        m_process->kill();
    }
}

void CliRunner::startNext()
{
    if (m_running || m_queue.isEmpty()) {
        return;
    }

    m_current = m_queue.dequeue();
    m_running = true;
    m_timedOut = false;
    m_buffer.clear();
    m_timeout->start(m_current.timeoutMs);

    // The process object is reused, so a terminal left over from a previous job
    // has to go before the next one starts, modifier and all.
    m_pty.close();
    m_process->setChildProcessModifier({});

    if (!m_current.secret.isEmpty() && !m_pty.attach(m_process)) {
        m_timeout->stop();
        CliResult result;
        result.failure = tr("Could not open a private channel to the nordvpn command.");
        finishCurrent(result);
        return;
    }

    if (runningInFlatpak()) {
        QStringList args{QStringLiteral("--host"),
                         QStringLiteral("--env=LC_ALL=%1").arg(QLatin1String(kLocaleEnv)),
                         QStringLiteral("--env=LANG=%1").arg(QLatin1String(kLocaleEnv)),
                         QStringLiteral("nordvpn")};
        args.append(m_current.args);
        m_process->start(QStringLiteral("flatpak-spawn"), args);
    } else {
        m_process->start(QStringLiteral("nordvpn"), m_current.args);
    }

    // The child is forked by the time start() returns, and the line sits in the
    // terminal's input queue until it asks for it.
    if (!m_current.secret.isEmpty()) {
        m_pty.writeLine(m_current.secret);
        m_current.secret.clear();
    }
}

void CliRunner::finishCurrent(const CliResult &result)
{
    if (!m_running) {
        return;
    }

    Job job = std::move(m_current);
    m_current = Job{};
    m_running = false;
    m_pty.close();

    if (job.callback) {
        job.callback(result);
    }

    if (m_queue.isEmpty()) {
        Q_EMIT busyChanged();
    } else {
        startNext();
    }
}

QString CliRunner::sanitize(const QString &raw)
{
    static const QRegularExpression ansi(QStringLiteral("\x1B\\[[0-9;?]*[A-Za-z]"));

    QString text = raw;
    text.remove(ansi);

    // Progress spinners overwrite the current line with carriage returns; only
    // the last write of each line is the final state.
    QStringList lines;
    const QStringList rawLines = text.split(u'\n');
    lines.reserve(rawLines.size());
    for (const QString &line : rawLines) {
        QString resolved = line.section(u'\r', -1);
        while (resolved.endsWith(u' ') || resolved.endsWith(u'\t')) {
            resolved.chop(1);
        }
        lines.append(resolved);
    }

    while (!lines.isEmpty() && lines.constFirst().trimmed().isEmpty()) {
        lines.removeFirst();
    }
    while (!lines.isEmpty() && lines.constLast().trimmed().isEmpty()) {
        lines.removeLast();
    }

    return lines.join(u'\n');
}
