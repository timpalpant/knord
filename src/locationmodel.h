#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QStringList>

/*!
 * A flat, filterable list of places you can connect to: countries, the cities
 * of one country, or specialty server groups.
 *
 * Filtering is done in the model rather than through a proxy so that QML can
 * bind straight to `count` and show an empty-state without extra plumbing.
 */
class LocationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Obtained from the NordVpn singleton")

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1, //!< raw CLI token, e.g. "United_States"
        DisplayRole,                 //!< "United States"
        FlagRole,                    //!< flag emoji, may be empty
        ConnectArgsRole,             //!< argv tail for `nordvpn connect`
        HasCitiesRole,               //!< true for countries
    };
    Q_ENUM(Roles)

    struct Entry {
        QString name;
        QString display;
        QString flag;
        QStringList connectArgs;
        bool hasCities = false;

        bool operator==(const Entry &other) const = default;
    };

    explicit LocationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool isEmpty() const;
    QString filter() const;
    void setFilter(const QString &filter);

    void setEntries(const QList<Entry> &entries);
    void clear();

Q_SIGNALS:
    void filterChanged();
    void countChanged();

private:
    void rebuild();

    QList<Entry> m_all;
    QList<Entry> m_visible;
    QString m_filter;
};
