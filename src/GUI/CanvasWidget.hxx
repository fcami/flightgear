// CanvasWidget.hxx - XML dialog canvas widget
// SPDX-FileCopyrightText: 2025 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/canvas/canvas_fwd.hxx>

#include <GUI/PUICompatObject.hxx>

class CanvasWidget : public PUICompatObject
{
public:
    CanvasWidget(naRef impl);
    virtual ~CanvasWidget();

    static void setupGhost(nasal::Hash& guiModule);

    void init() override;

private:
    void createChildCanvas(int width, int height);

    simgear::canvas::CanvasPtr getCanvas() const;

    simgear::canvas::CanvasPtr _canvas;
};

using CanvasWidgetRef = SGSharedPtr<CanvasWidget>;
