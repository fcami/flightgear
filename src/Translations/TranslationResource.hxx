// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Container class for related translation units
 */

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "LanguageInfo.hxx"
#include "TranslationUnit.hxx"

/**
 * @brief Class that holds translation units within a resource (“context”)
 *
 * This class provides functions for adding TranslationUnit
 * instances to a translation “resource”, modifying these objects and
 * retrieving information from them, in particular obtaining translations
 * associated to a translation “basic id” within the resource.
 */

class TranslationResource
{
public:
    using intType = flightgear::LanguageInfo::intType;

    TranslationResource() = delete;
    /**
     * @brief Constructor
     *
     * @param name   resource name (used for error messages)
     */
    explicit TranslationResource(std::string name) noexcept;
    /**
     * @brief Add a translation unit to the TranslationResource
     *
     * @param name   “basic id” of the translation unit (which corresponds to
     *               the element name in the default translation XML file)
     * @param index  zero-based index used to distinguish between elements with
     *               the same @a name within a translation resource
     * @param sourceText  text that the translation unit gives a translation of
     * @param hasPlural   tells whether the translation unit has plural forms
     *                    (variants chosen based on a “cardinal number”)
     *
     * The added TranslationUnit instance has an empty list of target texts.
     */
    void addTranslationUnit(std::string name, int index, std::string sourceText,
                            bool hasPlural = false);
    /**
     * @brief Set the first target text of a translation unit.
     *
     * @param name   “basic id” of the translation unit to modify
     * @param index  index used to distinguish between translation units that
     *               share the same “basic id” (which corresponds to elements
     *               having the same name in a default translation XML file)
     * @param targetText  first plural form of the translation associated to
     *                    @a name and @a index (this is the only form for
     *                    translatable strings that have “no plural form”)
     */
    void setFirstTargetText(std::string name, int index,
                            std::string targetText);
    /**
     * @brief Set all target texts of a translation unit.
     *
     * @param name         same as in setFirstTargetText()
     * @param index        same as in setFirstTargetText()
     * @param targetTexts  vector of strings which are translations in the
     *                     order of plural forms for the target language of
     *                     the translation unit
     */
    void setTargetTexts(std::string name, int index,
                        std::vector<std::string> targetTexts);

    /**
     * @brief Return a shared pointer to a TranslationUnit
     *
     * @param name   element name (aka “basic id”) in the default translation
     *               XML file
     * @param index  zero-based index used to distinguish between elements with
     *               the same @a name in the TranslationResource
     * @return a shared pointer to the TranslationUnit with this name and index
     */
    std::shared_ptr<TranslationUnit>
    translationUnit(const std::string& name, int index = 0) const;

    /**
     * @brief Get a single translation.
     *
     * @param basicId   name of the XML element corresponding to the
     *                  translation to retrieve in the default translation
     *                  file for the specified resource
     * @param index     index among strings sharing the same basicId
     * @return The translated string
     *
     * There may be several elements named @a basicId in the default
     * translation file for the specified @a resource; these elements are
     * distinguished by their @a index.
     *
     * If there is no translatable string with the specified @a basicId and
     * @a index in the `TranslationResource`, return an empty string.
     */
    std::string get(const std::string& basicId, int index = 0) const;
    /**
     * @brief Same as get(), but for a string that has plural forms.
     *
     * @param cardinalNumber  an integer corresponding to a number of
     *                        “things” (concrete or abstract)
     * @param basicId         same as for get()
     * @param index           same as for get()
     * @return The translated string
     */
    std::string getPlural(intType cardinalNumber, const std::string& basicId,
                          int index = 0) const;
    /**
     * @brief Get a single translation, with default for missing or empty
     *        strings.
     *
     * @param basicId       same as for get()
     * @param defaultValue  returned if the string can't be found or is
     *                      declared with an empty source text in the
     *                      default translation
     * @param index         same as for get()
     * @return The translated string or default value
     */
    std::string getWithDefault(const std::string& basicId,
                               const std::string& defaultValue,
                               int index = 0) const;
    /**
     * @brief Same as getWithDefault(), but for a string that has plural forms.
     *
     * @param cardinalNumber  an integer corresponding to a number of
     *                        “things” (concrete or abstract)
     * @param basicId         same as for getWithDefault()
     * @param defaultValue    same as for getWithDefault()
     * @param index           same as for getWithDefault()
     * @return The translated string or default value
     */
    std::string getPluralWithDefault(intType cardinalNumber,
                                     const std::string& basicId,
                                     const std::string& defaultValue,
                                     int index = 0) const;
    /**
     * @brief Get translations for all strings with a given element name.
     *
     * @param name  element name (aka “basic id”) in the default translation
     *              XML file
     * @return A vector containing the first target text (i.e., first plural
     *         form) of each translated string with the specified element name
     */
    std::vector<std::string> getAll(const std::string& name) const;
    /**
     * @brief Get the number of translated strings with the given element
     *        name.
     *
     * @param name  element name (aka “basic id”) in the default translation
     *              XML file
     * @return The number of translation units with basic id @a name in the
     *         TranslationResource
     */
    std::size_t getCount(const std::string& name) const;

    /**
     * @brief Set up a Nasal type that wraps TranslationResource
     */
    static void setupGhost();

private:
    /// Name of the resource (e.g., "options" or "dialog-location-in-air")
    std::string _name;
    // In the default translation files, this corresponds to a node name and
    // its index.
    using KeyType = std::pair<std::string, int>;
    using TranslationUnitRef = std::shared_ptr<TranslationUnit>;
    std::map<KeyType, TranslationUnitRef> _map;
};
