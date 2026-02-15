// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Information on plural forms for the supported languages
 */

#pragma once

#include <functional>
#include <map>
#include <string>
#include <type_traits>

namespace flightgear
{

/**
 * @brief Class providing information on plural forms
 *
 * Languages known to this class are identified by a @em languageId string
 * which is "default" for the default translation, otherwise the value of
 * /sim/intl/locale[n]/language-id for the selected locale.
 *
 * Plural forms are identified by an index that starts from 0. Whenever
 * possible, we use the same order as Qt Linguist for plural forms in a given
 * language.
 *
 * When retrieving a translation that has plural form, one obtains the index of
 * the plural form to use by providing a “cardinal number” (a number of “items”
 * of any kind, which could be concrete objects or abstract things) to a
 * language-dependent function. This class contains a map from @em languageId
 * strings to such functions, which is used by getPluralFormIndex().
 */

class LanguageInfo
{
public:
    using intType = long long;

    /// Return the number of plural forms in the specified language
    static std::size_t getNumberOfPluralForms(const std::string& languageId);
    /// Return the index of the plural form to use for a number of “items”
    static std::size_t getPluralFormIndex(const std::string& languageId,
                                          intType number);

private:
    // Important: std::abs(i) is not UB and fits in an uintType as long as i
    // has type intType.
    using uintType = std::make_unsigned_t<intType>;

    static const std::map<std::string, std::size_t> nbPluralFormsMap;

    using funcType = std::function<std::size_t(uintType)>;
    /**
     * @brief Map from language ids to functions that return the index of the
     *        plural form to use for a given cardinal number.
     */
    static const std::map<std::string, funcType> pluralFormIndexFuncMap;

    static std::size_t pluralFormIndex_EngineeringEnglishStyle(uintType n);
    static std::size_t pluralFormIndex_EnglishStyle(uintType n);
    static std::size_t pluralFormIndex_FrenchStyle(uintType n);
    static std::size_t pluralFormIndex_PolishStyle(uintType n);
    static std::size_t pluralFormIndex_RussianStyle(uintType n);
    static std::size_t pluralFormIndex_SlovakStyle(uintType n);
};

}
