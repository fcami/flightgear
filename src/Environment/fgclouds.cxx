// Build a cloud layer based on metar
//
// Written by Harald JOHNSEN, started April 2005.
//
// SPDX-FileCopyrightText: 2005 Harald JOHNSEN <hjohnsen@evc.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "fgclouds.hxx"

#include <Main/fg_props.hxx>
#include <cstdio>
#include <cstring>

#include <osg/Image>

#include <simgear/constants.h>
#include <simgear/math/sg_geodesy.hxx>
#include <simgear/props/props_io.hxx>
#include <simgear/scene/sky/cloudfield.hxx>
#include <simgear/scene/sky/newcloud.hxx>
#include <simgear/scene/sky/sky.hxx>
#include <simgear/scene/util/StateAttributeFactory.hxx>
#include <simgear/sound/soundmgr.hxx>
#include <simgear/structure/commands.hxx>

#include <Airports/airport.hxx>
#include <Environment/environment_ctrl.hxx>
#include <Main/globals.hxx>
#include <Main/util.hxx>
#include <Time/light.hxx>
#include <Viewer/renderer.hxx>
#include <Viewer/view.hxx>
#include <fast_marching_method.hpp>

namespace fmm = thinks::fast_marching_method;

// RNG seed to ensure cloud synchronization across multi-process
// deployments
static mt seed;

// Exception count from the fmm library
static unsigned int _FMMExceptionCount = 0;

FGClouds::FGClouds() : index(0)
{
    update_event = 0;
    _options = new simgear::SGReaderWriterOptions;
    _options->setObjectCacheHint(osgDB::Options::CACHE_ALL);

    _cloudUpdateNode = new osg::Group;
    _cloudUpdateNode->setName("Cloud Update Node");
    _cloudUpdateNode->addUpdateCallback(new FGCloudUpdateCallback(this));

    auto p = globals->get_props()->getNode("/sim/rendering/hdr/clouds/");

    _cloudBaseM = SGPropObjDouble(p, "cloud-base-m");
    _cloudBaseM.setDefault(0.0);

    _cloudBaseZNorm = SGPropObjDouble(p, "cloud-base-z-norm");
    _cloudBaseZNorm.setDefault(0.0);

    _cloudCenterX = SGPropObjDouble(p, "cloud-center-x");
    _cloudCenterX.setDefault(0.0);

    _cloudCenterY = SGPropObjDouble(p, "cloud-center-y");
    _cloudCenterY.setDefault(0.0);

    _cloudCenterZ = SGPropObjDouble(p, "cloud-center-z");
    _cloudCenterZ.setDefault(0.0);

    _mirrorU = SGPropObjBool(p, "mirror-u");
    _mirrorU.setDefault(false);

    _mirrorV = SGPropObjBool(p, "mirror-v");
    _mirrorV.setDefault(false);

    _cloudFieldRepeating = SGPropObjBool(p, "cloud-field-repeating");
    _cloudFieldRepeating.setDefault(false);

    _activeVoxelFieldHeightNorm = SGPropObjDouble(p, "active-voxel-field-height-norm");
    _activeVoxelFieldHeightNorm.setDefault(0.0);

    _shadeUpdateAngleDeg = SGPropObjDouble(p, "shade-update-angle-deg");
    _shadeUpdateAngleDeg.setDefault(0.0);
}

FGClouds::~FGClouds()
{
    // Wait for any in-flight background rebuild to finish before
    // destroying the placement map (the task holds raw pointers into it).
    if (_rebuildFuture.valid())
        _rebuildFuture.wait();

    if (_shadeRebuildFuture.valid())
        _shadeRebuildFuture.wait();

    globals->get_commands()->removeCommand("add-cloud");
    globals->get_commands()->removeCommand("del-cloud");
    globals->get_commands()->removeCommand("move-cloud");
}

int FGClouds::get_update_event(void) const
{
    return update_event;
}

void FGClouds::set_update_event(int count)
{
    update_event = count;
    buildCloudLayers();
}

void FGClouds::Init(void)
{
    mt_init_time_10(&seed);
    _FMMExceptionCount = 0;

    globals->get_commands()->addCommand("add-cloud", this, &FGClouds::add3DCloud);
    globals->get_commands()->addCommand("del-cloud", this, &FGClouds::delete3DCloud);
    globals->get_commands()->addCommand("move-cloud", this, &FGClouds::move3DCloud);

    _fieldDirty = true;
    _fieldRepeating = true;
}

// Build an individual cloud. Returns the extents of the cloud for coverage calculations
double FGClouds::buildCloud(SGPropertyNode* cloud_def_root, SGPropertyNode* box_def_root,
                            const std::string& name, double altFt, double grid_z_rand, SGCloudField* layer)
{
    SGPropertyNode* box_def = NULL;
    SGPropertyNode* cld_def = NULL;
    float extent = 0.0;

    SGPath texture_root = globals->get_fg_root();
    texture_root.append("Textures");
    texture_root.append("Sky");

    box_def = box_def_root->getChild(name.c_str());

    string base_name = name.substr(0, 2);
    if (!box_def) {
        if (name[2] == '-') {
            box_def = box_def_root->getChild(base_name.c_str());
        }
        if (!box_def)
            return 0.0;
    }

    float detailedFieldWidthM = (float)_detailedFieldWidth * (float)_detailedFieldVoxelSize;

    // Note that these are all in metres
    float x = mt_rand(&seed) * detailedFieldWidthM - (detailedFieldWidthM / 2.0);
    float y = mt_rand(&seed) * detailedFieldWidthM - (detailedFieldWidthM / 2.0);
    float z = (float)altFt * SG_FEET_TO_METER + (float)grid_z_rand * (mt_rand(&seed) - 0.5);
    SGVec3f pos(x, y, z);

    float lon = fgGetNode("/position/longitude-deg", false)->getFloatValue();
    float lat = fgGetNode("/position/latitude-deg", false)->getFloatValue();

    for (int i = 0; i < box_def->nChildren(); i++) {
        SGPropertyNode* abox = box_def->getChild(i);
        if (abox->getNameString() == "box") {
            string type = abox->getStringValue("type", "cu-small");
            cld_def = cloud_def_root->getChild(type.c_str());
            if (!cld_def) return 0.0;

            float w = abox->getFloatValue("width", 1000.0);
            float h = abox->getFloatValue("height", 1000.0);
            int hdist = abox->getIntValue("hdist", 1);
            int vdist = abox->getIntValue("vdist", 1);

            float c = abox->getFloatValue("count", 5);
            int count = (int)(c + (mt_rand(&seed) - 0.5) * c);

            extent = std::max(w * w, extent);

            for (int j = 0; j < count; j++) {
                // Locate the clouds randomly in the defined space. The hdist and
                // vdist values control the horizontal and vertical distribution
                // by simply summing random components.
                float x = 0.0;
                float y = 0.0;
                float z = 0.0;

                for (int k = 0; k < hdist; k++) {
                    x += (mt_rand(&seed) / hdist);
                    y += (mt_rand(&seed) / hdist);
                }

                for (int k = 0; k < vdist; k++) {
                    z += (mt_rand(&seed) / vdist);
                }

                x = w * (x - 0.5) + pos[0]; // N/S
                y = w * (y - 0.5) + pos[1]; // E/W
                z = h * z + pos[2];         // Up/Down. pos[2] is the cloudbase

                addCloud(std::make_unique<SGVoxelTextureCloud>(type, cld_def, &seed, _options),
                         index++, lon, lat, z * SG_METER_TO_FEET, x, y);
            }
        }
    }

    // Return the maximum extent of the cloud
    return extent;
}

