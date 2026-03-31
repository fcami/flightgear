// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Class for retrieving translated strings
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "LanguageInfo.hxx"
#include "TranslationDomain.hxx"

/**
 * @brief Class for retrieving translated strings
 *
 * The default domain is “core”; it corresponds to translations defined in
 * FGData. Other domains are “current-aircraft” and “addons/⟨addonId⟩”.
 *
 * If the translatable string specified by the (domain, resource, basicId,
 * index) tuple doesn't have has-plural="true" in the default translation,
 * it is a string with no plural forms. Member functions get() and
 * getWithDefault() are appropriate for retrieving translations of such
 * strings. On the other hand, if a translatable string is defined with
 * has-plural="true" in the default translation, it has plural forms.
 * Member functions getPlural() and getPluralWithDefault() are suitable for
 * such strings: they require an additional parameter (“cardinal number”)
 * which is necessary to choose the correct plural form.
 *
 * If your code doesn't know in advance whether the string has plural
 * forms, use translationUnit(). With the result, you can query whether the
 * string has plural forms and obtain a translation (see the overloads of
 * TranslationUnit::getTranslation(): one accepts a cardinal number, the
 * other one doesn't). Doing so minimizes the number of lookups for the
 * resource, basicId and index because a @c TranslationUnit object contains
 * all the information pertaining to a given translatable string.
 *
 * Examples:
 *
 * Retrieve the translation of a string defined in FGData that has no
 * plural forms (domain = "core", resource = "options", basicId = "usage",
 * index = 0):
 * @code
 * std::string t = FGTranslate().get("options", "usage");
 * @endcode
 *
 * or equivalently:
 * @code
 * std::string t = FGTranslate("core").get("options", "usage");
 * @endcode
 *
 * Similar thing but using the second string (index 1) having the id
 * "fg-scenery-desc":
 * @code
 * std::string t = FGTranslate().get("options", "fg-scenery-desc", 1);
 * @endcode
 *
 * Similar thing, but fetching the translation from the Skeleton add-on:
 * @code
 * std::string t =
 *   FGTranslate("addons/org.flightgear.addons.Skeleton").get(
 *     "some-resource", "some-id", 1);
 * @endcode
 *
 * Retrieve a translation with plural forms defined in the current
 * aircraft. Let's assume the translation depends on a number of
 * liveries present in the @a nbLiveries variable.
 * @code
 * std::string t = FGTranslate("current-aircraft").getPlural(
 *                   nbLiveries, "some-resource", "some-id");
 * @endcode
 */

class FGTranslate
{
public:
    // I did this to avoid making both LanguageInfo and FGTranslate class
    // templates...
    using intType = flightgear::LanguageInfo::intType;

    /**
     * @brief Constructor.
     *
     * @param domain  a string such as “core”, “current-aircraft” or
     *                “addons/⟨addonId⟩”
     *
     * The constructed FGTranslate instance will allow retrieving
     * translations from the chosen domain. The domain must already exist
     * when the constructor is called.
     */
    explicit FGTranslate(const std::string& domain = "core");

    /**
     * @brief Change the domain from which to retrieve translations.
     *
     * @param domain  a string such as “core”, “current-aircraft” or
     *                “addons/⟨addonId⟩”
     * @return The FGTranslate instance
     *
     * If you intend to query translations from one domain only, better
     * pass it directly to the constructor, if possible.
     */
    FGTranslate& setDomain(const std::string& domain);

    /**
     * @brief Get the specified resource.
     *
     * @param resourceName  name of the resource
     * @return A shared pointer to the resource
     *
     * This function logs warnings if the domain or resource can't be found.
     */
    flightgear::TranslationDomain::ResourceRef
    getResource(const std::string& resourceName) const;

    /**
     * @brief Get a single translation.
     *
     * @param resource  resource name, aka “context” (such as "atc", "menu",
     *                  "sys", etc.) the translatable string belongs to
     * @param basicId   name of the XML element corresponding to the
     *                  translation to retrieve in the default translation
     *                  file for the specified resource
     * @param index     index among strings sharing the same basicId
     * @return The translated string
     *
     * The translation is fetched from the domain specified with the
     * constructor or with setDomain().
     *
     * There may be several elements named @a basicId in the default
     * translation file for the specified @a resource; these elements are
     * distinguished by their @a index.
     *
     * If the @a resource doesn't exist in the domain or if there is no
     * translatable string with the specified @a basicId and @a index
     * in the @a resource, return an empty string.
     */
    std::string get(const std::string& resource, const std::string& basicId,
                    int index = 0) const;
    /**
     * @brief Same as get(), but for a string that has plural forms.
     *
     * @param cardinalNumber  an integer corresponding to a number of
     *                        “things” (concrete or abstract)
     * @param resource        same as for get()
     * @param basicId         same as for get()
     * @param index           same as for get()
     * @return The translated string
     */
    std::string getPlural(intType cardinalNumber, const std::string& resource,
                          const std::string& basicId, int index = 0) const;
    /**
     * @brief Get a single translation, with default for missing or empty
     *        strings.
     *
     * @param resource      same as for get()
     * @param basicId       same as for get()
     * @param defaultValue  returned if the string can't be found or is
     *                      declared with an empty source text in the
     *                      default translation
     * @param index         same as for get()
     * @return The translated string or default value
     */
    std::string getWithDefault(const std::string& resource,
                               const std::string& basicId,
                               const std::string& defaultValue,
                               int index = 0) const;
    /**
     * @brief Same as getWithDefault(), but for a string that has plural forms.
     *
     * @param cardinalNumber  an integer corresponding to a number of
     *                        “things” (concrete or abstract)
     * @param resource        same as for getWithDefault()
     * @param basicId         same as for getWithDefault()
     * @param defaultValue    same as for getWithDefault()
     * @param index           same as for getWithDefault()
     * @return The translated string or default value
     */
    std::string getPluralWithDefault(
        intType cardinalNumber, const std::string& resource,
        const std::string& basicId, const std::string& defaultValue,
        int index = 0) const;

    /**
     * @brief Get all translations associated to an id (tag name).
     *
     * @param resource      same as for get()
     * @param basicId       same as for get()
     * @return A vector containing all translations with id @a basicId in the
     *         specified @a resource
     */
    std::vector<std::string> getAll(const std::string& resource,
                                    const std::string& basicId) const;
    /**
     * @brief Get the number of translatable strings with a given id (tag name).
     *
     * @param resource      same as for get()
     * @param basicId       same as for get()
     * @return The number of elements named @a basicId in the specified
     *         @a resource (this is the size of the vector that getAll() would
     *         return)
     */
    std::size_t getCount(const std::string& resource,
                         const std::string& basicId) const;

    /**
     * @brief Return a shared pointer to a TranslationUnit.
     *
     * @param resource      same as for get()
     * @param basicId       same as for get()
     * @param index         same as for get()
     *
     * This function allows one to efficiently perform several operations
     * on the same translatable string (for instance, querying whether it
     * has plural forms before getting a translation, or retrieving several
     * translations for different values of the “cardinal number”).
     */
    std::shared_ptr<TranslationUnit>
    translationUnit(const std::string& resource, const std::string& basicId,
                    int index = 0) const;
    /**
     * @brief Set up a Nasal type that wraps FGTranslate
     */
    static void setupGhost();

private:
    using TranslationDomain = flightgear::TranslationDomain;

    using TranslationDomainRef = std::shared_ptr<const TranslationDomain>;
    TranslationDomainRef _domain;
};
