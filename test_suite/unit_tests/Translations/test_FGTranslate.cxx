// SPDX-FileCopyrightText: 2025 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Translations: automated tests for FGTranslate
 *
 * Some tests in this file depend on particular default translation strings
 * (“engineering English”) and translations in $FG_ROOT/Translations. If these
 * are modified, the changes will have to be reflected here.
 */

#include "config.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <Add-ons/AddonManager.hxx>
#include <Main/fg_props.hxx>
#include <Main/globals.hxx>
#include <Main/locale.hxx>
#include <Translations/FGTranslate.hxx>

#include "test_FGTranslate.hxx"
#include "test_suite/FGTestApi/testGlobals.hxx"

using namespace std::string_literals;

using std::string;

using flightgear::addons::AddonManager;

/* codespell:ignore-begin (French) */

// The en_US strings may differ from the default translation strings (the
// former are found in <target> elements of
// Translations/en_US/FlightGear-nonQt.xlf, the latter in <source> elements of
// the same file and in Translations/default/{auto-extracted,}/*.xml; however,
// this should in general only happen for strings that have plural forms.
void FGTranslateTests::commonBetweenDefaultTranslationAndEn_US()
{
    string fetched = FGTranslate().get("options", "general-options");
    CPPUNIT_ASSERT_EQUAL("General Options"s, fetched);

    fetched = FGTranslate("core").get("options", "general-options");
    CPPUNIT_ASSERT_EQUAL("General Options"s, fetched);

    fetched = FGTranslate().get("options", "fg-scenery-desc", 0);
    CPPUNIT_ASSERT_EQUAL("Specify the scenery path(s);"s, fetched);

    fetched = FGTranslate().get("options", "fg-scenery-desc", 1);
    CPPUNIT_ASSERT_EQUAL("Defaults to $FG_ROOT/Scenery"s, fetched);

    fetched = FGTranslate().get("dialog-exit", "exit-button-label");
    CPPUNIT_ASSERT_EQUAL("Exit"s, fetched);
}

void FGTranslateTests::test_defaultTranslation()
{
    FGTestApi::setUp::initTestGlobals("test_defaultTranslation", "default");
    commonBetweenDefaultTranslationAndEn_US();
}

void FGTranslateTests::test_en_US()
{
    FGTestApi::setUp::initTestGlobals("test_en_US", "en_US");
    commonBetweenDefaultTranslationAndEn_US();
}

void FGTranslateTests::test_fr()
{
    FGTestApi::setUp::initTestGlobals("test_fr", "fr");

    string fetched = FGTranslate().get("options", "general-options");
    CPPUNIT_ASSERT_EQUAL("Options générales"s, fetched);

    fetched = FGTranslate("core").get("options", "general-options");
    CPPUNIT_ASSERT_EQUAL("Options générales"s, fetched);

    fetched = FGTranslate().get("dialog-exit", "exit-button-label");
    CPPUNIT_ASSERT_EQUAL("Quitter"s, fetched);

    fetched = FGTranslate().get("options", "fg-scenery-desc", 0);
    CPPUNIT_ASSERT_EQUAL("Spécifie l'emplacement des répertoires des scènes ;"s,
                         fetched);

    fetched = FGTranslate().get("options", "fg-scenery-desc", 1);
    CPPUNIT_ASSERT_EQUAL("Positionné par défaut à $FG_ROOT/Scenery"s, fetched);
}

void FGTranslateTests::test_nonExistentTranslation()
{
    FGTestApi::setUp::initTestGlobals("test_nonExistentTranslation",
                                      "non-existent language");

    // None of the /sim/intl/locale[n] nodes matches the above language,
    // therefore FGLocale::selectLanguage() uses the fallback translation at
    // /sim/intl/locale[0], which is English.
    const string fetched = FGTranslate().get("options", "general-options");
    CPPUNIT_ASSERT_EQUAL("General Options"s, fetched);
}

