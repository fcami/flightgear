/*
 * SPDX-FileName:tileentry.cxx -- routines to handle a scenery tile
 * SPDX-FileCopyrightText: 1998 Curtis L. Olson  - http://www.flightgear.org/~curt
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <simgear/compiler.h>

#include <string>
#include <sstream>
#include <istream>

#include <osg/LOD>

#include <simgear/bucket/newbucket.hxx>
#include <simgear/debug/logstream.hxx>
#include <simgear/scene/tgdb/SGOceanTile.hxx>
#include <simgear/scene/tsync/terrasync.hxx>

#include "tileentry.hxx"

using std::string;

// Base constructor
TileEntry::TileEntry ( const SGBucket& b )
    : tile_bucket( b ),
      _node( new osg::LOD ),
      _priority(-FLT_MAX),
      _current_view(false),
      _time_expired(-1.0)
{
    _create_orthophoto();

    // Give a default LOD range so that traversals that traverse
    // active children (like the groundcache lookup) will work before
    // tile manager has had a chance to update this node.
    _node->setRange(0, 0.0, 10000.0);
}

TileEntry::TileEntry( const TileEntry& t )
: tile_bucket( t.tile_bucket ),
  tileFileName(t.tileFileName),
  _node( new osg::LOD ),
  _priority(t._priority),
  _current_view(t._current_view),
  _time_expired(t._time_expired)
{
    _create_orthophoto();

    _node->setName(tileFileName);
    // Give a default LOD range so that traversals that traverse
    // active children (like the groundcache lookup) will work before
    // tile manager has had a chance to update this node.
    _node->setRange(0, 0.0, 10000.0);
}

void TileEntry::_create_orthophoto() {
    bool use_photoscenery = fgGetBool("/sim/rendering/photoscenery/enabled");
    if (use_photoscenery) {
        _orthophoto = simgear::Orthophoto::fromBucket(tile_bucket, globals->get_fg_scenery());
        if (_orthophoto) {
            simgear::OrthophotoManager::instance()->registerOrthophoto(tile_bucket.gen_index(), _orthophoto);
        }
    }
}

// Destructor
TileEntry::~TileEntry() = default;

// Update the ssg transform node for this tile so it can be
// properly drawn relative to our (0,0,0) point
void TileEntry::prep_ssg_node(float vis) {
    if (!is_loaded())
        return;
    // visibility can change from frame to frame so we update the
    // range selector cutoff's each time.
    float bounding_radius = _node->getChild(0)->getBound().radius();
    _node->setRange( 0, 0, vis + bounding_radius );
}

void
TileEntry::addToSceneGraph(osg::Group *terrain_branch)
{
    terrain_branch->addChild( _node.get() );

    SG_LOG( SG_TERRAIN, SG_DEBUG,
            "connected a tile into scene graph.  _node = "
            << _node.get() );
    SG_LOG( SG_TERRAIN, SG_DEBUG, "num parents now = "
            << _node->getNumParents() );
}


void
TileEntry::removeFromSceneGraph()
{
    if (! is_loaded()) {
        SG_LOG( SG_TERRAIN, SG_DEBUG, "removing a not-fully loaded tile!" );
    } else {
        SG_LOG( SG_TERRAIN, SG_DEBUG, "removing a fully loaded tile!  _node = " << _node.get() );
    }

    if (_node->getNumParents() > 0) {
        // find the first parent (should only be one)
        osg::Group *parent = _node->getParent( 0 ) ;
        if( parent ) {

            parent->removeChild( _node.get() );
        }
    }
}

// Constructor - STG Variant
STGTileEntry::STGTileEntry ( const SGBucket& b ) : TileEntry(b)
{
    tileFileName = b.gen_index_str() + ".stg";
    _node->setName(tileFileName);
}

// Destructor - STG Variant
STGTileEntry::~STGTileEntry ()
{
}

// Constructor - VPB version
VPBTileEntry::VPBTileEntry ( const SGBucket& b, osg::ref_ptr<simgear::SGReaderWriterOptions> options ) : TileEntry(b)
{
    tileFileName = "vpb/" + b.gen_vpb_base() + ".osgb";

    if (checkForVPBData(options)) {
        _tileState = TileState::VPBData;
        useVPBData();
    } else {
        auto terraSync = globals->get_subsystem<simgear::SGTerraSync>();
        if (terraSync && terraSync->isEnabled()) {
            _tileState = TileState::AwaitingSync;
        } else {
            _tileState = TileState::OceanData;
        }
        addOceanPlaceholder(options);
    }
}

// Destructor - VPB Variant
VPBTileEntry::~VPBTileEntry() = default;

bool VPBTileEntry::checkForVPBData(osg::ref_ptr<simgear::SGReaderWriterOptions> options) const
{
    // The tile can either be a simple file, or contained within a 1x1 degree zipfile.
    const auto zipFileName = "vpb/" + tile_bucket.gen_base_path() + ".zip";

    auto filePathList = options->getDatabasePathList();
    for (auto path : filePathList) {
        SGPath p(path, tileFileName);
        if (p.exists()) {
            return true;
        }

        SGPath archive(path, zipFileName);
        if (archive.exists()) {
            return true;
        }
    }
    return false;
}

void VPBTileEntry::useVPBData()
{
    // File exists - set it up for loading later, removing any existing Ocean placeholder
    _node->removeChildren(0, _node->getNumChildren());
    _node->setName(tileFileName);

    // Give a default LOD range so that traversals that traverse
    // active children (like the groundcache lookup) will work before
    // tile manager has had a chance to update this node.
    // This is the maximum 1x1 degree tile radius in km.
    _node->setRange(0, 0.0, 157000.0);
}

void VPBTileEntry::addOceanPlaceholder(osg::ref_ptr<simgear::SGReaderWriterOptions> options)
{
    // Real data doesn't exist (yet) - add a 1x1 degree Ocean tile so there's
    // something to render while TerraSync catches up (or forever, if this
    // genuinely is open ocean - see maybeRefreshPlaceholder()).
    const double lat = floor(tile_bucket.get_center_lat()) + 0.5;
    const double lon = floor(tile_bucket.get_center_lon()) + 0.5;
    SG_LOG(SG_TERRAIN, SG_DEBUG, "Generating Ocean Tile for " << lat << ", " << lon);

    // Standard for WS2.0 is 5 points per bucket (~30km), or 8km spacing.
    // 1 degree latitude and 1 degree of longitude at the equator is 111km, 15 points
    // are equivalent resolution.
    osg::Node* oceanTile = SGOceanTile(lat, lon, 1.0, 1.0, options->getMaterialLib(), 15, 15);
    _node->addChild(oceanTile, 0, 250000.0);
}

void VPBTileEntry::maybeRefreshPlaceholder(osg::ref_ptr<simgear::SGReaderWriterOptions> options)
{
    if (_tileState == TileState::VPBData || _tileState == TileState::OceanData) {
        return;
    }

    auto terraSync = globals->get_subsystem<simgear::SGTerraSync>();
    const bool pending = terraSync && terraSync->isTileDirPending(tile_bucket.gen_base_path());

    if (pending) {
        // Still syncing (or syncing again) - nothing to check yet.
        _tileState = TileState::Syncing;
        return;
    }

    if (_tileState != TileState::Syncing) {
        // Never observed TerraSync actively syncing this tile, so nothing
        // has changed since we last looked - don't bother re-checking disk.
        return;
    }

    // TerraSync was syncing this tile and has now finished, one way or
    // another - our one chance to notice if real data has arrived.
    if (checkForVPBData(options)) {
        _tileState = TileState::VPBData;
        useVPBData();
    } else {
        _tileState = TileState::OceanData;
    }
}
