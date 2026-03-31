// SPDX-FileCopyrightText: 2000 Cdr. VS Renganthan <vsranga@ada.ernet.in>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * @brief Interface to the "External"-ly driven ADA flight model
 */

#pragma once

class SGSocket;

#include <FDM/flight.hxx>


class FGADA : public FGInterface
{
private:
    SGSocket *fdmsock;
#if 0
    // Auxiliary Flight Model parameters, basically for HUD
    double        aux1;           // auxiliary flag
    double        aux2;           // auxiliary flag
    double        aux3;           // auxiliary flag
    double        aux4;           // auxiliary flag
    double        aux5;           // auxiliary flag
    double        aux6;           // auxiliary flag
    double        aux7;           // auxiliary flag
    double        aux8;           // auxiliary flag
    float        aux9;            // auxiliary flag
    float        aux10;           // auxiliary flag
    float        aux11;           // auxiliary flag
    float        aux12;           // auxiliary flag
    float        aux13;           // auxiliary flag
    float        aux14;           // auxiliary flag
    float        aux15;           // auxiliary flag
    float        aux16;           // auxiliary flag
    float        aux17;           // auxiliary flag
    float        aux18;           // auxiliary flag
    int          iaux1;           // auxiliary flag
    int          iaux2;           // auxiliary flag
    int          iaux3;           // auxiliary flag
    int          iaux4;           // auxiliary flag
    int          iaux5;           // auxiliary flag
    int          iaux6;           // auxiliary flag
    int          iaux7;           // auxiliary flag
    int          iaux8;           // auxiliary flag
    int          iaux9;           // auxiliary flag
    int         iaux10;           // auxiliary flag
    int         iaux11;           // auxiliary flag
    int         iaux12;           // auxiliary flag
#endif
    // copy FDM state to FGADA structures
    bool copy_to_FGADA();

    // copy FDM state from FGADA structures
    bool copy_from_FGADA();

public:
    FGADA( double dt );
    ~FGADA();

    // Subsystem API.
    void init() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "ada"; }
};