void FGTranslateTests::test_getWithDefault()
{
    FGTestApi::setUp::initTestGlobals("test_getWithDefault", "en");

    string fetched = FGTranslate().getWithDefault("options", "general-options",
                                                  "some default");
    CPPUNIT_ASSERT_EQUAL("General Options"s, fetched);

    fetched = FGTranslate().getWithDefault("options", "non-existent foobar",
                                           "the default");
    CPPUNIT_ASSERT_EQUAL("the default"s, fetched);

    // Change the selected language to French
    globals->get_locale()->selectLanguage("fr");

    fetched = FGTranslate().getWithDefault("options", "general-options",
                                           "some default");
    CPPUNIT_ASSERT_EQUAL("Options générales"s, fetched);

    fetched = FGTranslate().getWithDefault(
        "options", "non-existent foobar", "the default");
    CPPUNIT_ASSERT_EQUAL("the default"s, fetched);
}

void FGTranslateTests::test_pluralsAndAircraftDomain()
{
    FGTestApi::setUp::initTestGlobals("test_pluralsAndAircraftDomain", "en_US");

    const auto dir = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "Aircraft" / "Test";
    fgSetString("/sim/aircraft-dir"s, dir.utf8Str());

    globals->get_locale()->loadAircraftTranslations();
    auto tr = FGTranslate("current-aircraft");

    auto translUnit = tr.translationUnit("some-resource", "hello");
    CPPUNIT_ASSERT(!translUnit->getPluralStatus()); // no plural forms here

    string fetched = translUnit->getTranslation();
    CPPUNIT_ASSERT_EQUAL("Hello from the Test aircraft!"s, fetched);

    // String with plural forms. In English, we have singular for 1 and plural
    // for all other non-negative integers, including 0.
    translUnit = tr.translationUnit("some-resource", "cats");
    CPPUNIT_ASSERT(translUnit->getPluralStatus()); // the string has plural forms

    fetched = translUnit->getTranslation(0);
    CPPUNIT_ASSERT_EQUAL("%1 cats is not enough cats."s, fetched);

    fetched = translUnit->getTranslation(1);
    CPPUNIT_ASSERT_EQUAL("%1 cat is not enough cats."s, fetched);

    fetched = translUnit->getTranslation(2);
    CPPUNIT_ASSERT_EQUAL("%1 cats is not enough cats."s, fetched);

    // Same thing, but done less efficiently (this involves more lookups)
    fetched = tr.getPlural(0, "some-resource", "cats");
    CPPUNIT_ASSERT_EQUAL("%1 cats is not enough cats."s, fetched);

    fetched = tr.getPlural(1, "some-resource", "cats");
    CPPUNIT_ASSERT_EQUAL("%1 cat is not enough cats."s, fetched);

    fetched = tr.getPlural(2, "some-resource", "cats");
    CPPUNIT_ASSERT_EQUAL("%1 cats is not enough cats."s, fetched);

    // Change the selected language to French
    globals->get_locale()->selectLanguage("fr");
    globals->get_locale()->loadAircraftTranslations();
    tr = FGTranslate("current-aircraft");

    translUnit = tr.translationUnit("some-resource", "hello");
    CPPUNIT_ASSERT(!translUnit->getPluralStatus());

    fetched = translUnit->getTranslation();
    CPPUNIT_ASSERT_EQUAL("Bonjour depuis l'aéronef Test !"s, fetched);

    // String with plural forms. In French, we have singular for 0 and 1,
    // plural for all other non-negative integers.
    translUnit = tr.translationUnit("some-resource", "cats");
    CPPUNIT_ASSERT(translUnit->getPluralStatus());

    fetched = translUnit->getTranslation(0);
    CPPUNIT_ASSERT_EQUAL("%1 chat, ce n'est pas assez de chats."s, fetched);

    fetched = translUnit->getTranslation(1);
    CPPUNIT_ASSERT_EQUAL("%1 chat, ce n'est pas assez de chats."s, fetched);

    fetched = translUnit->getTranslation(2);
    CPPUNIT_ASSERT_EQUAL("%1 chats, ce n'est pas assez de chats."s, fetched);

    // Same thing, but done less efficiently (this involves more lookups)
    fetched = tr.getPlural(0, "some-resource", "cats");
    CPPUNIT_ASSERT_EQUAL("%1 chat, ce n'est pas assez de chats."s, fetched);

    fetched = tr.getPlural(1, "some-resource", "cats");
    CPPUNIT_ASSERT_EQUAL("%1 chat, ce n'est pas assez de chats."s, fetched);

    fetched = tr.getPlural(2, "some-resource", "cats");
    CPPUNIT_ASSERT_EQUAL("%1 chats, ce n'est pas assez de chats."s, fetched);

    fetched = tr.getPluralWithDefault(2, "some-resource", "non-existent-id",
                                      "the default");
    CPPUNIT_ASSERT_EQUAL("the default"s, fetched);

    fetched = tr.getPluralWithDefault(2, "some-resource", "non-existent-id",
                                      "the default", 0 /* explicit index */);
    CPPUNIT_ASSERT_EQUAL("the default"s, fetched);
}