void FGClouds::buildLayer(int iLayer, const string& name, double coverage, double altFt)
{
    struct {
        string name;
        double count;
    } tCloudVariety[20];
    int CloudVarietyCount = 0;
    double totalCount = 0.0;

    SGSky* thesky = globals->get_renderer()->getSky();

    float lon = globals->get_aircraft_position().getLongitudeDeg();
    float lat = globals->get_aircraft_position().getLatitudeDeg();

    SGPropertyNode* cloud_def_root = fgGetNode("/environment/cloudlayers/clouds", false);
    SGPropertyNode* box_def_root = fgGetNode("/environment/cloudlayers/boxes", false);
    SGPropertyNode* layer_def_root = fgGetNode("/environment/cloudlayers/layers", false);
    SGCloudField* layer = thesky->get_cloud_layer(iLayer)->get_layer3D();
    layer->clear();

    // If we don't have the required properties, then render the cloud in 2D
    if (coverage == 0.0 || layer_def_root == NULL || cloud_def_root == NULL || box_def_root == NULL) {
        thesky->get_cloud_layer(iLayer)->set_enable3dClouds(false);
        return;
    }

    // If we can't find a definition for this cloud type, then render the cloud in 2D
    SGPropertyNode* layer_def = NULL;
    layer_def = layer_def_root->getChild(name.c_str());
    if (!layer_def) {
        if (name[2] == '-') {
            string base_name = name.substr(0, 2);
            layer_def = layer_def_root->getChild(base_name.c_str());
        }
        if (!layer_def) {
            thesky->get_cloud_layer(iLayer)->set_enable3dClouds(false);
            return;
        }
    }

    // At this point, we know we've got some 3D clouds to generate.
    thesky->get_cloud_layer(iLayer)->set_enable3dClouds(true);

    double grid_z_rand = layer_def->getDoubleValue("grid-z-rand");

    for (int i = 0; i < layer_def->nChildren(); i++) {
        SGPropertyNode* acloud = layer_def->getChild(i);
        if (acloud->getNameString() == "cloud") {
            string cloud_name = acloud->getStringValue("name");
            tCloudVariety[CloudVarietyCount].name = cloud_name;
            double count = acloud->getDoubleValue("count", 1.0);
            tCloudVariety[CloudVarietyCount].count = count;

            totalCount += count;
            if (CloudVarietyCount < 20)
                CloudVarietyCount++;
        }

        if (acloud->getNameString() == "layer") {
            // Layer clouds are a special case - they don't come in boxes and we can
            // generate them directly, passing in the coverage parameter.
            // We don't add a random Z offset as the typical use case is representing
            // a layer of cloud with a defined cloudbase
            string cloud_name = acloud->getStringValue("name");
            if (cloud_def_root->hasChild(cloud_name.c_str())) {
                SGPropertyNode* cld_def = cloud_def_root->getChild(cloud_name.c_str());
                float z = (float)altFt * SG_FEET_TO_METER;

                addCloud(std::make_unique<SGVoxelLayerCloud>(cloud_name, cld_def, &seed, coverage, thesky->get_cloud_layer(iLayer)->getThickness_m()),
                         index++, lon, lat, z * SG_METER_TO_FEET, 0, 0);
            } else {
                SG_LOG(SG_ENVIRONMENT, SG_DEV_ALERT, "Unable to find cloud definition for layer type " << cloud_name);
            }
        }
    }
    totalCount = 1.0 / totalCount;

    if (CloudVarietyCount > 0) {
        // Determine how much cloud coverage we need in m^2.
        float fieldWidthM = (float)_detailedFieldWidth * (float)_detailedFieldVoxelSize;
        double cov = coverage * fieldWidthM * fieldWidthM;

        while (cov > 0.0f) {
            double choice = mt_rand(&seed);

            for (int i = 0; i < CloudVarietyCount; i++) {
                choice -= tCloudVariety[i].count * totalCount;
                if (choice <= 0.0) {
                    cov -= buildCloud(cloud_def_root,
                                      box_def_root,
                                      tCloudVariety[i].name,
                                      altFt,
                                      grid_z_rand,
                                      layer);
                    break;
                }
            }
        }
    }
}

