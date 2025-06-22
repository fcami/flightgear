/*
 * SPDX-FileName: StringTemplate.cxx
 * SPDX-FileComment: format strings with flightgear properties
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "StringInterpolation.hxx"
#include <Main/fg_props.hxx>

class PropertyStringInterpolatorEvaluator : public StringInterpolatorEvaluator
{
public:
    PropertyStringInterpolatorEvaluator(const std::string & tmpl ) : StringInterpolatorEvaluator(tmpl) {}
    virtual std::string eval()
    {
        auto n = fgGetNode(_tmpl);
        if( n ) return n->getStringValue();
        else return std::string();
    }
};

class NasalStringInterpolatorEvaluator : public StringInterpolatorEvaluator
{
public:
    NasalStringInterpolatorEvaluator(const std::string& tmpl)
        : StringInterpolatorEvaluator(tmpl),
          _nasal(globals->get_subsystem<FGNasalSys>())
    {
        if (!_nasal) {
            throw sg_exception("Nasal not available");
        }
    }

    virtual std::string eval()
    {
        std::string output, parseErrors;
        bool ok = _nasal->parseAndRunWithOutput(_tmpl, output, parseErrors);
        if (!ok) {
            SG_LOG(SG_NASAL, SG_ALERT, "Nasal parse errors:" << parseErrors);
            return {};
        }

        return output;
    }

private:
    FGNasalSys * _nasal;
};

std::shared_ptr<StringInterpolatorEvaluator> StringInterpolatorEvaluator::create(const std::string& tmpl)
{
    if (simgear::strutils::starts_with(tmpl, "prop:"))
        return std::make_shared<PropertyStringInterpolatorEvaluator>(tmpl.substr(5));
    else if (simgear::strutils::starts_with(tmpl, "nasal:"))
        return std::make_shared<NasalStringInterpolatorEvaluator>(tmpl.substr(6));
    else
        return std::make_shared<PropertyStringInterpolatorEvaluator>(tmpl);
}


std::string StringInterpolator::interpolate(const std::string& format)
{
    StringInterpolator interpolator(format);
    return interpolator.interpolate();
}


std::string StringInterpolator::interpolate() const
{
    std::string_view _formatView(_format);
    std::string result;
    size_t pos = 0;

    while (pos < _formatView.size()) {
        size_t start = _formatView.find("${", pos);
        if (start == std::string_view::npos) {
            result.append(_formatView.substr(pos));
            break;
        }

        // Append everything before ${
        result.append(_formatView.substr(pos, start - pos));

        // Find the end of the variable expression by matching the first unescaped '}'
        size_t brace_depth = 0;
        size_t i = start + 2;
        for (; i < _formatView.size(); ++i) {
            if (_formatView[i] == '{') {
                ++brace_depth;
            } else if (_formatView[i] == '}') {
                if (brace_depth == 0)
                    break;
                else
                    --brace_depth;
            }
        }

        if (i >= _formatView.size()) {
            // No matching closing }, treat as literal
            result.append(_formatView.substr(start));
            break;
        }

        // Extract variable name between ${ and matching }
        std::string_view key = _formatView.substr(start + 2, i - (start + 2));
        result += StringInterpolatorEvaluator::create(std::string(key))->eval();

        pos = i + 1; // advance position
    }

    return result;
}