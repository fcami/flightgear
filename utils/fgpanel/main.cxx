// SPDX-FileCopyrightText: 2011 Torsten Dreyer
// SPDX-License-Identifier: GPL-2.0-or-later

#include "FGPanelApplication.hxx"
using std::endl;

int
main (int argc, char ** argv) {
  try {
    FGPanelApplication app (argc,argv);
    app.Run ();
    return 0;
  }
  catch (...) {
      std::cerr << "Sorry, your program terminated." << endl;
  }
}