void FGClouds::buildCloudLayers(void)
{
    // Cancel any in-flight rebuild before clearing the map it holds pointers into
    if (_rebuildFuture.valid())
        _rebuildFuture.wait();

    // Clear out the existing clouds;  Destructor handles cleanup
    {
        std::lock_guard<std::mutex> lk(_placementMutex);
        _cloudPlacementMap.clear();
    }

    // Enable repeating of the voxel space as the clouds we generate should be global
    setCloudsRepeating(true);

    SGPropertyNode* metar_root = fgGetNode("/environment", true);

    //double wind_speed_kt	 = metar_root->getDoubleValue("wind-speed-kt");
    double temperature_degc = metar_root->getDoubleValue("temperature-sea-level-degc");
    double dewpoint_degc = metar_root->getDoubleValue("dewpoint-sea-level-degc");
    double pressure_mb = metar_root->getDoubleValue("pressure-sea-level-inhg") * SG_INHG_TO_PA / 100.0;
    double rel_humidity = metar_root->getDoubleValue("relative-humidity");

    // formula d'Epsy, base d'un cumulus
    double cumulus_base = 122.0 * (temperature_degc - dewpoint_degc);
    double stratus_base = 100.0 * (100.0 - rel_humidity) * SG_FEET_TO_METER;

    SGSky* thesky = globals->get_renderer()->getSky();
    for (int iLayer = 0; iLayer < thesky->get_cloud_layer_count(); iLayer++) {
        SGPropertyNode* cloud_root = fgGetNode("/environment/clouds/layer", iLayer, true);

        double alt_ft = cloud_root->getDoubleValue("elevation-ft");
        double alt_m = alt_ft * SG_FEET_TO_METER;
        string coverage = cloud_root->getStringValue("coverage");

        double coverage_norm = 0.0;
        if (coverage == "few")
            coverage_norm = 2.0 / 8.0; // <1-2
        else if (coverage == "scattered")
            coverage_norm = 4.0 / 8.0; // 3-4
        else if (coverage == "broken")
            coverage_norm = 6.0 / 8.0; // 5-7
        else if (coverage == "overcast")
            coverage_norm = 8.0 / 8.0; // 8

        string layer_type = "nn";

        if (coverage == "cirrus") {
            layer_type = "ci";
        } else if (alt_ft > 16500) {
            //			layer_type = "ci|cs|cc";
            layer_type = "ci";
        } else if (alt_ft > 6500) {
            //			layer_type = "as|ac|ns";
            layer_type = "ac";
            if (pressure_mb < 1005.0 && coverage_norm >= 0.5)
                layer_type = "ns";
        } else {
            //			layer_type = "st|cu|cb|sc";
            if (cumulus_base * 0.80 < alt_m && cumulus_base * 1.20 > alt_m) {
                // +/- 20% from cumulus probable base
                layer_type = "cu";
            } else if (stratus_base * 0.80 < alt_m && stratus_base * 1.40 > alt_m) {
                // +/- 20% from stratus probable base
                layer_type = "st";
            } else {
                // above formulae is far from perfect
                if (alt_ft < 2000)
                    layer_type = "st";
                else if (alt_ft < 4500)
                    layer_type = "cu";
                else
                    layer_type = "sc";
            }
        }

        cloud_root->setStringValue("layer-type", layer_type);
        buildLayer(iLayer, layer_type, coverage_norm, alt_ft);
    }
}

/**
 * Adds a 3D cloud to a cloud layer.
 *
 * Property arguments
 * layer - the layer index to add this cloud to. (Defaults to 0)
 * index - the index for this cloud (to be used later)
 * lon/lat/alt - the position for the cloud
 * (Various) - cloud definition properties. See README.3DClouds
 *
 */
bool FGClouds::add3DCloud(const SGPropertyNode* arg, SGPropertyNode* root)
{
    string name = arg->getStringValue("name", "undefined");
    int index = arg->getIntValue("index", 0);
    float lon = arg->getFloatValue("lon-deg", 0.0f);
    float lat = arg->getFloatValue("lat-deg", 0.0f);
    float alt = arg->getFloatValue("alt-ft", 0.0f);
    float x = arg->getFloatValue("x-offset-m", 0.0f);
    float y = arg->getFloatValue("y-offset-m", 0.0f);

    bool success = addCloud(std::unique_ptr<SGVoxelCloud>(SGVoxelCloud::buildCloud(name, arg, &seed, _options)),
                            index, lon, lat, alt, x, y);
    return success;
}

/**
  * Removes a 3D cloud from a cloud layer
  *
  * Property arguments
  *
  * layer - the layer index to remove this cloud from. (defaults to 0)
  * index - the cloud index
  *
  */
bool FGClouds::delete3DCloud(const SGPropertyNode* arg, SGPropertyNode* root)
{
    int i = arg->getIntValue("index", 0);
    return removeCloud(i);
}

/**
 * Move a cloud within a 3D layer
 *
 * Property arguments
 * layer - the layer index to add this cloud to. (Defaults to 0)
 * index - the cloud index to move.
 * lon/lat/alt - the position for the cloud
 *
 */
bool FGClouds::move3DCloud(const SGPropertyNode* arg, SGPropertyNode* root)
{
    int i = arg->getIntValue("index", 0);
    float lon = arg->getFloatValue("lon-deg", 0.0f);
    float lat = arg->getFloatValue("lat-deg", 0.0f);
    float alt = arg->getFloatValue("alt-ft", 0.0f);
    float x = arg->getFloatValue("x-offset-m", 0.0f);
    float y = arg->getFloatValue("y-offset-m", 0.0f);
    return repositionCloud(i, lon, lat, alt, x, y);
}

bool FGClouds::addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, float lon, float lat, float altFt)
{
    return addCloud(std::move(cloud), index, lon, lat, altFt, 0.0f, 0.0f);
}

bool FGClouds::addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, float lon, float lat, float altFt, float x, float y)
{
    SGGeod loc = SGGeod::fromDegFt(lon, lat, altFt);
    return addCloud(std::move(cloud), index, loc, x, y);
}

bool FGClouds::removeCloud(int index)
{
    std::lock_guard<std::mutex> lk(_placementMutex);

    if (_cloudPlacementMap.contains(index)) {
        _cloudPlacementMap.erase(index); // Destructor deletes the cloud
        _fieldDirty = true;
        return true;
    } else {
        return false;
    }
}

osg::Vec3f FGClouds::getFinalPos(SGGeod loc, float x, float y)
{
    const float alt = loc.getElevationFt();
    // Determine any shift by x/y
    if ((x != 0.0f) || (y != 0.0f)) {
        double crs = 90.0 - SG_RADIANS_TO_DEGREES * atan2(y, x);
        double dst = dist(SGVec2f(x, y), SGVec2f(0.0, 0.0));
        double endcrs;

        SGGeod base_pos = SGGeod::fromGeodFt(loc, 0.0f);
        SGGeodesy::direct(base_pos, crs, dst, loc, endcrs);
    }

    // The direct call provides the position at 0 alt, so adjust as required.
    loc.setElevationFt(alt);

    // Work out where this cloud should go in OSG coordinates.
    SGVec3<double> cart;
    SGGeodesy::SGGeodToCart(loc, cart);
    return toOsg(cart);
}

