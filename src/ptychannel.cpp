#include "ptychannel.h"

#include <QProcess>

#include <cerrno>
#include <cstdlib>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

PtyChannel::~PtyChannel()
{
    close();
}

bool PtyChannel::isOpen() const
{
    return m_master >= 0;
}

bool PtyChannel::attach(QProcess *process)
{
    if (!process || isOpen()) {
        return false;
    }

    const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0) {
        return false;
    }
    // Both ends stay out of every other child this runner spawns; the dup2 below
    // is what puts the slave on fd 0, and dup2 clears the flag on the new fd.
    ::fcntl(master, F_SETFD, FD_CLOEXEC);

    if (::grantpt(master) != 0 || ::unlockpt(master) != 0) {
        ::close(master);
        return false;
    }

    const char *name = ::ptsname(master);
    if (!name) {
        ::close(master);
        return false;
    }

    // Opened here rather than inside the child hook, which then needs nothing
    // but dup2 and stays async-signal-safe.
    const int slave = ::open(name, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (slave < 0) {
        ::close(master);
        return false;
    }

    // Without this the terminal echoes the secret straight back to the master
    // end. The reader turns echo off for itself as well, but only once it has
    // reached its prompt, which is after we have written.
    struct termios tio;
    if (::tcgetattr(slave, &tio) == 0) {
        tio.c_lflag &= ~static_cast<tcflag_t>(ECHO | ECHOE | ECHOK | ECHONL);
        ::tcsetattr(slave, TCSANOW, &tio);
    }

    m_master = master;
    m_slave = slave;

    // Runs in the forked child, between fork and exec. No controlling terminal
    // is claimed: isatty() and the termios calls the reader makes only need the
    // descriptor itself, and leaving the session alone keeps QProcess's own
    // process handling intact.
    process->setChildProcessModifier([slave] {
        if (::dup2(slave, STDIN_FILENO) < 0) {
            ::_exit(1);
        }
    });

    return true;
}

bool PtyChannel::writeLine(const QString &secret)
{
    if (m_master < 0) {
        return false;
    }

    QByteArray payload = secret.toUtf8();
    payload.append('\n');

    qsizetype written = 0;
    bool ok = true;
    while (written < payload.size()) {
        const ssize_t n = ::write(m_master, payload.constData() + written, size_t(payload.size() - written));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            ok = false;
            break;
        }
        written += n;
    }

    // toUtf8() handed back a buffer of its own, so this really does clear the
    // only copy we made. Best effort: it says nothing about the QString itself.
    payload.fill('\0');
    return ok;
}

void PtyChannel::close()
{
    if (m_slave >= 0) {
        ::close(m_slave);
        m_slave = -1;
    }
    if (m_master >= 0) {
        ::close(m_master);
        m_master = -1;
    }
}
