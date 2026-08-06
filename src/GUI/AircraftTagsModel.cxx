// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AircraftTagsModel.hxx"

#include <array>

#include <simgear/misc/strutils.hxx>

#include <Main/globals.hxx>
#include <Main/locale.hxx>

using namespace std::string_literals;

namespace {
// Mirrors flightgear.meta.aircraft_catalogs.catalogTags.TagGroup on the
// fgmeta-python side. Only the group id strings need to stay in sync: the
// actual tags and their translations are discovered at runtime from the
// "tags" resource (see AircraftTagsModel::reload()).
constexpr std::array<const char*, 6> knownGroups = {
    "role", "manufacturer", "era", "feature", "propulsion", "sim-feature"};

const std::string tagIdPrefix = "tag-"s;
const std::string descriptionSuffix = "-description"s;
const std::string tagsResource = "tags"s;

} // namespace

AircraftTagsModel::AircraftTagsModel(QObject* parent) : QAbstractListModel(parent)
{
    reload();
}

void AircraftTagsModel::reload()
{
    beginResetModel();
    _tags.clear();

    const auto ids = globals->get_locale()->getLocalizedStringIdsWithPrefix(tagIdPrefix, tagsResource);
    for (const auto& translationId : ids) {
        if (simgear::strutils::ends_with(translationId, descriptionSuffix))
            continue; // sibling description entry, not a tag itself

        const std::string rest = translationId.substr(tagIdPrefix.size());
        for (const char* group : knownGroups) {
            const std::string prefix = group + "-"s;
            if (rest.rfind(prefix, 0) == 0) {
                _tags.push_back({rest.substr(prefix.size()), group, translationId});
                break;
            }
        }
    }

    endResetModel();
}

int AircraftTagsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(_tags.size());
}

const AircraftTagsModel::TagInfo* AircraftTagsModel::infoForTag(const QString& tag) const
{
    const auto tagId = tag.toStdString();
    for (const auto& info : _tags) {
        if (info.tagId == tagId)
            return &info;
    }
    return nullptr;
}

QVariant AircraftTagsModel::data(const QModelIndex& index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= static_cast<int>(_tags.size()))
        return {};

    const auto& info = _tags.at(static_cast<size_t>(row));
    switch (role) {
    case TagRole:
    case Qt::DisplayRole:
        return QString::fromStdString(info.tagId);
    case NameRole:
        return nameForTag(QString::fromStdString(info.tagId));
    case DescriptionRole:
        return descriptionForTag(QString::fromStdString(info.tagId));
    case GroupRole:
        return QString::fromStdString(info.group);
    default:
        return {};
    }
}

QHash<int, QByteArray> AircraftTagsModel::roleNames() const
{
    auto result = QAbstractListModel::roleNames();
    result[TagRole] = "tag";
    result[NameRole] = "name";
    result[DescriptionRole] = "description";
    result[GroupRole] = "group";
    return result;
}

QString AircraftTagsModel::nameForTag(QString tag) const
{
    const auto* info = infoForTag(tag);
    if (!info)
        return tag;

    const auto name = globals->get_locale()->getLocalizedString(info->translationId, tagsResource, info->tagId);
    return QString::fromStdString(name);
}

QString AircraftTagsModel::descriptionForTag(QString tag) const
{
    const auto* info = infoForTag(tag);
    if (!info)
        return {};

    const auto desc = globals->get_locale()->getLocalizedString(info->translationId + descriptionSuffix, tagsResource);
    return QString::fromStdString(desc);
}

int AircraftTagsModel::indexOfTag(QString tag) const
{
    const auto tagId = tag.toStdString();
    for (size_t i = 0; i < _tags.size(); ++i) {
        if (_tags.at(i).tagId == tagId)
            return static_cast<int>(i);
    }
    return -1;
}

bool AircraftTagsModel::isKnownTag(QString tag) const
{
    return infoForTag(tag) != nullptr;
}
