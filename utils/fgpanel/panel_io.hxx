// SPDX-FileCopyrightText: 2000 David Megginson
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef __PANEL_IO_HXX
#define __PANEL_IO_HXX

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif

#include "FGPanel.hxx"

class FGReadablePanel : public FGPanel {
public:
    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "readable-panel"; }

    static SGSharedPtr<FGPanel> read (SGPropertyNode_ptr root);
};

#endif // __PANEL_IO_HXX
