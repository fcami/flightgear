// SPDX-FileCopyrightText: 2021 James Hogan <james@albanarts.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef VRMANAGER_HXX
#define VRMANAGER_HXX 1

#include <config.h>

#ifdef ENABLE_OSGXR

#include <osg/ref_ptr>
#include <osg/observer_ptr>

#include <osgXR/Manager>
#include <osgXR/Space>

#include <simgear/props/propertyObject.hxx>
#include <simgear/scene/viewer/CompositorPass.hxx>

#include "CameraGroup.hxx"

#include <map>

namespace flightgear
{

class VRManager : public osgXR::Manager
{
    public:

        class ReloadCompositorCallback : public CameraInfo::ReloadCompositorCallback
        {
            public:

                ReloadCompositorCallback(VRManager *manager) :
                    _manager(manager)
                {
                }

                void preReloadCompositor(CameraGroup *cgroup, CameraInfo *info) override
                {
                    _manager->preReloadCompositor(cgroup, info);
                }

                void postReloadCompositor(CameraGroup *cgroup, CameraInfo *info) override
                {
                    _manager->postReloadCompositor(cgroup, info);
                }

            protected:

                osg::observer_ptr<VRManager> _manager;
        };

        class ViewCallback : public osgXR::View::Callback
        {
            public:

                ViewCallback(VRManager *manager) :
                    _manager(manager)
                {
                }

                // Overridden from osgXR::View::Callback
                void updateSubView(osgXR::View *view, unsigned int subviewIndex,
                                   const osgXR::View::SubView &subview) override
                {
                    _manager->updateSubView(view, subviewIndex, subview);
                }

            protected:

                osg::observer_ptr<VRManager> _manager;
        };

        VRManager();
        virtual ~VRManager();

        static VRManager* instance(bool destroy = false);
        static void destroyInstance()
        {
            instance(true);
        }
        // Call after reset, threading should still be disabled
        void reset();

        void syncProperties();
        void syncReadOnlyProperties();
        void syncSettingProperties();

        // Settings

        bool getUseMirror() const;

        void setValidationLayer(bool validationLayer);
        void setDepthInfo(bool depthInfo);
        void setVisibilityMask(bool visibilityMask);

        void setVRMode(const std::string& mode);
        void setSwapchainMode(const std::string& mode);
        void setMirrorMode(const std::string& mode);

        // osgXR::Manager overrides

        void update() override;

        void doCreateView(osgXR::View *xrView) override;
        void doDestroyView(osgXR::View *xrView) override;

        void onRunning() override;
        void onStopped() override;
        void onUserPresence(bool userPresent) override;

        // Callback entry points

        void preReloadCompositor(CameraGroup *cgroup, CameraInfo *info);
        void postReloadCompositor(CameraGroup *cgroup, CameraInfo *info);

        void updateSubView(osgXR::View *view, unsigned int subviewIndex,
                           const osgXR::View::SubView &subview);

        // Commands

        bool cmdRecenter(const SGPropertyNode* arg, SGPropertyNode* root);

        // Access head space
        osgXR::Space* getHeadSpace()
        {
            return _headSpace;
        }

    protected:

        typedef std::map<osgXR::View *, osg::ref_ptr<CameraInfo>> XRViewToCamInfo;
        XRViewToCamInfo _camInfos;

        typedef std::map<CameraInfo *, osg::ref_ptr<osgXR::View>> CamInfoToXRView;
        CamInfoToXRView _xrViews;

        osg::ref_ptr<ReloadCompositorCallback> _reloadCompositorCallback;

        osg::ref_ptr<osgXR::Space> _headSpace;

        // Properties

        SGPropObjBool _propXrLayersValidation;
        SGPropObjBool _propXrExtensionsDepthInfo;
        SGPropObjBool _propXrExtensionsVisibilityMask;
        SGPropObjString _propXrRuntimeName;
        SGPropObjString _propXrSystemName;
        SGPropObjBool _propXrSystemUserPresence;

        SGPropObjString _propStateString;
        SGPropObjBool _propPresent;
        SGPropObjBool _propRunning;
        SGPropObjBool _propUserPresent;

        SGPropObjBool _propEnabled;
        SGPropObjBool _propDepthInfo;
        SGPropObjBool _propVisibilityMask;
        SGPropObjBool _propValidationLayer;
        SGPropObjString _propMode;
        SGPropObjString _propSwapchainMode;
        SGPropObjBool _propMirrorEnabled;
        SGPropObjString _propMirrorMode;

        // Property listeners

        template <typename T, typename R = T>
        class Listener : public SGPropertyChangeListener
        {
            public:
                typedef void (VRManager::*SetterFn)(R v);

                Listener(VRManager *manager, SetterFn setter) :
                    _manager(manager),
                    _setter(setter)
                {
                }

                void valueChanged(SGPropertyNode *node) override
                {
                    (_manager->*_setter)(node->template getValue<T>());
                }

            protected:

                VRManager *_manager;
                SetterFn _setter;
        };
        typedef Listener<bool> ListenerBool;
        typedef Listener<std::string, const std::string&> ListenerString;

        ListenerBool _listenerEnabled;
        ListenerBool _listenerDepthInfo;
        ListenerBool _listenerVisibilityMask;
        ListenerBool _listenerValidationLayer;
        ListenerString _listenerMode;
        ListenerString _listenerSwapchainMode;
        ListenerString _listenerMirrorMode;
};

}

#endif // ENABLE_OSGXR

#endif
