#pragma once

#include "ptychannel.h"

#include <QObject>
#include <QQueue>
#include <QStringList>

#include <functional>

class QProcess;
class QTimer;

struct CliResult {
    bool ok = false; //!< process started, exited normally, exit code 0
    int exitCode = -1;
    QString output;  //!< stdout and stderr merged; the CLI uses both
    QString failure; //!< human readable reason when !ok
};

/*!
 * Runs `nordvpn` subcommands one at a time.
 *
 * The daemon does not appreciate concurrent clients, so every job goes through
 * a single queue. Jobs may be tagged so that a queued job of the same tag is
 * not enqueued twice -- used to keep status polling from piling up behind a
 * slow connect.
 */
class CliRunner : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const CliResult &)>;
    //! Called with everything printed so far, each time more output arrives.
    using ProgressCallback = std::function<void(const QString &)>;

    explicit CliRunner(QObject *parent = nullptr);

    /*! Enqueue a job. \a tag may be empty; if not, an already queued job with
     *  the same tag causes this one to be dropped. */
    void enqueue(const QStringList &args, Callback callback, int timeoutMs = 25000, const QString &tag = {});

    /*! Like enqueue(), but also reports output as it is produced. Needed for
     *  commands that print something useful and then keep running. */
    void enqueueStreaming(const QStringList &args, Callback callback, ProgressCallback progress, int timeoutMs = 25000, const QString &tag = {});

    /*! Like enqueue(), but feeds \a secret to the command over a pseudo-terminal
     *  once it is running, instead of passing it in \a args where every local
     *  process could read it back out of /proc. The command has to be one that
     *  prompts for the value; see PtyChannel. */
    void enqueueWithSecret(const QStringList &args, const QString &secret, Callback callback, int timeoutMs = 25000, const QString &tag = {});

    //! Kill whatever is running and drop anything queued.
    void cancelAll();

    bool isBusy() const;
    bool hasQueued(const QString &tag) const;

    /*! Strip terminal escape sequences and spinner redraws from CLI output. */
    static QString sanitize(const QString &raw);

Q_SIGNALS:
    void busyChanged();

private:
    struct Job {
        QStringList args;
        Callback callback;
        ProgressCallback progress;
        int timeoutMs = 25000;
        QString tag;
        QString secret; //!< written to the child's terminal, never to argv
    };

    void enqueueJob(Job job);
    void startNext();
    void finishCurrent(const CliResult &result);

    QQueue<Job> m_queue;
    Job m_current;
    QProcess *m_process = nullptr;
    QTimer *m_timeout = nullptr;
    PtyChannel m_pty;
    QString m_buffer;
    bool m_running = false;
    bool m_timedOut = false;
    bool m_cancelled = false;
};