bool FGClouds::addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, SGGeod loc, float x, float y)
{
    std::lock_guard<std::mutex> lk(_placementMutex);

    // If this cloud index already exists, don't replace it.
    if (_cloudPlacementMap.contains(index)) return false;
    osg::Vec3f pos = getFinalPos(loc, x, y);

    _cloudPlacementMap.emplace(index, std::make_pair(std::move(cloud), pos));
    _fieldDirty = true;

    return true;
}

bool FGClouds::repositionCloud(int index, float lon, float lat, float alt)
{
    return repositionCloud(index, lon, lat, alt, 0.0f, 0.0f);
}

bool FGClouds::repositionCloud(int index, float lon, float lat, float alt, float x, float y)
{
    std::lock_guard<std::mutex> lk(_placementMutex);

    auto it = _cloudPlacementMap.find(index);
    if (it == _cloudPlacementMap.end()) return false;

    SGGeod loc = SGGeod::fromDegFt(lon, lat, alt);
    osg::Vec3f pos = getFinalPos(loc, x, y);

    it->second.second = pos; // update position, cloud object untouched
    _fieldDirty = true;
    return true;
}

// Sun direction in voxel (Z-up, field-relative) space. Must be called on
// the main thread (reads FGLight subsystem state and _cloudPosMatrix).
osg::Vec3f FGClouds::computeSunDirVoxel() const
{
    assert(SGThreads::isMainThread());
    auto l = globals->get_subsystem<FGLight>();

    // sun_vec() points from scene toward sun in world (ECEF) space
    SGVec4f sunWorld = l->sun_vec();
    osg::Vec3f sunOsg(sunWorld.x(), sunWorld.y(), sunWorld.z());

    // Rotate into local Z-up frame: multiply by the inverse (transpose for orthonormal) of _cloudPosMatrix
    // In OSG row-vector convention: v_local = v_world * M^-1
    const osg::Matrixf worldToLocal = osg::Matrix::inverse(_cloudPosMatrix);
    osg::Vec3f sunLocal = sunOsg * worldToLocal;
    sunLocal.normalize();
    return sunLocal;
}

FGClouds::RebuildSnapshot FGClouds::captureSnapshot()
{
    RebuildSnapshot snap;

    assert(SGThreads::isMainThread());

    // Read config (always main-thread safe)
    auto cloudsProp = globals->get_props()->getNode("/sim/rendering/hdr/clouds/");
    snap.detailedFieldWidth = cloudsProp->getIntValue("detailed-voxel-field-width", 256);
    snap.detailedFieldHeight = cloudsProp->getIntValue("detailed-voxel-field-height", 64);
    snap.detailedFieldVoxelSize = cloudsProp->getIntValue("detailed-voxel-size-m", 200);
    snap.voxelOpticalDepth = cloudsProp->getFloatValue("voxel-optical-depth", 2.5);
    snap.fieldRepeating = _fieldRepeating;
    snap.cloudbaseM = 99999.0f;

    // Write back for use by FGClouds outside of the snapshot.
    _detailedFieldWidth = snap.detailedFieldWidth;
    _detailedFieldHeight = snap.detailedFieldHeight;
    _detailedFieldVoxelSize = snap.detailedFieldVoxelSize;

    if (_fieldRepeating) {
        snap.roughVoxelSizeFactor = 1;
        snap.roughFieldVoxelSize = 1;
        snap.roughFieldWidth = 1;
        snap.roughFieldHeight = 1;
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Repeating detailed field being used - Rough Voxel field not in use.");
    } else {
        // This is the size factor for the rough field.  Note that as this is in each dimension
        // the occupany is 1/8th
        snap.roughVoxelSizeFactor = cloudsProp->getIntValue("rough-voxel-size-factor", 2);
        snap.roughFieldVoxelSize = snap.detailedFieldVoxelSize * snap.roughVoxelSizeFactor;
        SGVoxelTextureCloud::setRoughVoxelScale(snap.roughVoxelSizeFactor);

        // The rough field width is a factor of the detailed field width
        snap.roughFieldWidth = snap.detailedFieldWidth * cloudsProp->getIntValue("rough-voxel-field-factor", 2);
        snap.roughFieldHeight = snap.detailedFieldHeight / snap.roughVoxelSizeFactor;
    }

    // Save off the current location, which will be used in transforms.
    // We will determine the altitude later, so make sure it's 0 for
    // the various conversions between ECF and local coordinates.
    SGGeod geod = globals->get_view_position();
    geod.setElevationM(0);

    SGGeodesy::SGGeodToCart(geod, _centerCart);
    _cloudPosMatrix = makeZUpFrameRelative(geod);
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Rebuilding field at " << geod.getLatitudeDeg() << " " << geod.getLongitudeDeg() << " " << geod.getElevationFt());

    _cloudCenterX = _centerCart.x();
    _cloudCenterY = _centerCart.y();
    _cloudCenterZ = _centerCart.z();
    _cloudFieldRepeating = _fieldRepeating;

    snap.centerCart = _centerCart;
    snap.cloudPosMatrix = _cloudPosMatrix;

    // Take the placement map snapshot under lock - this is the only part
    // that races with addCloud/removeCloud.
    const osg::Vec3f centerOsg = toOsg(_centerCart);
    {
        const float detailedFieldRadiusM = 0.5f * (float)snap.detailedFieldWidth * (float)snap.detailedFieldVoxelSize;
        const float roughFieldRadiusM = 0.5f * (float)snap.roughFieldWidth * (float)snap.roughFieldVoxelSize;

        std::lock_guard<std::mutex> lk(_placementMutex);
        for (const auto& [key, value] : _cloudPlacementMap) {
            const SGVoxelCloud* c = std::get<0>(value).get();
            osg::Vec3f q = std::get<1>(value) - centerOsg;
            osg::Vec3f p = _cloudPosMatrix * q;

            // Check if any part is within the X/Y bounds for each of the voxelMaps.
            if (p.x() > -detailedFieldRadiusM && p.x() < detailedFieldRadiusM &&
                p.y() > -detailedFieldRadiusM && p.y() < detailedFieldRadiusM) {
                SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Adding detailed cloud at " << p.x() << " " << p.y() << " " << p.z() << " d: " << p.length());
                auto localCloud = std::make_pair(c, p);
                snap.detailedFieldList.push_back(localCloud);
                snap.cloudbaseM = std::min(p.z(), snap.cloudbaseM);
            }

            // Only use the rough field map if we aren't using a repeating (detailed) field)
            if (!_fieldRepeating &&
                p.x() > -roughFieldRadiusM && p.x() < roughFieldRadiusM &&
                p.y() > -roughFieldRadiusM && p.y() < roughFieldRadiusM) {
                // Local coordinate cloud placement
                SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Adding rough cloud at " << p.x() << " " << p.y() << " " << p.z() << " d: " << p.length());
                auto localCloud = std::make_pair(c, p);
                snap.roughFieldList.push_back(localCloud);
                snap.cloudbaseM = std::min(p.z(), snap.cloudbaseM);
            }
        }
    }
    return snap;
}

