#include "ptychannel.h"

#include <QFile>
#include <QProcess>
#include <QTest>

/*!
 * `nordvpn login --token` will not read a token from a pipe, and putting it on
 * the command line publishes it to every process on the machine. These tests
 * pin down both halves of the way out: the child gets a real terminal, and the
 * secret reaches it without ever passing through argv.
 */
class TestPtyChannel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void childStdinIsATerminal();
    void deliversTheSecret();
    void keepsTheSecretOutOfProc();
    void writeFailsWhenClosed();
};

namespace {

/*! Start /bin/sh on \a script with a pty on its stdin. */
bool startWithPty(QProcess *process, PtyChannel *pty, const QString &script)
{
    process->setProcessChannelMode(QProcess::MergedChannels);
    if (!pty->attach(process)) {
        return false;
    }
    process->start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), script});
    return process->waitForStarted(5000);
}

} // namespace

void TestPtyChannel::childStdinIsATerminal()
{
    QProcess process;
    PtyChannel pty;

    QVERIFY(startWithPty(&process, &pty, QStringLiteral("if [ -t 0 ]; then echo TTY; else echo PIPE; fi")));
    QVERIFY(process.waitForFinished(5000));

    QCOMPARE(QString::fromUtf8(process.readAll()).trimmed(), QStringLiteral("TTY"));
}

void TestPtyChannel::deliversTheSecret()
{
    QProcess process;
    PtyChannel pty;

    // Reads one line the way the CLI's token prompt does, then proves what it got.
    QVERIFY(startWithPty(&process, &pty, QStringLiteral("read line; echo \"GOT:$line\"")));
    QVERIFY(pty.writeLine(QStringLiteral("s3cret-token")));
    QVERIFY(process.waitForFinished(5000));

    const QString output = QString::fromUtf8(process.readAll());
    QVERIFY2(output.contains(QStringLiteral("GOT:s3cret-token")), qPrintable(output));
    // Echo is off, so the only occurrence is the one the child printed itself.
    QCOMPARE(output.count(QStringLiteral("s3cret-token")), 1);
}

void TestPtyChannel::keepsTheSecretOutOfProc()
{
    QProcess process;
    PtyChannel pty;

    QVERIFY(startWithPty(&process, &pty, QStringLiteral("read line; echo \"GOT:$line\"")));

    // The child is parked on read(), so its command line is there to be looked
    // at -- exactly as another user on the machine would look at it.
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(process.processId()));
    QVERIFY(cmdline.open(QIODevice::ReadOnly));
    const QByteArray argv = cmdline.readAll();
    QVERIFY(!argv.isEmpty());

    QVERIFY(pty.writeLine(QStringLiteral("s3cret-token")));
    QVERIFY(process.waitForFinished(5000));

    QVERIFY2(!argv.contains("s3cret-token"), argv.constData());
    QVERIFY(QString::fromUtf8(process.readAll()).contains(QStringLiteral("GOT:s3cret-token")));
}

void TestPtyChannel::writeFailsWhenClosed()
{
    PtyChannel pty;
    QVERIFY(!pty.isOpen());
    QVERIFY(!pty.writeLine(QStringLiteral("nothing to write to")));
}

QTEST_GUILESS_MAIN(TestPtyChannel)
#include "tst_ptychannel.moc"
