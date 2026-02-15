// soundmanager.hxx -- Wraps the SimGear OpenAl sound manager class
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2001 Curtis L. Olson

#pragma once

#include <memory>
#include <map>

#include <simgear/props/propertyObject.hxx>
#include <simgear/props/props.hxx>
#include <simgear/sound/soundmgr.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

class FGSampleQueue;
class SGSoundMgr;
class Listener;
class VoiceSynthesizer;

#ifdef ENABLE_AUDIO_SUPPORT
class FGSoundManager : public SGSoundMgr
{
public:
    FGSoundManager();
    virtual ~FGSoundManager();

    // Subsystem API.
    void init() override;
    void reinit() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "sound"; }

    void activate(bool State);
    void update_device_list();

    VoiceSynthesizer * getSynthesizer( const std::string & voice );

private:
    bool stationaryView() const;

    bool playAudioSampleCommand(const SGPropertyNode * arg, SGPropertyNode * root);

    std::map<std::string,SGSharedPtr<FGSampleQueue>> _queue;

    double _active_dt;
    bool _is_initialized, _enabled;
    SGPropertyNode_ptr _sound_working, _sound_enabled, _volume, _device_name;
    SGPropObjBool _headTracked;
    SGPropertyNode_ptr _velocityNorthFPS, _velocityEastFPS, _velocityDownFPS;
    SGPropertyNode_ptr _frozen;
    std::unique_ptr<Listener> _listener;

    std::map<std::string,VoiceSynthesizer*> _synthesizers;
};
#else
#include "Main/fg_props.hxx"

// provide a dummy sound class
class FGSoundManager : public SGSubsystem
{
public:
    FGSoundManager() { fgSetBool("/sim/sound/working", false);}
    ~FGSoundManager() {}

    // Subsystem API.
    void update(double dt) {} override

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "sound"; }
};

#endif // ENABLE_AUDIO_SUPPORT