// Build the cloud field centered on the current location.
FGClouds::RebuildResult FGClouds::runRebuild(RebuildSnapshot snap)
{
    RebuildResult result;
    result.fieldRepeating = snap.fieldRepeating;
    result.cloudbaseM = snap.cloudbaseM;

    if ((snap.fieldRepeating && snap.detailedFieldList.empty()) ||
        (!snap.fieldRepeating && snap.roughFieldList.empty())) {
        // Nothing to display, so clean up and return early.
        osg::ref_ptr<osg::Image> dummyVoxelData = new osg::Image();
        dummyVoxelData->setName("Dummy Cloud Voxel Data");
        dummyVoxelData->allocateImage(1, 1, 1, GL_RGBA, GL_FLOAT);
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "rebuildField - No cloud data in range");
        result.detailedVoxelData = dummyVoxelData;
        result.roughVoxelData = dummyVoxelData;
        return result;
    }

    const int detailedVoxelSpaceSizeMBytes = snap.detailedFieldWidth * snap.detailedFieldWidth * snap.detailedFieldHeight * 12 / 1024 / 1024;
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Rebuilding Cloud voxel field");
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Detailed Voxel size: " << snap.detailedFieldVoxelSize << "m");
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Detailed Voxel space: " << snap.detailedFieldWidth << " x " << snap.detailedFieldWidth << " x " << snap.detailedFieldHeight << " total size: " << detailedVoxelSpaceSizeMBytes << " MB");
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Detailed Voxel space: " << (snap.detailedFieldWidth * snap.detailedFieldVoxelSize / 1000.0) << "x" << (snap.detailedFieldWidth * snap.detailedFieldVoxelSize / 1000.0) << "km " << (snap.detailedFieldHeight * snap.detailedFieldVoxelSize * SG_METER_TO_FEET) << "ft");

    if (!snap.fieldRepeating) {
        // As the atmosphere is thin, we assume the detailed field is sufficiently
        // high to include the entire troposphere, so therefore the rough field height is
        // calculated automatically.
        const int roughVoxelSpaceSizeMBytes = snap.roughFieldWidth * snap.roughFieldWidth * snap.roughFieldHeight * 12 / 1024 / 1024;
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Rough Voxel size: " << snap.roughFieldVoxelSize << "m");
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Rough Voxel space" << snap.roughFieldWidth << "x" << snap.roughFieldWidth << "x" << snap.roughFieldHeight << " total size: " << roughVoxelSpaceSizeMBytes << " MB");
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Rough Voxel space: " << (snap.roughFieldWidth * snap.roughFieldVoxelSize / 1000.0) << "x" << (snap.roughFieldWidth * snap.roughFieldVoxelSize / 1000.0) << "x" << (snap.roughFieldHeight * snap.roughFieldVoxelSize / 1000.0) << " km");
    }

    result.roughVoxelData = new osg::Image();
    result.roughVoxelData->setName("Rough Cloud Voxel Data");
    result.roughVoxelData->setFileName("Rough Cloud Voxel Data");
    result.roughVoxelData->allocateImage(snap.roughFieldWidth, snap.roughFieldWidth, snap.roughFieldHeight, GL_RGBA, GL_FLOAT);

    result.detailedVoxelData = new osg::Image();
    result.detailedVoxelData->setName("Detailed Cloud Voxel Data");
    result.detailedVoxelData->setFileName("Detailed Cloud Voxel Data");
    result.detailedVoxelData->allocateImage(snap.detailedFieldWidth, snap.detailedFieldWidth, snap.detailedFieldHeight, GL_RGBA, GL_FLOAT);

    // By default OSG will de-reference the image data once it's loaded into OpenGL (and the GPU) to save memory.
    // However we need the voxel data in subsequent frames to regenerate the shade texture.  Set data variance to dynamic to disable this.
    result.roughVoxelData->setDataVariance(osg::Object::DYNAMIC);
    result.detailedVoxelData->setDataVariance(osg::Object::DYNAMIC);

    result.windOffsetData = new osg::Image();
    result.windOffsetData->setName("Wind Offset Data");
    result.windOffsetData->setFileName("Wind Offset Data");
    result.windOffsetData->allocateImage(snap.detailedFieldHeight, 1, 1, GL_RGBA, GL_FLOAT);

    // Set up the wind column
    const std::size_t width = result.windOffsetData->s();
    float* raw = reinterpret_cast<float*>(result.windOffsetData->data());
    for (std::size_t i = 0; i < width; i++) {
        raw[i * 4 + 0] = 0.0f;
        raw[i * 4 + 1] = 0.0f;
        raw[i * 4 + 2] = 0.0f;
        raw[i * 4 + 3] = 0.0f;
    }

    // The alpha value is use for a Signed Distance Field, and indicates the maximum distance that can be travelled
    // before hitting something in UV coordinates.  We default to 1 pixel.
    const float roughSDFMin = 1.0f / (float)snap.roughFieldWidth;
    const float detailedSDFMin = 1.0f / (float)snap.detailedFieldWidth;

    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "SDF Minima: detailed: " << detailedSDFMin << " rough: " << roughSDFMin);

    float* data = reinterpret_cast<float*>(result.roughVoxelData->data());
    size_t numVoxels = snap.roughFieldWidth * snap.roughFieldWidth * snap.roughFieldHeight;
    for (size_t i = 0; i < numVoxels; ++i) {
        data[i * 4 + 0] = 0.0f; // x
        data[i * 4 + 1] = 0.0f; // y
        data[i * 4 + 2] = 0.0f; // z (density)
        data[i * 4 + 3] = roughSDFMin;
    }

    data = reinterpret_cast<float*>(result.detailedVoxelData->data());
    numVoxels = snap.detailedFieldWidth * snap.detailedFieldWidth * snap.detailedFieldHeight;
    for (size_t i = 0; i < numVoxels; ++i) {
        data[i * 4 + 0] = 0.0f; // x
        data[i * 4 + 1] = 0.0f; // y
        data[i * 4 + 2] = 0.0f; // z (density)
        data[i * 4 + 3] = detailedSDFMin;
    }

    // Now write the detailed clouds into the voxel space.
    //
    // The Voxel layout is as follows
    // [0] .x - Dimension.  This is a positive gradient with 1.0 at the center of the cloud, and 0.0 at the edge
    // [1] .y - Type.  From wispy (0.0) to billowy (1.0)
    // [2] .z - Density.  0.0 is no cloud density, 1.0 is fully opaque density.  Use this to determine if there is any cloud at this location.
    // [3] .a - Signed Distance Field in UV space.  The maximum radius sphere centered on this point that doesn't contain any cloud density.  Used for adaptive ray marching.
    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Creating detailed field of " << snap.detailedFieldList.size() << " clouds");
    const osg::Vec3f zOffset = osg::Vec3f(0.0, 0.0, -snap.cloudbaseM);

    for (const auto& cl : snap.detailedFieldList) {
        const SGVoxelCloud* c = cl.first;
        osg::Vec3f p = cl.second + zOffset;
        double z = (double)c->addCloudToDetailedVoxelField(result.detailedVoxelData, (float)snap.detailedFieldVoxelSize, p);
        result.maxZ = std::max(z, result.maxZ);
    }

    // Generate SDF
    generateSDF(result.detailedVoxelData);

    if (!snap.fieldRepeating) {
        // Now generate the rough voxel space in a similar manner
        for (const auto& cl : snap.roughFieldList) {
            const SGVoxelCloud* c = cl.first;
            osg::Vec3f p = cl.second + zOffset;
            double z = (double)c->addCloudToRoughVoxelField(result.roughVoxelData, (float)snap.roughFieldVoxelSize, p);
            result.maxZ = std::max(z, result.maxZ);
        }

        // Generate an SDF
        generateSDF(result.roughVoxelData);
    }

    return result;
}

