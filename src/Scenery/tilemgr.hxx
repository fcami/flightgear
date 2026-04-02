// tilemgr.hxx -- routines to handle dynamic management of scenery tiles
//
// Written by Curtis Olson, started January 1998.
//
// SPDX-FileCopyrightText: 1998 Curtis L. Olson
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/compiler.h>

#include <simgear/bucket/newbucket.hxx>
#include "SceneryPager.hxx"
#include "tilecache.hxx"

namespace osg
{
class Node;
}

namespace simgear
{
class SGReaderWriterOptions;
}

class FGTileMgr {

private:

    // Tile loading state
    enum load_state {
        Start = 0,
        Inited = 1,
        Running = 2
    };

    load_state state, last_state;

    // schedule a tile for loading, returns true when tile is already loaded
    bool sched_tile( const SGBucket& b, double priority,bool current_view, double request_time);

    // schedule a needed buckets for loading
    void schedule_needed(const SGBucket& curr_bucket, double rangeM);

    bool isTileDirSyncing(const std::string& tileFileName) const;

    SGBucket previous_bucket;
    SGBucket current_bucket;
    SGBucket pending;
    osg::ref_ptr<simgear::SGReaderWriterOptions> _options;

    double scheduled_visibility;

    /**
     * tile cache
     */
    TileCache tile_cache;

    class TileManagerListener;
    friend class TileManagerListener;
    std::unique_ptr<TileManagerListener> _listener;

    // update various queues internal queues
    void update_queues(bool& isDownloadingScenery);

    // schedule tiles for the viewer bucket
    void schedule_tiles_at(const SGGeod& location, double rangeM);

    SGPropertyNode_ptr _visibilityMeters;
    SGPropertyNode_ptr _lodDetailed, _lodRoughDelta, _lodBareDelta, _disableNasalHooks;
    SGPropertyNode_ptr _scenery_loaded, _scenery_override;

    // Statistics from the database pager.
    SGPropertyNode_ptr _pager_file_queue_size, _pager_compile_queue_size, _pager_merge_queue_size;
    SGPropertyNode_ptr _pager_min_merge_time, _pager_mean_merge_time, _pager_max_merge_time;
    SGPropertyNode_ptr _pager_active_lod_count;

    osg::ref_ptr<flightgear::SceneryPager> _pager;

    /// is caching of expired tiles enabled or not?
    bool _enableCache = true;

public:
    FGTileMgr();
    ~FGTileMgr();

    // Initialize the Tile Manager
    void init();
    void reinit();
    void shutdown();
    void update(double dt);

    const SGBucket& get_current_bucket () const { return current_bucket; }

    // Returns true if scenery is available for the given lat, lon position
    // within a range of range_m.
    // lat and lon are expected to be in degrees.
    bool schedule_scenery(const SGGeod& position, double range_m, double duration=0.0);

    // Returns true if tiles around current view position have been loaded
    bool isSceneryLoaded();

    // notify the tile manahger the material library was reloaded,
    // so it can pass this through to its options object
    void materialLibChanged();
};
