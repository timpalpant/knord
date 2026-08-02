#pragma once

#include <QString>

class QProcess;

/*!
 * A pseudo-terminal that hands a secret to a child process on stdin, so that it
 * never appears in the process table.
 *
 * Anything passed as an argument is world readable through /proc/<pid>/cmdline
 * for as long as the process lives, which is not where an account credential
 * belongs. `nordvpn login --token` reads the token from stdin when it is given
 * no argument -- but it refuses a plain pipe ("stdin is not a terminal"), so a
 * QProcess pipe will not do and fd 0 has to be a real tty.
 *
 * Linux only, which is also all this application targets.
 */
class PtyChannel
{
public:
    PtyChannel() = default;
    ~PtyChannel();

    PtyChannel(const PtyChannel &) = delete;
    PtyChannel &operator=(const PtyChannel &) = delete;

    /*! Allocate a pty pair and point \a process's stdin at the slave end. The
     *  process must not have been started yet. Returns false and leaves
     *  \a process untouched if the pair could not be set up. */
    bool attach(QProcess *process);

    /*! Send \a secret, terminated by a newline, to the child. Safe to call as
     *  soon as the process has started: the terminal buffers the line until the
     *  child reads it, and neither the prompt nor the switch to no-echo mode
     *  discards it. */
    bool writeLine(const QString &secret);

    /*! Release both ends. Called from the destructor; also safe to call twice. */
    void close();

    bool isOpen() const;

private:
    int m_master = -1;
    int m_slave = -1;
};
