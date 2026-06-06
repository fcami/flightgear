// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftSearchModel.hxx"

#include <QRegularExpression>

#include "AircraftItemModel.hxx"

#include <simgear/package/Package.hxx>

AircraftSearchModel::AircraftSearchModel(QObject* pr, QAbstractItemModel* source)
    : QSortFilterProxyModel(pr)
{
    setSourceModel(source);
    setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect(this, &QAbstractItemModel::rowsInserted, this, &AircraftSearchModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &AircraftSearchModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &AircraftSearchModel::countChanged);
}

void AircraftSearchModel::setSearchString(QString s)
{
    m_searchString = s;

    m_searchProps = new SGPropertyNode;
    int index = 0;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    Q_FOREACH (QString term, s.split(QRegularExpression("\\W+"), Qt::SkipEmptyParts)) {
#else
    Q_FOREACH (QString term, s.split(QRegularExpression("\\W+"), QString::SkipEmptyParts)) {
#endif
        m_searchProps->getNode("all-of/text", index++, true)->setStringValue(term.toStdString());
    }

    invalidate();
    emit countChanged();
    emit searchStringChanged();
}

int AircraftSearchModel::count() const
{
    return rowCount();
}

bool AircraftSearchModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_searchString.isEmpty()) {
        return true;
    }

    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    simgear::pkg::PackageRef pkg = index.data(AircraftPackageRefRole).value<simgear::pkg::PackageRef>();
    if (pkg) {
        return pkg->matches(m_searchProps.ptr());
    }

    QString baseName = index.data(Qt::DisplayRole).toString();
    if (baseName.contains(m_searchString, Qt::CaseInsensitive)) {
        return true;
    }

    QString longDesc = index.data(AircraftLongDescriptionRole).toString();
    if (longDesc.contains(m_searchString, Qt::CaseInsensitive)) {
        return true;
    }

    const int variantCount = index.data(AircraftVariantCountRole).toInt();
    for (int variant = 0; variant < variantCount; ++variant) {
        QString desc = index.data(AircraftVariantDescriptionRole + variant).toString();
        if (desc.contains(m_searchString, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}
