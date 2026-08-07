// QtLauncher.hxx - GUI launcher dialog using Qt5
//
// Written by James Turner, started December 2014.
//
// SPDX-FileCopyrightText: 2014 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

class SGPath;

namespace flightgear
{
  // Only requires FGGlobals to be initialized if 'doInitQSettings' is true.
  // Safe to call several times.
  void initApp(int& argc, char** argv, bool doInitQSettings = true);

  // ensures Qt-related resources are cleaned up. Avoids crashes on shutdown
  // if QPA assets are hanging around. (With the XCB QPA plugin especially)
  void shutdownQtApp();

  // Requires FGGlobals to be initialized. Safe to call several times.
  void initQSettings();

  /**
   * @brief switch the installed launcher/simulator-UI (Qt qsTr()) translator
   * to match @p code, removing any previously installed dynamic translator
   * first. An empty @p code means "use the detected system language".
   */
  void switchUILanguage(const std::string& code);

  // Set 'fg-root' in QSettings to the special value "!ask" if either Alt or
  // Shift is pressed. Return true in this case, false otherwise.
  bool checkKeyboardModifiersForSettingFGRoot();

  bool runLauncherDialog();

  bool runInAppLauncherDialog();

  enum LockFileDialogResult
  {
    LockFileContinue,
    LockFileReset,
    LockFileQuit
  };

  LockFileDialogResult showLockFileDialog();

  /**
   * @brief restartTheApp quit the application and relaunch it, passing the
   * --launcher flag explicitly.
   */
  void restartTheApp();

  /**
 @ brief helper to re-open the launcher once FLightGear exits cleanly
 */
  void startLaunchOnExit(const std::vector<std::string>& originalCommandLine);

  void launcherSetSceneryPaths();

    bool showSetupRootDialog(bool usingDefaultRoot);

    enum class SetupRootResult {
        RestoredOk,
        UserExit,
        UserSelected,
        UseDefault
    };

    SetupRootResult restoreUserSelectedRoot(SGPath& path);

  void warnAboutGLVersion();
  } // namespace flightgear