// Build the shade texture from the (already-built) detailed voxel field and the
// current sun direction.  Run independently of runRebuild() above so that it can be
// re-triggered whenever the sun direction has changed significantly, without having
// to rebuild the rest of the voxel field.
FGClouds::ShadeRebuildResult FGClouds::runShadeRebuild(ShadeSnapshot snap)
{
    ShadeRebuildResult result;

    result.voxelShadeData = new osg::Image();
    result.voxelShadeData->setName("Voxel Shade Data");
    result.voxelShadeData->setFileName("Voxel Shade Data");
    result.voxelShadeData->allocateImage(snap.width, snap.width, snap.height, GL_RGBA, GL_FLOAT);

    const float* voxelRaw = snap.voxelData.data();
    float* shadeRaw = reinterpret_cast<float*>(result.voxelShadeData->data());

    // Zero the output
    const size_t numFloats = (size_t)snap.width * snap.width * snap.height * 4;
    std::fill(shadeRaw, shadeRaw + numFloats, 0.0f);

    SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "Sun Direction Z-Up: " << snap.sunDirVoxel.x() << ", " << snap.sunDirVoxel.y() << ", " << snap.sunDirVoxel.z());

    // sunDirVoxel Z needs to be scaled up because the voxel space is not a cube.
    osg::Vec3f marchDir = snap.sunDirVoxel;
    marchDir.z() = marchDir.z() * (float)snap.width / (float)snap.height;
    marchDir.normalize();

    // Step size in voxels - sub-voxel to avoid aliasing at low sun angles
    const float STEP_SIZE = 0.5f;
    const osg::Vec3f step = marchDir * STEP_SIZE;

    // Trilinear interpolation of density from the voxel field.
    // p is in voxel coordinates [0..width) x [0..width) x [0..height).
    // Returns 0 outside the field.
    auto sampleDensity = [&](float px, float py, float pz) -> float {
        // Clamp-to-border: outside field = no density
        if (px < 0.0f || px >= float(snap.width) - 1.0f ||
            py < 0.0f || py >= float(snap.width) - 1.0f ||
            pz < 0.0f || pz >= float(snap.height) - 1.0f)
            return 0.0f;

        int x0 = int(px), y0 = int(py), z0 = int(pz);
        int x1 = x0 + 1, y1 = y0 + 1, z1 = z0 + 1;
        float fx = px - x0, fy = py - y0, fz = pz - z0;

        auto d = [&](int x, int y, int z) -> float {
            return voxelRaw[(z * snap.width * snap.width + y * snap.width + x) * 4 + 2];
        };

        // Trilinear interpolation
        return (1 - fz) * ((1 - fy) * ((1 - fx) * d(x0, y0, z0) + fx * d(x1, y0, z0)) + fy * ((1 - fx) * d(x0, y1, z0) + fx * d(x1, y1, z0))) +
               fz * ((1 - fy) * ((1 - fx) * d(x0, y0, z1) + fx * d(x1, y0, z1)) + fy * ((1 - fx) * d(x0, y1, z1) + fx * d(x1, y1, z1)));
    };

    // For each voxel, ray-march toward the sun accumulating optical depth
    for (int k = 0; k < snap.height; ++k) {
        for (int j = 0; j < snap.width; ++j) {
            for (int i = 0; i < snap.width; ++i) {
                float sunOpticalDepth = 0.0f;

                // Start half a step above this voxel (avoid self-shadowing)
                float px = float(i) + step.x() * 0.5f;
                float py = float(j) + step.y() * 0.5f;
                float pz = float(k) + step.z() * 0.5f;

                while (px >= 0.0f && px < float(snap.width) &&
                       py >= 0.0f && py < float(snap.width) &&
                       pz >= 0.0f && pz < float(snap.height)) {
                    float density = sampleDensity(px, py, pz);
                    sunOpticalDepth += density * snap.voxelOpticalDepth * STEP_SIZE;

                    px += step.x();
                    py += step.y();
                    pz += step.z();
                }

                const int idx = (k * snap.width * snap.width + j * snap.width + i) * 4;
                shadeRaw[idx + 0] = sunOpticalDepth;
                shadeRaw[idx + 1] = 0.0f; // filled by vertical pass below
                shadeRaw[idx + 2] = 0.0f;
                shadeRaw[idx + 3] = 0.0f;
            }
        }
    }

    // --- VERTICAL SKY DENSITY (top-down, independent of sun) ---
    for (int k = snap.height - 1; k >= 0; --k) {
        for (int j = 0; j < snap.width; ++j) {
            for (int i = 0; i < snap.width; ++i) {
                const int idx = (k * snap.width * snap.width + j * snap.width + i) * 4;
                float density = voxelRaw[idx + 2];

                float verticalOpticalDepth = 0.0f;
                if (k < snap.height - 1) {
                    verticalOpticalDepth = shadeRaw[((k + 1) * snap.width * snap.width + j * snap.width + i) * 4 + 1];
                }
                verticalOpticalDepth += density * snap.voxelOpticalDepth;
                shadeRaw[idx + 1] = verticalOpticalDepth;
            }
        }
    }

    return result;
}

