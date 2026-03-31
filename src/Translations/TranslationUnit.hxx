// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Container class for a string and its translation
 */

#pragma once

#include <string>
#include <vector>

#include "LanguageInfo.hxx"

/**
 * @brief Class holding a source string and its translation in a language
 *
 * This class is a container for a source text and its translation in the
 * language corresponding to the currently selected locale, including plural
 * forms if any. The source text is a string in “engineering English”, known
 * as a “master string” or “default translation”.
 *
 * When the translation may depend on an integer (a number of “items” of any
 * kind), instances of this class will have `_hasPlural = true` and
 * their @c _sourceText member should use an indeterminate form such as “Found
 * %1 file(s)”. Corresponding plural forms in proper English locales (as
 * opposed to “engineering English”) will then be “Found %1 file” and “Found
 * %1 files”; they will be chosen according to the number passed to
 * FGTranslate::setCardinalNumber().
 */

class TranslationUnit
{
public:
    using intType = flightgear::LanguageInfo::intType;

    explicit TranslationUnit(const std::string sourceText = {},
                             const std::vector<std::string> targetTexts = {},
                             bool hasPlural = false);

    /// Return the source text of the object, i.e. the “engineering English” form
    std::string getSourceText() const;
    /// Set the source text of the object, i.e. the “engineering English” form
    void setSourceText(std::string text);

    /// Get the target text (translated string) associated to @a pluralFormIndex
    std::string getTargetText(int pluralFormIndex = 0) const;
    // For sanity checks when a caller is about to use a plural form index
    /// Return the number of target texts (plural forms) present in the object
    std::size_t getNumberOfTargetTexts() const;
    /// Set the target text for the specified plural form
    void setTargetText(int pluralFormIndex, std::string text);
    /// Set all target texts at once
    void setTargetTexts(std::vector<std::string> texts);

    /**
     * @brief Return the plural status of a translation unit
     *
     * @return True if the translated string is declared as having plural
     *         forms in the default translation
     *
     * The size of @c _targetTexts depends on whether the <trans-unit> in the
     * XLIFF translation file contains a <target> element and on the plural
     * status of the TranslationUnit (if its plural status is true and the
     * string has been translated, the size of @c _targetTexts should
     * correspond to the return value of LanguageInfo::getNumberOfPluralForms()
     * for the selected language).
     */
    bool getPluralStatus() const;
    /**
     * @brief Set the plural status of a translation unit
     *
     * @param hasPlural  true if the translation unit has plural forms
     */
    void setPluralStatus(int hasPlural);

    /**
     * @brief Get the target text of a non-plural translation unit
     * @return The requested translation (in the selected language)
     *
     * This function is for translatable strings without plural forms (i.e.,
     * those defined without `has-plural="true"` in the default translation.).
     *
     * The return value is the first target text, unless it is empty (string
     * not translated) or the translation unit has no target text
     * (fgfs --language=default); in the latter two cases, the return value is
     * the source text.
     */
    std::string getTranslation() const;
    /**
     * @brief Get the target text of a translation unit that has plural forms
     * @param cardinalNumber  an integer corresponding to a number of
     *                        “things” (concrete or abstract)
     * @return The requested translation (in the selected language)
     *
     * This function is for translatable strings that have plural forms (i.e.,
     * those defined with `has-plural="true"` in the default translation). The
     * selected form depends on the @a cardinalNumber argument.
     */
    std::string getTranslation(intType cardinalNumber) const;

    /**
     * @brief Set up a Nasal type that wraps TranslationUnit
     */
    static void setupGhost();

private:
    /// String to translate, in “engineering English”
    std::string _sourceText;
    /**
     * @brief Translation (possibly with plural forms) of the source text
     *
     * This member can be empty or contain an empty string if the source text
     * has not been translated. If it has been translated:
     *   - if the plural status is false, `_targetTexts[0]` is the
     *     translation;
     *   - otherwise, @c _targetTexts contains the plural forms of the
     *     translation in the same order as Qt Linguist.
     */
    std::vector<std::string> _targetTexts;
    /// True if the TranslationUnit has plural forms
    bool _hasPlural;
};
