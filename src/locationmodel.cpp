#include "locationmodel.h"

LocationModel::LocationModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int LocationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_visible.size();
}

QVariant LocationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visible.size()) {
        return {};
    }

    const Entry &entry = m_visible.at(index.row());
    switch (role) {
    case NameRole:
        return entry.name;
    case DisplayRole:
    case Qt::DisplayRole:
        return entry.display;
    case FlagRole:
        return entry.flag;
    case ConnectArgsRole:
        return entry.connectArgs;
    case HasCitiesRole:
        return entry.hasCities;
    default:
        return {};
    }
}

QHash<int, QByteArray> LocationModel::roleNames() const
{
    return {
        {NameRole, QByteArrayLiteral("name")},
        // Deliberately not "display": QQC2.ItemDelegate already declares a
        // FINAL property of that name, and a clashing role breaks the delegate.
        {DisplayRole, QByteArrayLiteral("label")},
        {FlagRole, QByteArrayLiteral("flag")},
        {ConnectArgsRole, QByteArrayLiteral("connectArgs")},
        {HasCitiesRole, QByteArrayLiteral("hasCities")},
    };
}

bool LocationModel::isEmpty() const
{
    return m_visible.isEmpty();
}

QString LocationModel::filter() const
{
    return m_filter;
}

void LocationModel::setFilter(const QString &filter)
{
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    Q_EMIT filterChanged();
    rebuild();
}

void LocationModel::setEntries(const QList<Entry> &entries)
{
    m_all = entries;
    rebuild();
}

void LocationModel::clear()
{
    m_all.clear();
    rebuild();
}

void LocationModel::rebuild()
{
    QList<Entry> visible;
    const QString needle = m_filter.trimmed();

    if (needle.isEmpty()) {
        visible = m_all;
    } else {
        visible.reserve(m_all.size());
        for (const Entry &entry : std::as_const(m_all)) {
            if (entry.display.contains(needle, Qt::CaseInsensitive) || entry.name.contains(needle, Qt::CaseInsensitive)) {
                visible.append(entry);
            }
        }
    }

    if (visible == m_visible) {
        return;
    }

    const int previousCount = m_visible.size();
    beginResetModel();
    m_visible = visible;
    endResetModel();

    if (previousCount != m_visible.size()) {
        Q_EMIT countChanged();
    }
}
