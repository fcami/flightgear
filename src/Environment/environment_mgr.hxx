// environment-mgr.hxx -- manager for natural environment information.
//
// Written by David Megginson, started February 2002.
//
// SPDX-FileCopyrightText: 2002 David Megginson <david@megginson.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <simgear/compiler.h>
#include <simgear/math/SGMath.hxx>
#include <simgear/props/tiedpropertylist.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include <cmath>

class FGEnvironment;
class FGClimate;
class FGClouds;
class FGPrecipitationMgr;
class SGSky;
struct FGEnvironmentMgrMultiplayerListener;

/**
* Base class for environment information management to enable mocking.
*/

class EnvironmentManager : public SGSubsystemGroup
{
public:
    virtual ~EnvironmentManager() = default;
    // Subsystem API.
    void bind() = 0;
    InitStatus incrementalInit() = 0;
    void reinit() = 0;
    void shutdown() = 0;
    void unbind() = 0;
    void update(double dt) = 0;
    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "environment"; }

    virtual FGEnvironment getEnvironment() const = 0;

    virtual const FGEnvironment* getAircraftEnvironment() const = 0;

    virtual FGEnvironment getEnvironmentAtPosition(const SGGeod& aPos) = 0;
};

/**
 * Manage environment information.
 */
class FGEnvironmentMgr : public EnvironmentManager
{
public:
    enum {
        MAX_CLOUD_LAYERS = 5
    };

    FGEnvironmentMgr();
    virtual ~FGEnvironmentMgr();

    // Subsystem API.
    void bind() override;
    InitStatus incrementalInit() override;
    void reinit() override;
    void shutdown() override;
    void unbind() override;
    void update(double dt) override;
    /**
     * Get the environment information for the plane's current position.
     */
    virtual FGEnvironment getEnvironment() const;

    const FGEnvironment* getAircraftEnvironment() const;

    virtual FGEnvironment getEnvironmentAtPosition(const SGGeod& aPos);

private:
    friend FGEnvironmentMgrMultiplayerListener;
    void updateClosestAirport();
    void updateTowerPosition();

    double get_cloud_layer_span_m(int index) const;
    void set_cloud_layer_span_m(int index, double span_m);
    double get_cloud_layer_elevation_ft(int index) const;
    void set_cloud_layer_elevation_ft(int index, double elevation_ft);
    double get_cloud_layer_thickness_ft(int index) const;
    void set_cloud_layer_thickness_ft(int index, double thickness_ft);
    double get_cloud_layer_transition_ft(int index) const;
    void set_cloud_layer_transition_ft(int index, double transition_ft);
    const char* get_cloud_layer_coverage(int index) const;
    void set_cloud_layer_coverage(int index, const char* coverage);
    int get_cloud_layer_coverage_type(int index) const;
    void set_cloud_layer_coverage_type(int index, int type);
    double get_cloud_layer_visibility_m(int index) const;
    void set_cloud_layer_visibility_m(int index, double visibility_m);
    double get_cloud_layer_maxalpha(int index) const;
    void set_cloud_layer_maxalpha(int index, double maxalpha);

    FGClimate* _climate = nullptr;
    FGEnvironment* _environment = nullptr; // always the same, for now
    std::vector<FGEnvironment*> environmentCache;

    FGClouds* fgClouds = nullptr;
    int max_tower_height_feet;
    int min_tower_height_feet;
    int default_tower_height_feet;
    bool rebuildCloudLayers = true;

    simgear::TiedPropertyList _tiedProperties;
    FGEnvironmentMgrMultiplayerListener* _multiplayerListener;
    SGSky* _sky;

    SGPropertyNode_ptr towerViewPositionLatDegNode;
    SGPropertyNode_ptr towerViewPositionLonDegNode;
    SGPropertyNode_ptr towerViewPositionAltFtNode;

    const class FGAICarrier* nearestCarrier;
    const class FGAirport* nearestAirport;
};
