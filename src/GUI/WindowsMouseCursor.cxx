// SPDX-FileCopyrightText: 2013 James Turner <zakalawe@mac.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "WindowsMouseCursor.hxx"

#include "windows.h"

#include <map>

#include <Main/globals.hxx>

class WindowsMouseCursor::WindowsMouseCursorPrivate
{
public:
    Cursor activeCursorKey;
    bool hideUntilMove;

    typedef std::map<Cursor, HCURSOR> CursorMap;
    CursorMap cursors;
};

HCURSOR windowsCursorForKey(FGMouseCursor::Cursor aKey)
{
    switch (aKey) {
    case FGMouseCursor::CURSOR_HAND: return LoadCursor(NULL, IDC_HAND);
    case FGMouseCursor::CURSOR_CROSSHAIR: return LoadCursor(NULL, IDC_CROSS);
    case FGMouseCursor::CURSOR_IBEAM: return LoadCursor(NULL, IDC_IBEAM);
    case FGMouseCursor::CURSOR_LEFT_RIGHT: return LoadCursor(NULL, IDC_SIZEWE);
    default: return LoadCursor(NULL, IDC_ARROW);
    }
}

WindowsMouseCursor::WindowsMouseCursor() :
    d(new WindowsMouseCursorPrivate)
{
    d->hideUntilMove = false;
	d->activeCursorKey = CURSOR_ARROW;
}

WindowsMouseCursor::~WindowsMouseCursor()
{
}


void WindowsMouseCursor::setCursor(Cursor aCursor)
{
    if (aCursor == d->activeCursorKey) {
        return;
    }

    d->activeCursorKey = aCursor;
    if (d->cursors.find(aCursor) == d->cursors.end()) {
        d->cursors[aCursor] = windowsCursorForKey(aCursor);
    }

    SetCursor(d->cursors[aCursor]);
}

void WindowsMouseCursor::setCursorVisible(bool aVis)
{
    d->hideUntilMove = false; // cancel this
    ShowCursor(aVis);
}

void WindowsMouseCursor::hideCursorUntilMouseMove()
{
    if (d->hideUntilMove) {
        return;
    }

    d->hideUntilMove = true;
    ShowCursor(false);
}

void WindowsMouseCursor::mouseMoved()
{
    if (d->hideUntilMove) {
        d->hideUntilMove = false;
        ShowCursor(true);
    }
}