void FGTranslateTests::test_multipleIndices()
{
    FGTestApi::setUp::initTestGlobals("test_multipleIndices", "en_US");

    const auto dir = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "Aircraft" / "Test";
    fgSetString("/sim/aircraft-dir"s, dir.utf8Str());

    globals->get_locale()->loadAircraftTranslations();
    auto tr = FGTranslate("current-aircraft");

    CPPUNIT_ASSERT_EQUAL(std::size_t(3),
                         tr.getCount("dialog-whatever", "sentence"));
    std::vector<std::string> v = {
        "Make sure the Prince doesn't leave this room until I come and get him.",
        "Not to leave the room even if you come and get him.",
        "No, no. Until I come and get him."
    };

    std::vector<std::string> v2 = tr.getAll("dialog-whatever", "sentence");
    bool equal = std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend());
    CPPUNIT_ASSERT(equal);

    // Change the selected language to French
    globals->get_locale()->selectLanguage("fr");
    globals->get_locale()->loadAircraftTranslations();
    tr = FGTranslate("current-aircraft");

    // There are 3 strings with basicId "sentence" in resource "dialog-whatever"
    CPPUNIT_ASSERT_EQUAL(std::size_t(3),
                         tr.getCount("dialog-whatever", "sentence"));
    v = {"Assurez-vous que le prince ne quitte pas cette pièce avant que je "
         "ne revienne le checher.",
         "Ne pas quitter la pièce même si vous revenez le chercher.",
         "Non, non. Jusqu'à ce que je revienne le chercher."
    };
    v2 = tr.getAll("dialog-whatever", "sentence");
    equal = std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend());
    CPPUNIT_ASSERT(equal);

    // Similar thing, but done less efficiently (this involves more lookups)
    for (std::size_t i = 0; i < v.size(); i++) {
        string fetched = tr.get("dialog-whatever", "sentence", i);
        CPPUNIT_ASSERT_EQUAL(v[i], fetched);
    }
}

void FGTranslateTests::test_getIdsWithPrefix()
{
    FGTestApi::setUp::initTestGlobals("test_getIdsWithPrefix", "en_US");

    const auto dir = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "Aircraft" / "Test";
    fgSetString("/sim/aircraft-dir"s, dir.utf8Str());

    globals->get_locale()->loadAircraftTranslations();
    auto tr = FGTranslate("current-aircraft");

    // "care-package" also matches prefix "car", since matching is done on
    // characters, not on some notion of word boundary.
    std::vector<std::string> v = {"car-brand", "car-model", "care-package"};
    std::vector<std::string> v2 = tr.getIdsWithPrefix("some-resource", "car");
    CPPUNIT_ASSERT(std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend()));

    // Exact, single match
    v = {"cats"};
    v2 = tr.getIdsWithPrefix("some-resource", "cats");
    CPPUNIT_ASSERT(std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend()));

    // No match
    v2 = tr.getIdsWithPrefix("some-resource", "zzz-non-existent");
    CPPUNIT_ASSERT(v2.empty());

    // Empty prefix: all ids of the resource, sorted, without duplicates
    v = {"car-brand", "car-model", "care-package", "cats", "dog", "hello"};
    v2 = tr.getIdsWithPrefix("some-resource", "");
    CPPUNIT_ASSERT(std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend()));

    // The "sentence" basicId has three translation units (indices 0 to 2) in
    // the "dialog-whatever" resource; it must only be reported once.
    v = {"sentence"};
    v2 = tr.getIdsWithPrefix("dialog-whatever", "sent");
    CPPUNIT_ASSERT(std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend()));

    // Non-existent resource
    v2 = tr.getIdsWithPrefix("non-existent-resource", "");
    CPPUNIT_ASSERT(v2.empty());
}

