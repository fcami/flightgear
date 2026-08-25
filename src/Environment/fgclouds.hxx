// Build a cloud layer based on metar
//
// Written by Harald JOHNSEN, started April 2005.
//
// SPDX-FileCopyrightText: 2005 Harald JOHNSEN <hjohnsen@evc.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <osg/Image>
#include <string>
#include <vector>

#include <atomic>
#include <future>
#include <mutex>

#include <Environment/environment.hxx>
#include <simgear/props/propertyObject.hxx>
#include <simgear/scene/util/SGReaderWriterOptions.hxx>

using std::vector;

// forward decls
class SGPropertyNode;
class SGCloudField;
class SGVoxelCloud;


class FGClouds
{
private:
    // Snapshot of everything the background thread needs - captured on the
    // main thread before the async task is launched.
    struct RebuildSnapshot {
        // Copy of placement map - raw pointers are safe because SGVoxelCloud
        // objects are owned by _cloudPlacementMap and outlive the task.
        using LocalPlacement = std::pair<const SGVoxelCloud*, osg::Vec3f>;
        std::vector<LocalPlacement> detailedFieldList;
        std::vector<LocalPlacement> roughFieldList;

        // Config captured at snapshot time
        int detailedFieldWidth{};
        int detailedFieldHeight{};
        int detailedFieldVoxelSize{};
        int roughFieldWidth{};
        int roughFieldHeight{};
        int roughFieldVoxelSize{};
        int roughVoxelSizeFactor{};
        float voxelOpticalDepth{};
        bool fieldRepeating{};

        // Field center for fgSet* calls at commit time
        SGVec3<double> centerCart;
        osg::Matrixf cloudPosMatrix;
        float cloudbaseM = 999999.0f;
    };

    // Output bundle produced by the background thread
    struct RebuildResult {
        osg::ref_ptr<osg::Image> detailedVoxelData;
        osg::ref_ptr<osg::Image> roughVoxelData;
        osg::ref_ptr<osg::Image> windOffsetData;
        double maxZ = 0.0f;
        float cloudbaseM = 999999.0f;
        bool fieldRepeating = true;
    };

    // Snapshot needed to (re)build just the shade texture, independently of
    // the rest of the voxel field.  This is cheap enough to redo whenever the
    // sun direction has moved significantly, without re-running the full
    // cloud placement/voxel field rebuild.
    struct ShadeSnapshot {
        // A private copy of the detailed voxel field's raw RGBA float data, taken on
        // the main thread.  We deliberately copy rather than share the osg::Image with
        // the background shade-rebuild thread: that Image is also live in an
        // osg::Texture3D and can be touched concurrently by the render/GPU-upload
        // path, which is a data race even though osg::Referenced ref-counting itself
        // is atomic.
        std::vector<float> voxelData;
        int width{};
        int height{};
        float voxelOpticalDepth{};

        // Sun direction (world-space, already transformed to Z-up)
        osg::Vec3f sunDirVoxel;
    };

    struct ShadeRebuildResult {
        osg::ref_ptr<osg::Image> voxelShadeData;
    };

    // Protects _cloudPlacementMap from concurrent access between
    // addCloud/removeCloud (main thread) and snapshot capture.
    mutable std::mutex _placementMutex;

    // Set on any mutation; cleared once a new async rebuild is launched.
    std::atomic<bool> _fieldDirty{false};

    // The running or completed async task.  checked in updateFromOsgTraversal.
    std::future<RebuildResult> _rebuildFuture;

    // The running or completed async shade-only rebuild task.
    std::future<ShadeRebuildResult> _shadeRebuildFuture;

    // Set to true by the background thread when it finishes.
    std::atomic<bool> _rebuildComplete{false};

    // Sun direction (Z-up, voxel space) used for the most recently launched
    // shade build, so we can detect when it has drifted enough to need redoing.
    osg::Vec3f _lastShadeSunDir;

    size_t _roughFieldWidth;
    size_t _roughFieldHeight;
    size_t _roughFieldVoxelSize;
    size_t _detailedFieldWidth;
    size_t _detailedFieldHeight;
    size_t _detailedFieldVoxelSize;

    typedef std::pair<std::unique_ptr<const SGVoxelCloud>, osg::Vec3f> CloudPlacement;

    typedef std::unordered_map<int, CloudPlacement> CloudPlacementMap;

    double buildCloud(SGPropertyNode* cloud_def_root, SGPropertyNode* box_def_root,
                      const std::string& name, double altM, double grid_z_rand, SGCloudField* layer);
    void buildLayer(int iLayer, const std::string& name, double coverage, double altM);

    void buildCloudLayers(void);

    int update_event;

    int index;

    CloudPlacementMap _cloudPlacementMap;

