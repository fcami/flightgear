// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Container class for related translation units
 */

#include <cassert>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <simgear/debug/logstream.hxx>
#include <simgear/nasal/cppbind/Ghost.hxx>

#include "TranslationResource.hxx"
#include "TranslationUnit.hxx"

using std::string;
using std::vector;

TranslationResource::TranslationResource(std::string name) noexcept
    :    _name(std::move(name))
{ }

void TranslationResource::addTranslationUnit(std::string name, int index,
                                             std::string sourceText,
                                             bool hasPlural)
{
    _map.emplace(KeyType(std::move(name), index),
                 new TranslationUnit(std::move(sourceText), {}, hasPlural));
}

void TranslationResource::setFirstTargetText(
    std::string name, int index, std::string targetText)
{
    const auto it = _map.find(std::make_pair(std::move(name), index));

    // If there is no such element in the map, it means addTranslationUnit()
    // wasn't called for this string. The most likely explanation for this
    // would be that it isn't in the default translation. IOW, it's an
    // obsolete string from the XLIFF file being loaded → ignore it.
    if (it != _map.end()) {
        // Set the first plural form
        it->second->setTargetText(0, std::move(targetText));
    }
}

void TranslationResource::setTargetTexts(
    std::string name, int index, std::vector<std::string> targetTexts)
{
    const auto key = std::make_pair(std::move(name), index);
    const auto translationUnit = _map[key];

    // Set the target texts only if this is not an obsolete string (see above)
    if (translationUnit) {
        translationUnit->setTargetTexts(std::move(targetTexts));
    }
}

TranslationResource::TranslationUnitRef
TranslationResource::translationUnit(const std::string& name, int index) const
{
    auto it = _map.find(std::make_pair(name, index));
    if (it != _map.end()) {
        return it->second;
    }

    return {};
}

string TranslationResource::get(const string& basicId, int index) const
{
    const auto translUnit = translationUnit(basicId, index);

    if (!translUnit) {
        return {};
    }

    if (translUnit->getPluralStatus()) {
        SG_LOG(SG_GENERAL, SG_DEV_ALERT,
               "TranslationResource::get() or "
               "TranslationResource::getWithDefault() used on translatable "
               "string '" << _name << "/" << basicId << ":" << index <<
               "' defined with has-plural=\"true\" in the default translation. "
               "Use TranslationResource::getPlural() or "
               "TranslationResource::getPluralWithDefault() instead.");
        return translUnit->getSourceText();
    } else {
        return translUnit->getTranslation();
    }
}

string TranslationResource::getPlural(intType cardinalNumber,
                                      const string& basicId, int index) const
{
    const auto translUnit = translationUnit(basicId, index);

    if (!translUnit) {
        return {};
    }

    if (!translUnit->getPluralStatus()) {
        SG_LOG(SG_GENERAL, SG_DEV_ALERT,
               "TranslationResource::getPlural() or "
               "TranslationResource::getPluralWithDefault() used on "
               "translatable string '" << _name << "/" << basicId << ":" <<
               index << "' that isn't defined with has-plural=\"true\" in the "
               "default translation. Use TranslationResource::get() or "
               "TranslationResource::getWithDefault() instead.");
        return translUnit->getSourceText();
    } else {
        return translUnit->getTranslation(cardinalNumber);
    }

    return {};
}

string TranslationResource::getWithDefault(
    const string& basicId, const string& defaultValue, int index) const
{
    const string result = get(basicId, index);

    return result.empty() ? defaultValue : result;
}

string TranslationResource::getPluralWithDefault(
    intType cardinalNumber, const string& basicId, const string& defaultValue,
    int index) const
{
    const string result = getPlural(cardinalNumber, basicId, index);

    return result.empty() ? defaultValue : result;
}

vector<string> TranslationResource::getAll(const string& name) const
{
    vector<string> result;
    decltype(_map)::const_iterator it;

    for (int index = 0;
         (it = _map.find(std::make_pair(name, index))) != _map.end();
         index++) {
        const auto& transUnit = it->second;
        // Plural form indices all hardcoded to 0
        const string targetText = transUnit->getTargetText(0);
        result.push_back(
            targetText.empty() ? transUnit->getSourceText() : targetText);
    }

    return result;
}

std::size_t TranslationResource::getCount(const string& name) const
{
    std::size_t index = 0;

    while (_map.find(std::make_pair(name, index)) != _map.end()) {
        index++;
    }

    return index;
}

