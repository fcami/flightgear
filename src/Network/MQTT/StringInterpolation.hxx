/*
 * SPDX-FileName: StringTemplate.hxx
 * SPDX-FileComment: format strings with flightgear properties
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once
#include <string>

class StringInterpolatorEvaluator
{
public:
    StringInterpolatorEvaluator(const std::string& tmpl) : _tmpl(tmpl) {}
    virtual ~StringInterpolatorEvaluator() = default;
    virtual std::string eval() = 0;
    static std::shared_ptr<StringInterpolatorEvaluator> create(const std::string& tmpl);

private:
    StringInterpolatorEvaluator();

protected:
    std::string _tmpl;
};

class StringInterpolator
{
public:
    StringInterpolator(const std::string& format) : _format(format) {}
    virtual ~StringInterpolator() = default;

    std::string interpolate() const;
    static std::string interpolate(const std::string& format);

private:
    std::string _format;
};