    // This is the ECF cartesian coordinates of the voxel field.
    SGVec3d _centerCart;
    osg::Matrixd _cloudPosMatrix;

    // Whether the cloud field is repeating (by simply mirroring the voxel space texture)
    bool _fieldRepeating;

    osg::ref_ptr<simgear::SGReaderWriterOptions> _options;

    // Properties that are subsequently mapped to Uniforms by the Effects system
    SGPropObjDouble _cloudBaseM;
    SGPropObjDouble _cloudBaseZNorm;
    SGPropObjDouble _cloudCenterX;
    SGPropObjDouble _cloudCenterY;
    SGPropObjDouble _cloudCenterZ;
    SGPropObjBool _mirrorU;
    SGPropObjBool _mirrorV;
    SGPropObjBool _cloudFieldRepeating;
    SGPropObjDouble _activeVoxelFieldHeightNorm;

    // Minimum sun direction change (in degrees) that triggers a shade rebuild.
    SGPropObjDouble _shadeUpdateAngleDeg;

    // A node in the scenegraph purely used to ensure that the voxel data
    // is modified during the update traversal.
    osg::ref_ptr<osg::Group> _cloudUpdateNode;

    // The voxel images.  These are kept in FGClouds to ensure they aren't
    // destroyed by OSG
    osg::ref_ptr<osg::Image> _detailedVoxelData;
    osg::ref_ptr<osg::Image> _roughVoxelData;
    osg::ref_ptr<osg::Image> _voxelShadeData;
    osg::ref_ptr<osg::Image> _windOffsetData;

    bool add3DCloud(const SGPropertyNode* arg, SGPropertyNode* root);
    bool delete3DCloud(const SGPropertyNode* arg, SGPropertyNode* root);
    bool move3DCloud(const SGPropertyNode* arg, SGPropertyNode* root);

    /**
     * Add a new cloud with a given index at a specific point defined by lon/lat and an x/y offset
     */
    bool addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, float lon, float lat, float alt, float x, float y);
    bool addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, SGGeod loc, float x, float y);
    bool addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, float lon, float lat, float alt);
    bool addCloud(std::unique_ptr<const SGVoxelCloud> cloud, int index, SGGeod loc);

    // add one cloud, data is not copied, ownership given
    void addCloud(SGVec3f& pos, const SGVoxelCloud* cloud);

    // Cloud handling functions.
    bool removeCloud(int index);
    bool repositionCloud(int index, float lon, float lat, float alt);
    bool repositionCloud(int index, float lon, float lat, float alt, float x, float y);

    // Utility functions
    static void generateSDF(osg::ref_ptr<osg::Image>);
    osg::Vec3f getFinalPos(SGGeod loc, float x, float y);

    // Sun direction in voxel (Z-up, field-relative) space. Shared
    // by both the full rebuild snapshot and the shade-only snapshot.
    osg::Vec3f computeSunDirVoxel() const;

    // Asynchronous rebuild of the cloud layers.
    RebuildSnapshot captureSnapshot();
    static RebuildResult runRebuild(RebuildSnapshot snap);
    void commitResult(RebuildResult result);

    // Asynchronous rebuild of just the shade texture - independent of the
    // cloud placement/voxel field rebuild above, so it can be re-run whenever
    // the sun direction changes significantly without rebuilding everything.
    ShadeSnapshot captureShadeSnapshot();
    static ShadeRebuildResult runShadeRebuild(ShadeSnapshot snap); // static = no 'this' access
    void commitShadeResult(ShadeRebuildResult result);

public:
    FGClouds();
    ~FGClouds();

    void Init(void);

    int get_update_event(void) const;
    void set_update_event(int count);
    bool get_3dClouds() const;
    void set_3dClouds(bool enable);

    bool isDirty(void) const { return _fieldDirty; }
    void setDirty(bool dirty) { _fieldDirty = dirty; }

    bool isCloudsRepeating(void) const { return _fieldRepeating; }
    void setCloudsRepeating(bool repeat) { _fieldRepeating = repeat; }

    void updateWindColumn(double dt, FGEnvironment* env);
    void updateFromOsgTraversal();
    void updateRepeatingField();
    osg::ref_ptr<osg::Group> getCloudUpdateNode() { return _cloudUpdateNode; }
};

class FGCloudUpdateCallback : public osg::NodeCallback
{
public:
    FGCloudUpdateCallback(FGClouds* clouds)
        : _clouds(clouds) {}

    void operator()(osg::Node* node, osg::NodeVisitor* nv) override
    {
        if (nv->getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR) {
            _clouds->updateFromOsgTraversal();
        }
        traverse(node, nv);
    }

private:
    FGClouds* _clouds;
};
