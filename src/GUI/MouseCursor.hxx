// MouseCursor.hxx - abstract interface for mouse cursor control

// SPDX-FileCopyrightText: 2013 James Turner <zakalawe@mac.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

class SGPropertyNode;

class FGMouseCursor
{
public:
    static FGMouseCursor* instance();

    virtual void setAutoHideTimeMsec(unsigned int aMsec);

    enum Cursor {
        CURSOR_NONE = 0,
        CURSOR_ARROW,
        CURSOR_HAND, ///< the browser 'link' cursor
        CURSOR_CLOSED_HAND,
        CURSOR_CROSSHAIR,
        CURSOR_IBEAM,  ///< for editing text
        CURSOR_IN_OUT, ///< arrow pointing into / out of the screen
        CURSOR_LEFT_RIGHT,
        CURSOR_UP_DOWN,
        CURSOR_LEFT_SIDE,
        CURSOR_RIGHT_SIDE,
        CURSOR_TOP_SIDE,
        CURSOR_BOTTOM_SIDE,
        CURSOR_TOP_LEFT,
        CURSOR_TOP_RIGHT,
        CURSOR_BOTTOM_LEFT,
        CURSOR_BOTTOM_RIGHT,
        CURSOR_SPIN_CW,
        CURSOR_SPIN_CCW,
        CURSOR_WAIT
    };

    virtual void setCursor(Cursor aCursor) = 0;

    virtual void setCursorVisible(bool aVis) = 0;

    virtual void hideCursorUntilMouseMove() = 0;

    virtual void mouseMoved() = 0;

    static Cursor cursorFromString(const std::string& str);

    virtual Cursor getCursor() const;

protected:
    FGMouseCursor();

    bool setCursorCommand(const SGPropertyNode* arg, SGPropertyNode*);

    unsigned int mAutoHideTimeMsec;

    Cursor m_currentCursor = CURSOR_ARROW;
};
