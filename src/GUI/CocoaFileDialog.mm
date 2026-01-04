// CocoaFileDialog.mm - Cocoa implementation of file-dialog interface
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2013 James Turner <james@flightgear.org>

#include "CocoaFileDialog.hxx"

// Apple
#include <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "GUI/FileDialog.hxx"
#include "simgear/debug/debug_types.h"
#include "simgear/math/sg_types.hxx"

#include <AppKit/NSSavePanel.h>
#include <AppKit/NSOpenPanel.h>

#include <osgViewer/Viewer>
#include <osgViewer/api/Cocoa/GraphicsWindowCocoa>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/strutils.hxx>

#include <GUI/CocoaHelpers_private.h>
#include <Main/globals.hxx>
#include <Main/fg_props.hxx>
#include <Viewer/renderer.hxx>

class CocoaFileDialog::CocoaFileDialogPrivate
{
public:
  CocoaFileDialogPrivate() = default;

  ~CocoaFileDialogPrivate() { [panel release]; }

  NSSavePanel *panel = nil;

  NSArray<UTType *> *buildContentTypes(const string_list &patterns) {
    // important that we return an empty array (and not `nil`) if
    // patterns is empty, since that corresponds to 'all types allowed'
    auto *types = [[NSMutableArray<UTType *> new] autorelease];
    for (auto p : patterns) {
      if (!simgear::strutils::starts_with(p, "*.")) {
        SG_LOG(SG_GUI, SG_ALERT, "can't use pattern on Cocoa:" << p);
        continue;
      }

      auto cocoaFileExtension = stdStringToCocoa(p.substr(2));
      auto *utType = [UTType typeWithFilenameExtension:cocoaFileExtension];
      [types addObject:utType];
    }

    return types;
  }
};

CocoaFileDialog::CocoaFileDialog(FGFileDialog::Usage use) :
    FGFileDialog(use)
{
    d.reset(new CocoaFileDialogPrivate);
    if (use == USE_SAVE_FILE) {
        d->panel = [NSSavePanel savePanel];
    } else {
        NSOpenPanel* openPanel = [NSOpenPanel openPanel];
        d->panel = openPanel;

        if (use == USE_CHOOSE_DIR) {
            [openPanel setCanChooseDirectories:YES];
        }
    } // of USE_OPEN_FILE or USE_CHOOSE_DIR -> building NSOpenPanel

    [d->panel retain];
}

CocoaFileDialog::~CocoaFileDialog()
{

}

void CocoaFileDialog::exec()
{
// find the native Cocoa NSWindow handle so we can parent the dialog and show
// it window-modal.
    NSWindow* cocoaWindow = nil;
    std::vector<osgViewer::GraphicsWindow*> windows;
    globals->get_renderer()->getViewerBase()->getWindows(windows);

    for (auto gw : windows) {
        // OSG doesn't use RTTI, so no dynamic cast. Let's check the class type
        // using OSG's own system, before we blindly static_cast<> and break
        // everything.
        if (strcmp(gw->className(), "GraphicsWindowCocoa")) {
            continue;
        }

        osgViewer::GraphicsWindowCocoa* gwCocoa = static_cast<osgViewer::GraphicsWindowCocoa*>(gw);
        cocoaWindow = (NSWindow*) gwCocoa->getWindow();
        break;
    }

// setup the panel fields now we have collected all the data
    if (_usage == USE_SAVE_FILE) {
        [d->panel setNameFieldStringValue:stdStringToCocoa(_placeholder)];
    }

    // if _filterPatterns is empty, we want an empty content types array to
    // indicate 'any file type' as per the NSSavePanel docs
    [d->panel setAllowedContentTypes:d->buildContentTypes(_filterPatterns)];
    [d->panel setTitle:stdStringToCocoa(_title)];

    if (_showHidden) {
        [d->panel setShowsHiddenFiles:YES];
    }

    [d->panel setDirectoryURL: pathToNSURL(_initialPath)];

    NSInteger result = [d->panel runModal];
    if (result == NSModalResponseOK) {
      NSString *nspath = [[d->panel URL] path];
      // NSLog(@"the URL is: %@", d->panel URL]);
      auto p = SGPath::fromUtf8([nspath UTF8String]);
      handleSelectedPath(p);
    }
}

void CocoaFileDialog::close()
{
    [d->panel close];
}