void FGTranslateTests::test_addonDomain()
{
    FGTestApi::setUp::initTestGlobals("test_addonDomain", "en_US");

    const auto& addonManager = AddonManager::createInstance();
    const auto dir = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "Add-ons" / "Test";
    addonManager->registerAddon(dir);

    globals->get_locale()->loadAddonTranslations();
    auto tr = FGTranslate("addons/org.flightgear.TestSuite.addons.Test");

    string fetched = tr.get("foo", "hello");
    CPPUNIT_ASSERT_EQUAL("Hello from the Test dummy add-on!"s, fetched);

    fetched = tr.get("foo", "question");
    CPPUNIT_ASSERT_EQUAL("What also floats in water?"s, fetched);

    fetched = tr.get("dialog-sample", "a-simple-label");
    CPPUNIT_ASSERT_EQUAL("A simple label"s, fetched);

    // A string with multiple indices
    CPPUNIT_ASSERT_EQUAL(std::size_t(3),
                         tr.getCount("dialog-sample", "sample-text"));
    std::vector<std::string> v = {
        "First sample text", "Second sample text", "Third sample text"
    };

    std::vector<std::string> v2 = tr.getAll("dialog-sample", "sample-text");
    bool equal = std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend());
    CPPUNIT_ASSERT(equal);

    // Change the selected language to French
    globals->get_locale()->selectLanguage("fr");
    globals->get_locale()->loadAddonTranslations();
    tr = FGTranslate("addons/org.flightgear.TestSuite.addons.Test");

    fetched = tr.get("foo", "hello");
    CPPUNIT_ASSERT_EQUAL("Bonjour depuis le greffon bidon Test !"s, fetched);

    fetched = tr.get("foo", "question");
    CPPUNIT_ASSERT_EQUAL("Qu'est-ce qui flotte également sur l'eau ?"s, fetched);

    fetched = tr.get("dialog-sample", "a-simple-label");
    CPPUNIT_ASSERT_EQUAL("Une simple étiquette"s, fetched);

    // A string with multiple indices
    CPPUNIT_ASSERT_EQUAL(std::size_t(3),
                         tr.getCount("dialog-sample", "sample-text"));
    v = {
        "Premier texte d'exemple",
        "Deuxième texte d'exemple",
        "Troisième texte d'exemple"
    };

    v2 = tr.getAll("dialog-sample", "sample-text");
    equal = std::equal(v.cbegin(), v.cend(), v2.cbegin(), v2.cend());
    CPPUNIT_ASSERT(equal);

    addonManager->reset(); // destroy the AddonManager
}

// Subroutine called by tests that are run with different FGLocale settings
void FGTranslateTests::NasalAPI_languageIndependentTests()
{
    std::string badCode = R"(
        var tr = FGTranslate.new("current-aircraft");
        # This string has plural status true...
        var translUnit = tr.translationUnit("some-resource", "cats");
        # ... therefore the cardinalNumber argument is missing here:
        translUnit.getTranslation();
    )";
    bool ok = FGTestApi::executeNasal(badCode);
    CPPUNIT_ASSERT(!ok);

    badCode = R"(
        var tr = FGTranslate.new("current-aircraft");
        # This string has plural status false...
        var translUnit = tr.translationUnit("some-resource", "hello");
        # ... therefore no argument must be provided here:
        translUnit.getTranslation(2);
    )";
    ok = FGTestApi::executeNasal(badCode);
    CPPUNIT_ASSERT(!ok);
}

