/*
 * SPDX-FileName: fgelev.cxx
 * SPDX-FileComment: compute scenery elevation
 * SPDX-FileCopyrightText: Copyright (C) 2009 - 2025  Mathias Froehlich
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <iomanip>
#include <iostream>

#include <osg/ArgumentParser>
#include <osg/Image>

#include <simgear/bucket/newbucket.hxx>
#include <simgear/bvh/BVHLineSegmentVisitor.hxx>
#include <simgear/bvh/BVHMaterial.hxx>
#include <simgear/bvh/BVHNode.hxx>
#include <simgear/bvh/BVHPageNode.hxx>
#include <simgear/bvh/BVHPager.hxx>
#include <simgear/math/SGGeod.hxx>
#include <simgear/misc/ResourceManager.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/scene/material/matlib.hxx>
#include <simgear/scene/model/BVHPageNodeOSG.hxx>
#include <simgear/scene/model/ModelRegistry.hxx>
#include <simgear/scene/tgdb/ReaderWriterSTG.hxx>
#include <simgear/scene/tgdb/userdata.hxx>
#include <simgear/scene/util/OptionsReadFileCallback.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>
#include <simgear/scene/util/SGSceneFeatures.hxx>

// Static linking of OSG needs special macros to force plugins to be included
USE_OSGPLUGIN(btg)
USE_OSGPLUGIN(stg)

namespace sg = simgear;

class Visitor : public sg::BVHLineSegmentVisitor
{
public:
    Visitor(const SGLineSegmentd& lineSegment, sg::BVHPager& pager) : BVHLineSegmentVisitor(lineSegment, 0),
                                                                      _pager(pager)
    {
    }
    virtual ~Visitor()
    {
    }
    virtual void apply(sg::BVHPageNode& node)
    {
        // we have a non threaded pager so load just right here.
        _pager.use(node);
        BVHLineSegmentVisitor::apply(node);
    }

private:
    sg::BVHPager& _pager;
};

static bool
intersect(sg::BVHNode& node, sg::BVHPager& pager,
          const SGVec3d& start, SGVec3d& end, double offset, const simgear::BVHMaterial** material)
{
    SGVec3d perp = offset * perpendicular(start - end);
    Visitor visitor(SGLineSegmentd(start + perp, end + perp), pager);
    node.accept(visitor);
    if (visitor.empty())
        return false;
    end = visitor.getLineSegment().getEnd();
    *material = visitor.getMaterial();
    return true;
}

int main(int argc, char** argv)
{
    osg::setNotifyLevel(osg::WARN);
    //sglog().setLogLevels(SG_ALL, SG_WARN);
    /// Read arguments and environment variables.

    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);

    if (arguments.read("--help")) {
        std::cout << "Usage: fgelev --tile-lat <lat> --tile-lon <lon> [Options...]\n\nQuery the elevation of lon/lat points in the scenery for a give tile." << std::endl;
        ;
        std::cout << "Input is read from STDIN line by line in the format <id> <lon> <lat>.  Output is to STDOUT in the format <id>: <elevation-m> [solid|-]" << std::endl
                  << std::endl;
        std::cout << "Arguments:" << std::endl;
        std::cout << "\t--tile-lat <lat> --tile-lon <lon>\t\tLatitude and Longitude of the tile to load. Note that only tile can be loaded at a time." << std::endl;
        std::cout << "\t[--expire N]\t\tExpire items in the cache after N requests (default 10). Use to control memory occupancy." << std::endl;
        std::cout << "\t[--print-solidness]\tOutput whether the location is on solid ground (\"solid\") or not (\"-\")." << std::endl;
        std::cout << "\t[--fg-root <path>]\tSet the FG data directory.  Used to retrieve terrain material information. Default to $FG_ROOT." << std::endl;
        std::cout << "\t[--fg-scenery <path>]\tSet the scenery directory.  Defaults to $FG_SCENERY, or $FG_ROOT/Scenery if $FG_SCENERY not set." << std::endl;
        return EXIT_FAILURE;
    }

    unsigned expire;
    if (arguments.read("--expire", expire)) {
    } else
        expire = 10;

    bool printSolidness = arguments.read("--print-solidness");

    std::string fg_root;
    if (arguments.read("--fg-root", fg_root)) {
    } else if (const char* fg_root_env = std::getenv("FG_ROOT")) {
        fg_root = fg_root_env;
    } else {
        fg_root = PKGLIBDIR;
    }

    SGPath fg_scenery;
    std::string s;
    if (arguments.read("--fg-scenery", s)) {
        fg_scenery = SGPath::fromLocal8Bit(s.c_str());
    } else if (std::getenv("FG_SCENERY")) {
        fg_scenery = SGPath::fromEnv("FG_SCENERY");
    } else {
        SGPath path(fg_root);
        path.append("Scenery");
        fg_scenery = path;
    }

    SGSharedPtr<SGPropertyNode> props = new SGPropertyNode;
    try {
        SGPath preferencesFile = fg_root;
        preferencesFile.append("defaults.xml");
        readProperties(preferencesFile, props);
    } catch (...) {
        // In case of an error, at least make summer :)
        props->getNode("sim/startup/season", true)->setStringValue("summer");

        SG_LOG(SG_GENERAL, SG_ALERT, "Problems loading FlightGear preferences.\n"
                                         << "Probably FG_ROOT is not properly set.");
    }

    // Force Virtual Planet Builder
    SGSceneFeatures::instance()->setVPBActive(true);

    /// now set up the simgears required model stuff

    simgear::ResourceManager::instance()->addBasePath(fg_root, simgear::ResourceManager::PRIORITY_DEFAULT);
    // Just reference simgears reader writer stuff so that the globals get
    // pulled in by the linker ...
    simgear::ModelRegistry::instance();

    sgUserDataInit(props.get());
    SGMaterialLibPtr ml = new SGMaterialLib;
    SGPath mpath(fg_root);
    mpath.append("Materials/regions/materials.xml");
    try {
        ml->load(fg_root, mpath.local8BitStr(), props);
    } catch (...) {
        SG_LOG(SG_GENERAL, SG_ALERT, "Problems loading FlightGear materials.\n"
                                         << "Probably FG_ROOT is not properly set.");
    }
    simgear::SGModelLib::init(fg_root, props);

    // Set up the reader/writer options
    osg::ref_ptr<simgear::SGReaderWriterOptions> options;
    if (osgDB::Options* ropt = osgDB::Registry::instance()->getOptions())
        options = new simgear::SGReaderWriterOptions(*ropt);
    else
        options = new simgear::SGReaderWriterOptions;
    osgDB::convertStringPathIntoFilePathList(fg_scenery.local8BitStr(),
                                             options->getDatabasePathList());
    options->setMaterialLib(ml);
    options->setPropertyNode(props);
    options->setPluginStringData("SimGear::FG_ROOT", fg_root);
    // we do not need the builtin boundingvolumes
    options->setPluginStringData("SimGear::BOUNDINGVOLUMES", "OFF");

    // Specifically identify this as FGElev.  This is picked up at a number
    // of points in the code to take into account that we don't have a graphics
    // context.
    options->setPluginStringData("SimGear::FGElev", "true");
    // We only want to load airports from STG files.  No objects nor any WS2.0 terrain that might be
    // on the scenery path.
    options->setPluginStringData("SimGear::FG_ONLY_AIRPORTS", "ON");

    string_list scenerySuffixes = {"Terrain"}; // Just Terrain
    options->setSceneryPathSuffixes(scenerySuffixes);

    // Install as global registry options so paged sub-tile loads inherit them
    osgDB::Registry::instance()->setOptions(options.get());

    props->getNode("sim/rendering/random-objects", true)->setBoolValue(false);
    props->getNode("sim/rendering/random-vegetation", true)->setBoolValue(false);
    props->getNode("sim/rendering/shaders/tessellation", true)->setBoolValue(false);

    int tileLat, tileLon;
    if (arguments.read("--tile-lat", s)) {
        try {
            tileLat = std::stoi(s);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid --tile-lat argument: " << ia.what() << '\n';
            return EXIT_FAILURE;
        }
    } else {
        std::cerr << "--tile-lat required.\n";
        return EXIT_FAILURE;
    }

    if (arguments.read("--tile-lon", s)) {
        try {
            tileLon = std::stoi(s);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid --tile-lon argument: " << ia.what() << '\n';
            return EXIT_FAILURE;
        }
    } else {
        std::cerr << "--tile-lon required.\n";
        return EXIT_FAILURE;
    }

    if ((tileLat < -90) || (tileLat > 90)) {
        std::cerr << "--tile-lat must be between -90 and 90.\n";
        return EXIT_FAILURE;
    }

    if ((tileLon < -180) || (tileLon > 180)) {
        std::cerr << "--tile-lon must be between -180 and 180.\n";
        return EXIT_FAILURE;
    }

    // Here, all arguments are processed
    arguments.reportRemainingOptionsAsUnrecognized();
    arguments.writeErrorMessages(std::cerr);

    // Now work out what VPB and STG files we need to generate.
    string_list fileList;

    const SGGeod minTile = SGGeod::fromDeg(tileLon, tileLat);
    const SGBucket tile = SGBucket(minTile);

    int maxTileLon = tileLon + 1;

    // Special case wrapping for longitude.  We don't need this for latitude
    if (maxTileLon == 181) maxTileLon = -179;

    const SGGeod maxTile = SGGeod::fromDeg(maxTileLon, tileLat + 1);

    // Add all the possible STG files for this tile location.
    std::vector<SGBucket> buckets;
    sgGetBuckets(minTile, maxTile, buckets);

    for (auto t : buckets) {
        fileList.push_back(t.gen_index_str() + ".stg");
    }

    // Main VPB tile is simply the L0 tile, assumed to be in an archive.  This is added
    // after the .stg files so that any airports are loaded first and will constrain
    // the elevation of the scenery mesh (see VPBTechnique::addElevationConstraint)
    auto vpbFilename = "vpb/" + tile.gen_vpb_base() + ".osgb";
    fileList.push_back(vpbFilename);

    // Get the whole world bvh tree
    SGSharedPtr<sg::BVHNode> node = sg::BVHPageNodeOSG::load(fileList, options, true);

    // if no model has been successfully loaded report failure.
    if (!node.valid()) {
        SG_LOG(SG_GENERAL, SG_ALERT, arguments.getApplicationName() << ": No data loaded");
        return EXIT_FAILURE;
    }

    // We assume that the above is a paged database.
    sg::BVHPager pager;

    while (std::cin.good()) {
        // Increment the paging relevant number
        pager.setUseStamp(1 + pager.getUseStamp());
        // and expire everything not accessed for the past 30 requests
        pager.update(expire);

        std::string id;
        std::cin >> id;
        double lon, lat;
        std::cin >> lon >> lat;
        if (std::cin.fail())
            return EXIT_FAILURE;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        SGVec3d start = SGVec3d::fromGeod(SGGeod::fromDegM(lon, lat, 10000));
        SGVec3d end = SGVec3d::fromGeod(SGGeod::fromDegM(lon, lat, -1000));

        const simgear::BVHMaterial* material = NULL;
        // Try to find an intersection
        bool found = intersect(*node, pager, start, end, 0, &material);
        double scale = 1e-5;
        while (!found && scale <= 1) {
            found = intersect(*node, pager, start, end, scale, &material);
            scale *= 2;
        }
        if (1e-5 < scale)
            std::cerr << "Found hole of minimum diameter "
                      << scale << "m at lon = " << lon
                      << "deg lat = " << lat << "deg" << std::endl;

        std::cout << id << ": ";
        if (!found) {
            std::cout << "-1000" << std::endl;
        } else {
            SGGeod geod = SGGeod::fromCart(end);
            std::cout << std::fixed << std::setprecision(3) << geod.getElevationM();
            if (printSolidness)
                std::cout << " " << (material && material->get_solid() ? "solid" : "-");
            std::cout << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
