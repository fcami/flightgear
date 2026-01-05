// SPDX-FileCopyrightText: 2026 Florent Rougon
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief String formatting function for Nasal similar to std::format()
 *
 * Synopsis: format(fmt_string, args...)
 */

#include <format>
#include <locale>
#include <string>
#include <string_view>
#include <utility>

#include <simgear/nasal/cppbind/NasalCallContext.hxx>

#include <Main/globals.hxx>

#include "NasalStringFormatting.hxx"

using std::string;
using std::string_view;

namespace nasal {

struct FieldIndexingStyles {
    bool implicitIndexing;
    bool explicitIndexing;
};

struct ReadFieldResult {
    std::string expansionResult;
    string::size_type endIndex; // after the closing brace
};

struct ParsedFormatSpec {
    ParsedFormatSpec() : localeSpecificFormatting(false), typeSpecifier('s'),
                         endIndex(0)
    {
    }

    std::string formatSpec;
    bool localeSpecificFormatting; // true if the 'L' specifier is in formatSpec
    char typeSpecifier;            // either implicit or at the end of formatSpec
    string::size_type endIndex;    // points to “after the closing brace”
};

static ReadFieldResult readField(
    const nasal::CallContext& ctx, const string& fmt,
    string::size_type startIndex, std::size_t& autoArgIndex,
    FieldIndexingStyles& fieldIndexingStylesFound);

// Auxiliary function for format_readFormatSpec(). Has several I/O parameters!
static string::size_type readWidthOrPrecision(
    string& formatSpec, const nasal::CallContext& ctx, const string& fmt,
    string::size_type fmtSize, string::size_type i, std::size_t& autoArgIndex,
    FieldIndexingStyles& fieldIndexingStylesFound)
{
    if (i < fmtSize && fmt[i] == '{') {
        // width or precision specifier given as a nested field
        const auto tmp = readField(ctx, fmt, i + 1, autoArgIndex,
                                   fieldIndexingStylesFound);
        formatSpec += std::move(tmp.expansionResult);
        return tmp.endIndex; // after the closing brace
    } else {
        // width or precision specifier either absent or given literally
        while (i < fmtSize && fmt[i] >= '0' && fmt[i] <= '9') {
            formatSpec += fmt[i++];
        }
        return i;
    }
}

// Reference: https://en.cppreference.com/w/cpp/utility/format/spec.html
static ParsedFormatSpec readFormatSpec(
    const nasal::CallContext& ctx, const string& fmt,
    string::size_type startIndex, std::size_t& autoArgIndex,
    FieldIndexingStyles& fieldIndexingStylesFound)
{
    ParsedFormatSpec res;

    auto i = startIndex;
    const auto fmtSize = fmt.size();
    if (i >= fmtSize) {
        // i > 0 because we read at least an opening brace
        ctx.runtimeError("format(): format string ends in unfinished field "
                         "at index %d",
                         i - 1);
    }

    if (fmt[i] == '}') {    // most common case, handle it right away
        res.endIndex = ++i; // skip past the closing brace
        return res;
    } else if (i + 1 >= fmtSize) { // no space left for a closing brace
        ctx.runtimeError("format(): format string ends in unfinished field "
                         "at index %d",
                         i);
    }

    // At least two chars can be read starting from fmt[i].
    if (fmt[i + 1] == '<' || fmt[i + 1] == '>' || fmt[i + 1] == '^') {
        // fmt[i] is the fill character and fmt[i + 1] the align specifier
        res.formatSpec += string_view(&fmt[i], 2);
        i += 2;
    } else if (fmt[i] == '<' || fmt[i] == '>' || fmt[i] == '^') {
        // align specifier but no fill character
        res.formatSpec += fmt[i++];
    }

    if (i < fmtSize && (fmt[i] == '+' || fmt[i] == '-' || fmt[i] == ' ')) {
        res.formatSpec += fmt[i++]; // fmt[i] is the 'sign' option
    }

    if (i < fmtSize && fmt[i] == '#') {
        res.formatSpec += fmt[i++]; // fmt[i] is the '#' option
    }

    if (i < fmtSize && fmt[i] == '0') {
        res.formatSpec += fmt[i++]; // fmt[i] is the '0' option
    }

    // Read the width, if any.
    i = readWidthOrPrecision(
        res.formatSpec /* I/O */, ctx, fmt, fmtSize, i, autoArgIndex /* I/O */,
        fieldIndexingStylesFound /* I/O */);

    // Read the precision, if any.
    if (i < fmtSize && fmt[i] == '.') {
        res.formatSpec += fmt[i++];
        i = readWidthOrPrecision(
            res.formatSpec /* I/O */, ctx, fmt, fmtSize, i,
            autoArgIndex /* I/O */, fieldIndexingStylesFound /* I/O */);
    }

    if (i < fmtSize && fmt[i] == 'L') {
        res.formatSpec += fmt[i++]; // fmt[i] is the 'L' option
        res.localeSpecificFormatting = true;
    }

    if (i < fmtSize && fmt[i] != '}') {
        res.typeSpecifier = fmt[i++]; // fmt[i] must be the type specifier
        res.formatSpec += res.typeSpecifier;
    }

    if (i >= fmtSize) {
        ctx.runtimeError("format(): format string ends with an unfinished "
                         "field at index %d",
                         i - 1);
    } else if (fmt[i] == '}') {
        res.endIndex = ++i; // skip past the closing brace
    } else {
        ctx.runtimeError("format(): unexpected format spec character '%c' "
                         "(0x%02x) at index %d of the format string",
                         fmt[i], fmt[i], i);
    }

    return res;
}

[[nodiscard]]
static bool isStringPresentationType(char t) noexcept
{
    return (t == 's' || t == '?');
}

[[nodiscard]]
static bool isIntegerPresentationType(char t) noexcept
{
    return (t == 'b' || t == 'B' || t == 'c' || t == 'd' || t == 'o' ||
            t == 'x' || t == 'X');
}

[[nodiscard]]
static bool isFloatingPointPresentationType(char t) noexcept
{
    return (t == 'a' || t == 'A' || t == 'e' || t == 'E' ||
            t == 'f' || t == 'F' || t == 'g' || t == 'G');
}

template <class T>
[[nodiscard]]
static string formatOneArg(const ParsedFormatSpec& spec, const T& argument)
{
    return spec.localeSpecificFormatting
               ? std::vformat(std::locale(""), // initialized from the environment
                              "{:" + spec.formatSpec + "}",
                              std::make_format_args(argument))
               : std::vformat("{:" + spec.formatSpec + "}",
                              std::make_format_args(argument));
}

static ReadFieldResult readField(
    const nasal::CallContext& ctx, const string& fmt,
    string::size_type startIndex, std::size_t& autoArgIndex,
    FieldIndexingStyles& fieldIndexingStylesFound)
{
    ReadFieldResult res;
    std::size_t nasalArgIndex;
    auto i = startIndex;

    if (i >= fmt.size()) {
        ctx.runtimeError("format(): format string ends with an unfinished "
                         "field at index %d",
                         i);
    }

    const auto endPos = fmt.find_first_of(":}", i);
    if (endPos == string::npos) {
        ctx.runtimeError("format(): unable to find field number end "
                         "delimiter ':' or '}' ");
    }

    const string argNum_s = fmt.substr(i, endPos - i);
    if (argNum_s.empty()) { // implicit field number
        nasalArgIndex = autoArgIndex++;
        fieldIndexingStylesFound.implicitIndexing = true;
    } else {
        nasalArgIndex = 1 + std::stoul(argNum_s);
        fieldIndexingStylesFound.explicitIndexing = true;
    }

    const string::size_type startOfFormatSpec = (fmt[endPos] == ':' ? endPos + 1
                                                                    : endPos);
    // The last two are I/O parameters
    const auto spec = readFormatSpec(ctx, fmt, startOfFormatSpec,
                                     autoArgIndex, fieldIndexingStylesFound);

    // Don't try to std::move() the arg, as std::make_format_args() takes
    // its arguments as non-const lvalue references.
    if (isStringPresentationType(spec.typeSpecifier)) {
        // Possible enhancement: if the argument is an object that has, say,
        // an __str__() method, we could call it.
        const auto arg = ctx.requireArg<string>(nasalArgIndex);
        res.expansionResult = formatOneArg(spec, arg);
    } else if (isIntegerPresentationType(spec.typeSpecifier)) {
        const auto arg = ctx.requireArg<long long>(nasalArgIndex);
        res.expansionResult = formatOneArg(spec, arg);
    } else if (isFloatingPointPresentationType(spec.typeSpecifier)) {
        const auto arg = ctx.requireArg<double>(nasalArgIndex); // long double?
        res.expansionResult = formatOneArg(spec, arg);
    }

    res.endIndex = spec.endIndex;
    return res;
}

// Implementation of the Nasal format() function
static naRef f_format(const nasal::CallContext& ctx)
{
    const auto fmt = ctx.requireArg<string>(0); // the format string
    FieldIndexingStyles fieldIndexingStylesFound = {false, false};
    // This will give the index of requested Nasal format() arguments for
    // fields with no arg-id. Start at 1 because argument 0 is the format
    // string.
    std::size_t autoArgIndex = 1;
    string res;            // will contain the desired formatted string
    res.reserve(12);       // should be enough for “many cases”
    string::size_type i{}; // current position inside 'fmt'

    while (i < fmt.size()) {
        switch (fmt[i]) {
        case '{':
            if (i + 1 < fmt.size()) {
                if (fmt[i + 1] == '{') { // "{{" escape sequence
                    res += '{';
                    i += 2;
                } else {
                    const auto tmp = readField(ctx, fmt, i + 1, autoArgIndex,
                                               fieldIndexingStylesFound);
                    i = tmp.endIndex; // after the closing brace
                    res += std::move(tmp.expansionResult);
                }
            } else {
                ctx.runtimeError("format(): lonely '{' at the end of the "
                                 "format string");
            }
            break;
        case '}':
            if (i + 1 < fmt.size() && fmt[i + 1] == '}') {
                res += '}';
                i += 2;
            } else {
                ctx.runtimeError("format(): '}' with no matching '{' "
                                 "at index %d of the format string",
                                 i);
            }
            break;
        default:
            res += fmt[i++]; // non-special character; just append it
        }
    }

    if (fieldIndexingStylesFound.implicitIndexing &&
        fieldIndexingStylesFound.explicitIndexing) {
        ctx.runtimeError("format(): arg-ids must be all present or all "
                         "omitted (e.g., {1}...{0} or {}...{})");
    }

    return ctx.to_nasal(std::move(res));
}

void addStringFormattingFunction()
{
    const auto nasalSys = globals->get_subsystem<FGNasalSys>();
    auto globalsModule = nasalSys->getGlobals();
    globalsModule.set("format", f_format);
}

} // namespace nasal
