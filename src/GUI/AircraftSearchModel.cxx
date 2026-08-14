// SPDX-FileCopyrightText: 2020 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftSearchModel.hxx"

#include <algorithm>

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

QVariantList AircraftSearchModel::selectedTags() const
{
    QVariantList result;
    for (const auto& t : m_selectedTags) {
        QVariantMap m;
        m["tag"] = t.tag;
        m["negated"] = t.negated;
        result.append(m);
    }
    return result;
}

void AircraftSearchModel::setSelectedTags(QVariantList tags)
{
    QList<SelectedTag> newTags;
    for (const auto& v : tags) {
        const auto m = v.toMap();
        newTags.append({m.value("tag").toString(), m.value("negated").toBool()});
    }

    m_selectedTags = newTags;
    invalidate();
    emit selectedTagsChanged();
    emit countChanged();
}

bool AircraftSearchModel::hasTag(QString tag) const
{
    for (const auto& t : m_selectedTags) {
        if (t.tag.compare(tag, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

void AircraftSearchModel::addTag(QString tag)
{
    if (hasTag(tag)) {
        return;
    }

    m_selectedTags.append({tag, false});
    invalidate();
    emit selectedTagsChanged();
    emit countChanged();
}

void AircraftSearchModel::removeTag(QString tag)
{
    const auto count = m_selectedTags.size();
    auto it = std::remove_if(m_selectedTags.begin(), m_selectedTags.end(), [&tag](const SelectedTag& t) {
        return t.tag.compare(tag, Qt::CaseInsensitive) == 0;
    });
    m_selectedTags.erase(it, m_selectedTags.end());

    if (m_selectedTags.size() == count) {
        return; // nothing removed
    }

    invalidate();
    emit selectedTagsChanged();
    emit countChanged();
}

void AircraftSearchModel::toggleTagNegation(QString tag)
{
    for (auto& t : m_selectedTags) {
        if (t.tag.compare(tag, Qt::CaseInsensitive) == 0) {
            t.negated = !t.negated;
            invalidate();
            emit selectedTagsChanged();
            emit countChanged();
            return;
        }
    }
}

void AircraftSearchModel::clearTags()
{
    if (m_selectedTags.isEmpty()) {
        return;
    }

    m_selectedTags.clear();
    invalidate();
    emit selectedTagsChanged();
    emit countChanged();
}

bool AircraftSearchModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    if (!m_selectedTags.isEmpty()) {
        const QStringList aircraftTags = index.data(AircraftTagsRole).toStringList();

        // negated tags always exclude (AND semantics); non-negated tags use
        // OR semantics, i.e. the aircraft must match at least one of them
        bool haveIncludeTag = false;
        bool matchedIncludeTag = false;

        for (const auto& selected : m_selectedTags) {
            const bool present = aircraftTags.contains(selected.tag, Qt::CaseInsensitive);
            if (selected.negated) {
                if (present) {
                    return false;
                }
            } else {
                haveIncludeTag = true;
                matchedIncludeTag = matchedIncludeTag || present;
            }
        }

        if (haveIncludeTag && !matchedIncludeTag) {
            return false;
        }
    }

    if (m_searchString.isEmpty()) {
        return true;
    }

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