void FGTranslateTests::test_NasalAPI_en_US()
{
    FGTestApi::setUp::initTestGlobals("test_NasalAPI_en_US", "en_US");

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();
    FGTestApi::setUp::initStandardNasal();
    globals->get_subsystem_mgr()->postinit();

    const auto dir = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "Aircraft" / "Test";
    fgSetString("/sim/aircraft-dir"s, dir.utf8Str());

    globals->get_locale()->loadAircraftTranslations();

    bool ok = FGTestApi::executeNasal(R"(
        var tr = FGTranslate.new();
        unitTest.assert_equal("Exit",
                              tr.get("dialog-exit", "exit-button-label"));

        var tr = FGTranslate.new("current-aircraft");
        unitTest.assert_equal("Hello from the Test aircraft!",
                              tr.get("some-resource", "hello"));

        unitTest.assert_equal("%1 cats is not enough cats.",
                              tr.getPlural(0, "some-resource", "cats"));
        unitTest.assert_equal("%1 cat is not enough cats.",
                              tr.getPlural(1, "some-resource", "cats"));
        unitTest.assert_equal("%1 cats is not enough cats.",
                              tr.getPlural(2, "some-resource", "cats"));

        # Prepare for testing getAll() and getCount()
        var expectedVec = [
          "Make sure the Prince doesn't leave this room until I come and get him.",
          "Not to leave the room even if you come and get him.",
          "No, no. Until I come and get him.",
        ];
        var nbSentences = size(expectedVec);

        var v = tr.getAll("dialog-whatever", "sentence");
        unitTest.assert_equal(nbSentences, size(v));

        unitTest.assert_equal(nbSentences,
                              tr.getCount("dialog-whatever", "sentence"));

        # Test FGTranslate.get() with the optional argument provided (an index)
        for (var i = 0; i < nbSentences; i += 1) {
          unitTest.assert_equal(expectedVec[i],
                                tr.get("dialog-whatever", "sentence", i));
        }

        # Test FGTranslate.getWithDefault()
        unitTest.assert_equal("Hello from the Test aircraft!",
                              tr.getWithDefault("some-resource", "hello",
                                                "the default value"));
        unitTest.assert_equal("the default value",
                              tr.getWithDefault("some-resource", "non-existent",
                                                "the default value"));

        unitTest.assert_equal(expectedVec[2],
                              tr.getWithDefault("dialog-whatever", "sentence",
                                                "the default value", 2));
        unitTest.assert_equal("the default value",
                              tr.getWithDefault("dialog-whatever", "non-existent",
                                                "the default value", 2));

        # Test FGTranslate.getPluralWithDefault()
        unitTest.assert_equal(
          "%1 cats is not enough cats.",
          tr.getPluralWithDefault(0, "some-resource", "cats", "default"));
        unitTest.assert_equal(
          "%1 cat is not enough cats.",
          tr.getPluralWithDefault(1, "some-resource", "cats", "default"));
        unitTest.assert_equal(
          "default",
          tr.getPluralWithDefault(0, "some-resource", "non-existent", "default"));
        unitTest.assert_equal(
          "default",
          tr.getPluralWithDefault(1, "some-resource", "non-existent", "default"));

        # This string isn't defined with has-plural="true"
        var translUnit = tr.translationUnit("some-resource", "hello");
        unitTest.assert_equal(0, translUnit.pluralStatus);

        # This string is defined with has-plural="true"
        var translUnit = tr.translationUnit("some-resource", "cats");

        unitTest.assert_equal(
          "%1 cat(s) is not enough cats.", translUnit.sourceText);
        unitTest.assert_equal(1, translUnit.pluralStatus);
        # 2 plural forms in English: singular and plural
        unitTest.assert_equal(2, translUnit.nbTargetTexts);

        unitTest.assert_equal(
          "%1 cat is not enough cats.", translUnit.getTargetText(0));
        unitTest.assert_equal(
          "%1 cats is not enough cats.", translUnit.getTargetText(1));

        unitTest.assert_equal("%1 cats is not enough cats.",
                              translUnit.getTranslation(0));
        unitTest.assert_equal("%1 cat is not enough cats.",
                              translUnit.getTranslation(1));
        unitTest.assert_equal("%1 cats is not enough cats.",
                              translUnit.getTranslation(2));

        # FGTranslate.translationUnit() with the optional index argument
        var translUnit = tr.translationUnit("dialog-whatever", "sentence", 2);
        unitTest.assert_equal("No, no. Until I come and get him.",
                              translUnit.getTranslation());
    )");
    CPPUNIT_ASSERT(ok);

    NasalAPI_languageIndependentTests();
}

