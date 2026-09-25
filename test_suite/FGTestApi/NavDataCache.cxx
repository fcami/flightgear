// SPDX-FileCopyrightText: 2018 Edward d'Auvergne
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "NavDataCache.hxx"

#include <iostream>

#include <simgear/misc/sg_path.hxx>
#include <simgear/timing/timestamp.hxx>

#include <Main/globals.hxx>
#include <Navaids/NavDataCache.hxx>


namespace FGTestApi {

namespace setUp {

void initNavDataCache()
{
    // Synthetic procedures (test_data/procedures/README.md). Airports cache
    // whether they looked for procedures, and the cache outlives each test, so
    // this must be added on every call, before any airport's procedures are
    // requested. It is added after the cache is created, so the extra
    // scenery path never triggers a cache rebuild.
    auto addTestProcedures = []() {
        globals->append_fg_scenery(SGPath::fromUtf8(FG_TEST_SUITE_DATA) / "procedures");
    };

    if (flightgear::NavDataCache::instance()) {
        addTestProcedures();
        return;
    }

    flightgear::NavDataCache* cache = flightgear::NavDataCache::createInstance();
    if (cache->isRebuildRequired()) {
        std::cerr << "Navcache rebuild for testing" << std::flush;

        while (cache->rebuild() != flightgear::NavDataCache::REBUILD_DONE) {
          SGTimeStamp::sleepForMSec(1000);
          std::cerr << "." << std::flush;
        }
    }

    addTestProcedures();
}

}  // End of namespace setUp.

}  // End of namespace FGTestApi.