void FGClouds::commitResult(RebuildResult result)
{
    // Setting property values needs to be done on the main thread as an atomic operation

    // We pad the cloud field height slightly so that we get smooth interpolation of density values in the shader.
    const double cloudFieldHeightM = (result.maxZ + 0.5f) * _detailedFieldVoxelSize;
    _activeVoxelFieldHeightNorm = cloudFieldHeightM / (double)(_detailedFieldHeight * _detailedFieldVoxelSize);
    _cloudBaseM = (double)result.cloudbaseM;
    _cloudBaseZNorm = (double)result.cloudbaseM / (double)(_detailedFieldHeight * _detailedFieldVoxelSize);
    _mirrorU = false;
    _mirrorV = false;

    // Keep the images alive as members of FGClouds
    _detailedVoxelData = result.detailedVoxelData;
    _roughVoxelData = result.roughVoxelData;
    _windOffsetData = result.windOffsetData;

    // Push them into the textures
    simgear::StateAttributeFactory::instance()->setCloudVoxelImages(_detailedVoxelData, _roughVoxelData, result.fieldRepeating);
    simgear::StateAttributeFactory::instance()->setCloudWindOffsetImage(_windOffsetData);
}

FGClouds::ShadeSnapshot FGClouds::captureShadeSnapshot()
{
    assert(SGThreads::isMainThread());

    ShadeSnapshot snap;
    snap.width = _detailedVoxelData->s();
    snap.height = _detailedVoxelData->r();

    // Copy the raw voxel data out of the live osg::Image on the main thread - see
    // the comment on ShadeSnapshot::voxelData for why we don't just hand the
    // background thread a ref_ptr to the (GPU-texture-backed) image itself.
    const float* voxelRaw = reinterpret_cast<const float*>(_detailedVoxelData->data());
    const size_t numFloats = (size_t)snap.width * (size_t)snap.width * (size_t)snap.height * 4;
    snap.voxelData.assign(voxelRaw, voxelRaw + numFloats);

    auto cloudsProp = globals->get_props()->getNode("/sim/rendering/hdr/clouds/");
    snap.voxelOpticalDepth = cloudsProp->getFloatValue("voxel-optical-depth", 2.5);
    snap.sunDirVoxel = computeSunDirVoxel();
    return snap;
}

void FGClouds::commitShadeResult(ShadeRebuildResult result)
{
    _voxelShadeData = result.voxelShadeData;
    simgear::StateAttributeFactory::instance()->setCloudShadeImage(_voxelShadeData);
}

void FGClouds::generateSDF(osg::ref_ptr<osg::Image> voxelImage)
{
    // Build the SDF from a set of voxel data.
    vector<std::array<int, 3>> cloudBoundaryIndices;
    vector<float> cloudBoundaryDistances;

    cloudBoundaryIndices.reserve(voxelImage->s() * voxelImage->t() * voxelImage->r() / 8); // rough estimate
    cloudBoundaryDistances.reserve(voxelImage->s() * voxelImage->t() * voxelImage->r() / 8);

    assert(voxelImage->s() == voxelImage->t());
    size_t width = (size_t)voxelImage->s();
    size_t height = (size_t)voxelImage->r();

    const float* raw = reinterpret_cast<const float*>(voxelImage->data());
    for (size_t k = 0; k < height; ++k) {
        for (size_t j = 0; j < width; ++j) {
            for (size_t i = 0; i < width; ++i) {
                size_t pixIdx = (k * width * width + j * width + i) * 4;
                if (raw[pixIdx + 2] > 0.0f) { // density channel
                    cloudBoundaryIndices.push_back({(int)i, (int)j, (int)k});
                    cloudBoundaryDistances.push_back(0.0f);
                }
            }
        }
    }

    if (cloudBoundaryDistances.empty()) {
        // This is an error condition
        SG_LOG(SG_ENVIRONMENT, SG_DEV_ALERT, "No clouds in voxel data for image " << voxelImage->getName());
        return;
    }

    try {
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "SDF calculation started for " << voxelImage->getName());
        float maxDistance = 0.0f;
        auto gridSize = std::array<size_t, 3>{{width, width, height}};
        auto sdf = fmm::SignedArrivalTime(
            gridSize,
            cloudBoundaryIndices,
            cloudBoundaryDistances,
            fmm::DistanceSolver<float, 3>(1.0));

        // The SDF is now calculated, so write it back to the voxel data.
        float* raw = reinterpret_cast<float*>(voxelImage->data());
        std::size_t idx = 0;
        for (std::size_t k = 0; k < height; ++k) {
            for (std::size_t j = 0; j < width; ++j) {
                for (std::size_t i = 0; i < width; ++i, ++idx) {
                    float distance = sdf[idx];
                    maxDistance = std::max(maxDistance, distance);
                    size_t pixIdx = (k * width * width + j * width + i) * 4;
                    if (raw[pixIdx + 3] > 0.0f) {
                        raw[pixIdx + 3] = distance / float(width);
                    }
                }
            }
        }

        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "SDF calculation complete. Maximum distance " << maxDistance);
    } catch (const std::exception& e) {
        // The fmm may through exceptions if it is unable to generate an SDF.  Given that
        // our data has a random element, this is insufficient reason to terminate FlightGear,
        // so we will simply log this.
        _FMMExceptionCount++;
        SG_LOG(SG_ENVIRONMENT, SG_DEV_ALERT, "Cloud Fast Marching Method to generate SDF threw exception'" << e.what() << "'. Ignoring.  Cloud ray-marching will be inefficient. Total exceptions: " << _FMMExceptionCount);
    }
}