vector<string> TranslationResource::getIdsWithPrefix(const string& prefix) const
{
    vector<string> result;
    std::set<string> dedup; // to avoid duplicates

    // KeyType pairs are ordered lexicographically by name first, so every
    // name starting with 'prefix' occupies one contiguous range starting at
    // this iterator.
    auto it = _map.lower_bound(KeyType(prefix, 0));

    for (; it != _map.end(); ++it) {
        const string& name = it->first.first;
        if (name.compare(0, prefix.size(), prefix) != 0) {
            break; // past the last name with this prefix
        }

        if (dedup.insert(name).second) {
            result.push_back(name);
        }
    }

    return result;
}

static naRef f_get(const TranslationResource& tr, const nasal::CallContext& ctx)
{
    if (ctx.argc < 1 || ctx.argc > 2) {
        ctx.runtimeError("TranslationResource.get(basicId[, index])");
    }

    const auto basicId = ctx.requireArg<std::string>(0);
    const auto index = ctx.getArg<int>(1); // the index defaults to 0

    return ctx.to_nasal(tr.get(std::move(basicId), index));
}

static naRef f_getPlural(const TranslationResource& tr, const nasal::CallContext& ctx)
{
    if (ctx.argc < 2 || ctx.argc > 3) {
        ctx.runtimeError(
            "TranslationResource.getPlural(cardinalNumber, basicId[, index])");
    }

    const auto cardinalNumber = ctx.requireArg<TranslationResource::intType>(0);
    const auto basicId = ctx.requireArg<std::string>(1);
    const auto index = ctx.getArg<int>(2); // the index defaults to 0

    return ctx.to_nasal(tr.getPlural(cardinalNumber, std::move(basicId),
                                     index));
}

static naRef f_getWithDefault(const TranslationResource& tr,
                              const nasal::CallContext& ctx)
{
    if (ctx.argc < 2 || ctx.argc > 3) {
        ctx.runtimeError("TranslationResource.getWithDefault(basicId, "
                         "defaultValue[, index])");
    }

    const auto basicId = ctx.requireArg<std::string>(0);
    const auto defaultValue = ctx.requireArg<std::string>(1);
    const auto index = ctx.getArg<int>(2); // the index defaults to 0

    return ctx.to_nasal(
        tr.getWithDefault(std::move(basicId), std::move(defaultValue), index));
}

static naRef f_getPluralWithDefault(const TranslationResource& tr,
                                    const nasal::CallContext& ctx)
{
    if (ctx.argc < 3 || ctx.argc > 4) {
        ctx.runtimeError(
            "TranslationResource.getPluralWithDefault(cardinalNumber, "
            "basicId, defaultValue[, index])");
    }

    const auto cardinalNumber = ctx.requireArg<TranslationResource::intType>(0);
    const auto basicId = ctx.requireArg<std::string>(1);
    const auto defaultValue = ctx.requireArg<std::string>(2);
    const auto index = ctx.getArg<int>(3); // the index defaults to 0

    return ctx.to_nasal(
        tr.getPluralWithDefault(cardinalNumber, std::move(basicId),
                                std::move(defaultValue), index));
}

static naRef f_translationUnit(const TranslationResource& tr,
                               const nasal::CallContext& ctx)
{
    if (ctx.argc < 1 || ctx.argc > 2) {
        ctx.runtimeError(
            "TranslationResource.translationUnit(basicId[, index])");
    }

    const auto basicId = ctx.requireArg<std::string>(0);
    const auto index = ctx.getArg<int>(1); // the index defaults to 0

    return ctx.to_nasal(
        tr.translationUnit(std::move(basicId), index));
}

// Static member function
void TranslationResource::setupGhost()
{
    using TranslationResourceRef = std::shared_ptr<TranslationResource>;
    using NasalTranslationResource = nasal::Ghost<TranslationResourceRef>;

    NasalTranslationResource::init("TranslationResource")
        .method("get", &f_get)
        .method("getPlural", &f_getPlural)
        .method("getWithDefault", &f_getWithDefault)
        .method("getPluralWithDefault", &f_getPluralWithDefault)
        .method("getAll", &TranslationResource::getAll)
        .method("getCount", &TranslationResource::getCount)
        .method("getIdsWithPrefix", &TranslationResource::getIdsWithPrefix)
        .method("translationUnit", &f_translationUnit);
}