void FGTranslateTests::test_NasalAPI_fr_FR()
{
    FGTestApi::setUp::initTestGlobals("test_NasalAPI_fr_FR", "fr_FR");

    globals->get_subsystem_mgr()->bind();
    globals->get_subsystem_mgr()->init();
    FGTestApi::setUp::initStandardNasal();
    globals->get_subsystem_mgr()->postinit();

    const auto dir = SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "Aircraft" / "Test";
    fgSetString("/sim/aircraft-dir"s, dir.utf8Str());

    globals->get_locale()->loadAircraftTranslations();

    bool ok = FGTestApi::executeNasal(R"(
        var tr = FGTranslate.new();
        unitTest.assert_equal("Quitter",
                              tr.get("dialog-exit", "exit-button-label"));

        var tr = FGTranslate.new("current-aircraft");
        unitTest.assert_equal("Bonjour depuis l'aéronef Test !",
                              tr.get("some-resource", "hello"));

        unitTest.assert_equal("%1 chat, ce n'est pas assez de chats.",
                              tr.getPlural(0, "some-resource", "cats"));
        unitTest.assert_equal("%1 chat, ce n'est pas assez de chats.",
                              tr.getPlural(1, "some-resource", "cats"));
        unitTest.assert_equal("%1 chats, ce n'est pas assez de chats.",
                              tr.getPlural(2, "some-resource", "cats"));

        # Prepare for testing getAll() and getCount()
        var expectedVec = [
          "Assurez-vous que le prince ne quitte pas cette pièce avant que je ne revienne le checher.",
          "Ne pas quitter la pièce même si vous revenez le chercher.",
          "Non, non. Jusqu'à ce que je revienne le chercher.",
        ];
        var nbSentences = size(expectedVec);

        var v = tr.getAll("dialog-whatever", "sentence");
        unitTest.assert_equal(nbSentences, size(v));

        unitTest.assert_equal(nbSentences,
                              tr.getCount("dialog-whatever", "sentence"));

        # Test FGTranslate.get() with the optional argument provided (an index)
        for (var i = 0; i < nbSentences; i += 1) {
          unitTest.assert_equal(expectedVec[i],
                                tr.get("dialog-whatever", "sentence", i));
        }

        # Test FGTranslate.getWithDefault()
        unitTest.assert_equal("Bonjour depuis l'aéronef Test !",
                              tr.getWithDefault("some-resource", "hello",
                                                "the default value"));
        unitTest.assert_equal("the default value",
                              tr.getWithDefault("some-resource", "non-existent",
                                                "the default value"));

        unitTest.assert_equal(expectedVec[2],
                              tr.getWithDefault("dialog-whatever", "sentence",
                                                "the default value", 2));
        unitTest.assert_equal("the default value",
                              tr.getWithDefault("dialog-whatever", "non-existent",
                                                "the default value", 2));

        # Test FGTranslate.getPluralWithDefault()
        unitTest.assert_equal(
          "%1 chat, ce n'est pas assez de chats.",
          tr.getPluralWithDefault(0, "some-resource", "cats", "default"));
        unitTest.assert_equal(
          "%1 chats, ce n'est pas assez de chats.",
          tr.getPluralWithDefault(2, "some-resource", "cats", "default"));
        unitTest.assert_equal(
          "default",
          tr.getPluralWithDefault(0, "some-resource", "non-existent", "default"));
        unitTest.assert_equal(
          "default",
          tr.getPluralWithDefault(2, "some-resource", "non-existent", "default"));

        # This string isn't defined with has-plural="true"
        var translUnit = tr.translationUnit("some-resource", "hello");
        unitTest.assert_equal(0, translUnit.pluralStatus);

        # This string is defined with has-plural="true"
        var translUnit = tr.translationUnit("some-resource", "cats");

        unitTest.assert_equal(
          "%1 cat(s) is not enough cats.", translUnit.sourceText);
        unitTest.assert_equal(1, translUnit.pluralStatus);
        # 2 plural forms in French: singular and plural
        unitTest.assert_equal(2, translUnit.nbTargetTexts);

        unitTest.assert_equal(
          "%1 chat, ce n'est pas assez de chats.", translUnit.getTargetText(0));
        unitTest.assert_equal(
          "%1 chats, ce n'est pas assez de chats.", translUnit.getTargetText(1));

        unitTest.assert_equal("%1 chat, ce n'est pas assez de chats.",
                              translUnit.getTranslation(0));
        unitTest.assert_equal("%1 chat, ce n'est pas assez de chats.",
                              translUnit.getTranslation(1));
        unitTest.assert_equal("%1 chats, ce n'est pas assez de chats.",
                              translUnit.getTranslation(2));

        # FGTranslate.translationUnit() with the optional index argument
        var translUnit = tr.translationUnit("dialog-whatever", "sentence", 2);
        unitTest.assert_equal(
          "Non, non. Jusqu'à ce que je revienne le chercher.",
          translUnit.getTranslation());
    )");
    CPPUNIT_ASSERT(ok);

    NasalAPI_languageIndependentTests();
}

/* codespell:ignore-end */

void FGTranslateTests::tearDown()
{
    FGTestApi::tearDown::shutdownTestGlobals();
}