// Update the wind column data, ready to be pushed to the Uniform during the updateFromOSGTraversal
void FGClouds::updateWindColumn(double dt, FGEnvironment* env)
{
    if (!_windOffsetData) return; // Early return if no cloud data has been generated

    const std::size_t width = _windOffsetData->s();
    const float fieldWidthM = (float)_detailedFieldVoxelSize * _detailedFieldWidth;
    float* raw = reinterpret_cast<float*>(_windOffsetData->data());
    for (std::size_t i = 0; i < width; i++) {
        // Work out the offset in normalized UV coordinates
        raw[i * 4] += env->get_wind_from_north_fps() * dt * SG_FEET_TO_METER / fieldWidthM;
        raw[i * 4 + 1] += env->get_wind_from_east_fps() * dt * SG_FEET_TO_METER / fieldWidthM;
        raw[i * 4 + 2] = 0.0f;
        raw[i * 4 + 3] = 0.0f;
    }
}

void FGClouds::updateFromOsgTraversal()
{
    bool shadeDirty = false;

    // Poll for a completed background rebuild first
    if (_rebuildFuture.valid() &&
        _rebuildFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        commitResult(_rebuildFuture.get()); // non-blocking — already done
        // Don't clear _fieldDirty here; a new mutation may have arrived
        // while the last rebuild was in flight — see below.
        shadeDirty = true;
    }

    // Launch a new rebuild if dirty and nothing is running
    if (_fieldDirty && (!_rebuildFuture.valid() ||
                        _rebuildFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)) {
        _fieldDirty = false; // clear before launch; any new mutation re-sets it
        RebuildSnapshot snap = captureSnapshot();
        _rebuildFuture = std::async(std::launch::async,
                                    &FGClouds::runRebuild, std::move(snap));
    }

    // Poll for a completed background shade rebuild first
    if (_shadeRebuildFuture.valid() &&
        _shadeRebuildFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        commitShadeResult(_shadeRebuildFuture.get()); // non-blocking — already done
    }

    // Mark the shade texture dirty if the sun has moved more than the configured
    // threshold since the last shade build. This lets the shade texture
    // track the sun (e.g. time-of-day changes) independently of cloud rebuilds.
    if (_detailedVoxelData && !shadeDirty) {
        const osg::Vec3f sunDirVoxel = computeSunDirVoxel();
        const float cosAngle = sunDirVoxel * _lastShadeSunDir;
        const float thresholdDeg = _shadeUpdateAngleDeg;
        if (cosAngle < std::cos(thresholdDeg * SG_DEGREES_TO_RADIANS)) {
            shadeDirty = true;
        }
    }

    // Launch a new shade rebuild if dirty, nothing is running, and there is
    // some voxel data to shade.
    if (shadeDirty && _detailedVoxelData &&
        (!_shadeRebuildFuture.valid() ||
         _shadeRebuildFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)) {
        ShadeSnapshot snap = captureShadeSnapshot();
        _lastShadeSunDir = snap.sunDirVoxel;
        _shadeRebuildFuture = std::async(std::launch::async,
                                         &FGClouds::runShadeRebuild, std::move(snap));
    }

    if (_fieldRepeating) updateRepeatingField();

    // Adjust the altitude of the cloud base
    const double cloudBaseM = _cloudBaseM;
    const double fieldHeightM = (double)(_detailedFieldHeight * _detailedFieldVoxelSize);
    _cloudBaseZNorm = cloudBaseM / fieldHeightM;

    // Write the wind offset data to the Uniform
    simgear::StateAttributeFactory::instance()->setCloudWindOffsetImage(_windOffsetData);
}

void FGClouds::updateRepeatingField()
{
    // Adjust the position and orientation of the voxel field to ensure that it is approximately
    // tangent to the local surface if the camera has moved a significant distance.  To make this
    // as seamless as possible, we move it by a whole UV space, taking advantage of the fact the
    // field is repeating.
    const float fieldWidthM = _detailedFieldWidth * _detailedFieldVoxelSize;
    SGVec3<double> currentCart;

    SGGeod currentGeod = globals->get_view_position();
    currentGeod.setElevationM(0);
    SGGeodesy::SGGeodToCart(currentGeod, currentCart);

    osg::Matrixf currentPosMatrix = makeZUpFrameRelative(currentGeod);
    osg::Vec3f localDrift = currentPosMatrix * (toOsg(currentCart) - toOsg(_centerCart));

    // Snap drift to nearest field-width multiple in XY
    float snapX = std::round(localDrift.x() / fieldWidthM);
    float snapY = std::round(localDrift.y() / fieldWidthM);

    if (std::abs(snapX) >= 1.0f || std::abs(snapY) >= 1.0f) {
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "_centerCart before snap: " << _centerCart.x() << ", " << _centerCart.y() << ", " << _centerCart.z());
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "localDrift: " << localDrift.x() << ", " << localDrift.y() << ", " << localDrift.z());
        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "snapX: " << snapX << " snapY: " << snapY);

        // Move centerCart by the snap in local space, converting via currentPosMatrix
        osg::Vec3d snapLocal(snapX * fieldWidthM, snapY * fieldWidthM, 0.0);
        osg::Vec3d snapECEF = osg::Matrix::inverse(currentPosMatrix) * snapLocal;
        _centerCart = _centerCart + toSG(snapECEF);

        SG_LOG(SG_ENVIRONMENT, SG_DEBUG, "_centerCart after snap: " << _centerCart.x() << ", " << _centerCart.y() << ", " << _centerCart.z());
        _cloudCenterX = _centerCart.x();
        _cloudCenterY = _centerCart.y();
        _cloudCenterZ = _centerCart.z();

        // The detailed texture wrap is set to MIRROR to ensure that the SDF is correct across the UV boundaries.
        // However this means that shifting by U=1 or V=1 results in a mirrored image. To compensate we tell
        // the shader to mirror the coordinates.  We could shift by 2xfieldWidth, and therefore U=2, but this results
        // in too large a rotation of the up vector.
        if (std::abs(snapX) >= 1.0f) {
            _mirrorU = !_mirrorU;
        }

        if (std::abs(snapY) >= 1.0f) {
            _mirrorV = !_mirrorV;
        }
    }
}
