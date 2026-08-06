// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Class for retrieving translated strings
 */

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <simgear/debug/logstream.hxx>
#include <simgear/nasal/cppbind/Ghost.hxx>
#include <simgear/nasal/cppbind/NasalCallContext.hxx>

#include <Main/locale.hxx>
#include <Main/globals.hxx>

#include "FGTranslate.hxx"
#include "TranslationDomain.hxx"

using std::string;

using flightgear::TranslationDomain;

FGTranslate::FGTranslate(const std::string& domain)
    : _domain(globals->get_locale()->getDomain(domain))
{ }

FGTranslate& FGTranslate::setDomain(const string& domain)
{
    // This logs a warning if the domain can't be found.
    _domain = globals->get_locale()->getDomain(domain);
    return *this;
}

TranslationDomain::ResourceRef
FGTranslate::getResource(const string& resourceName) const
{
    if (_domain) {
        return _domain->getResource(resourceName);
    }

    return {};
}

std::shared_ptr<TranslationUnit>
FGTranslate::translationUnit(const string& resourceName, const string& basicId,
                             int index) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->translationUnit(basicId, index);
    }

    return {};
}

string FGTranslate::get(const string& resourceName, const string& basicId,
                        int index) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->get(basicId, index);
    }

    return {};
}

string FGTranslate::getPlural(intType cardinalNumber, const string& resourceName,
                              const string& basicId, int index) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->getPlural(cardinalNumber, basicId, index);
    }

    return {};
}

string FGTranslate::getWithDefault(const string& resourceName,
                                   const string& basicId,
                                   const string& defaultValue, int index) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->getWithDefault(basicId, defaultValue, index);
    }

    return defaultValue;
}

string FGTranslate::getPluralWithDefault(
    intType cardinalNumber, const string& resourceName, const string& basicId,
    const string& defaultValue, int index) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->getPluralWithDefault(cardinalNumber, basicId,
                                              defaultValue, index);
    }

    return defaultValue;
}

std::vector<string> FGTranslate::getAll(const string& resourceName,
                                        const string& basicId) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->getAll(basicId);
    }

    return {};
}

std::size_t FGTranslate::getCount(const string& resourceName,
                                  const string& basicId) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->getCount(basicId);
    }

    return 0;
}

std::vector<string> FGTranslate::getIdsWithPrefix(const string& resourceName,
                                                  const string& prefix) const
{
    const auto resource = getResource(resourceName);

    if (resource) {
        return resource->getIdsWithPrefix(prefix);
    }

    return {};
}

static naRef f_get(const FGTranslate& tr, const nasal::CallContext& ctx)
{
    if (ctx.argc < 2 || ctx.argc > 3) {
        ctx.runtimeError("FGTranslate.get(resource, basicId[, index])");
    }

    const auto resource = ctx.requireArg<std::string>(0);
    const auto basicId = ctx.requireArg<std::string>(1);
    const auto index = ctx.getArg<int>(2); // the index defaults to 0

    return ctx.to_nasal(tr.get(std::move(resource), std::move(basicId),
                               index));
}

static naRef f_getPlural(const FGTranslate& tr, const nasal::CallContext& ctx)
{
    if (ctx.argc < 3 || ctx.argc > 4) {
        ctx.runtimeError(
            "FGTranslate.getPlural(cardinalNumber, resource, basicId[, index])");
    }

    const auto cardinalNumber = ctx.requireArg<FGTranslate::intType>(0);
    const auto resource = ctx.requireArg<std::string>(1);
    const auto basicId = ctx.requireArg<std::string>(2);
    const auto index = ctx.getArg<int>(3); // the index defaults to 0

    return ctx.to_nasal(tr.getPlural(cardinalNumber, std::move(resource),
                                     std::move(basicId), index));
}

static naRef f_getWithDefault(const FGTranslate& tr,
                              const nasal::CallContext& ctx)
{
    if (ctx.argc < 3 || ctx.argc > 4) {
        ctx.runtimeError("FGTranslate.getWithDefault(resource, basicId, "
                         "defaultValue[, index])");
    }

    const auto resource = ctx.requireArg<std::string>(0);
    const auto basicId = ctx.requireArg<std::string>(1);
    const auto defaultValue = ctx.requireArg<std::string>(2);
    const auto index = ctx.getArg<int>(3); // the index defaults to 0

    return ctx.to_nasal(
        tr.getWithDefault(std::move(resource), std::move(basicId),
                          std::move(defaultValue), index));
}

static naRef f_getPluralWithDefault(const FGTranslate& tr,
                                    const nasal::CallContext& ctx)
{
    if (ctx.argc < 4 || ctx.argc > 5) {
        ctx.runtimeError(
            "FGTranslate.getPluralWithDefault(cardinalNumber, resource, "
            "basicId, defaultValue[, index])");
    }

    const auto cardinalNumber = ctx.requireArg<FGTranslate::intType>(0);
    const auto resource = ctx.requireArg<std::string>(1);
    const auto basicId = ctx.requireArg<std::string>(2);
    const auto defaultValue = ctx.requireArg<std::string>(3);
    const auto index = ctx.getArg<int>(4); // the index defaults to 0

    return ctx.to_nasal(
        tr.getPluralWithDefault(cardinalNumber, std::move(resource),
                                std::move(basicId), std::move(defaultValue),
                                index));
}

static naRef f_translationUnit(const FGTranslate& tr,
                               const nasal::CallContext& ctx)
{
    if (ctx.argc < 2 || ctx.argc > 3) {
        ctx.runtimeError(
            "FGTranslate.translationUnit(resource, basicId[, index])");
    }

    const auto resource = ctx.requireArg<std::string>(0);
    const auto basicId = ctx.requireArg<std::string>(1);
    const auto index = ctx.getArg<int>(2); // the index defaults to 0

    return ctx.to_nasal(
        tr.translationUnit(std::move(resource), std::move(basicId), index));
}

// Static member function
void FGTranslate::setupGhost()
{
    using FGTranslateRef = std::shared_ptr<FGTranslate>;
    using NasalFGTranslate = nasal::Ghost<FGTranslateRef>;

    NasalFGTranslate::init("FGTranslate")
        .method("getResource", &FGTranslate::getResource)
        .method("get", &f_get)
        .method("getPlural", &f_getPlural)
        .method("getWithDefault", &f_getWithDefault)
        .method("getPluralWithDefault", &f_getPluralWithDefault)
        .method("getAll", &FGTranslate::getAll)
        .method("getCount", &FGTranslate::getCount)
        .method("getIdsWithPrefix", &FGTranslate::getIdsWithPrefix)
        .method("translationUnit", &f_translationUnit);
}
