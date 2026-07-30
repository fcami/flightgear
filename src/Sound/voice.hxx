// speech synthesis interface subsystem
//
// Written by Melchior FRANZ, started February 2006.
//
// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2006 Melchior Franz

#pragma once

#include <vector>

#include <simgear/compiler.h>
#include <simgear/props/props.hxx>
#include <simgear/io/sg_socket.hxx>
#include <simgear/structure/subsystem_mgr.hxx>

#include <Main/fg_props.hxx>

#if defined(ENABLE_THREADS)
#  include <OpenThreads/Thread>
#  include <OpenThreads/Mutex>
#  include <OpenThreads/ScopedLock>
#  include <OpenThreads/Condition>
#  include <simgear/threads/SGQueue.hxx>
#else
#  include <queue>
#endif // ENABLE_THREADS


class FGVoiceMgr : public SGSubsystem
{
public:
    FGVoiceMgr();
    ~FGVoiceMgr();

    // Subsystem API.
    void init() override;
    void shutdown() override;
    void update(double dt) override;

    // Subsystem identification.
    static const char* staticSubsystemClassId() { return "voice"; }

    class FGVoice;

protected:
    friend class FGFestivalVoice;

#if defined(ENABLE_THREADS)
    class FGVoiceThread;
    FGVoiceThread *_thread;
#endif

    std::string _host;
    std::string _port;
    bool _enabled;
    SGPropertyNode_ptr _pausedNode;
    bool _paused;
    std::vector<FGVoice *> _voices;
};



#if defined(ENABLE_THREADS)
class FGVoiceMgr::FGVoiceThread : public OpenThreads::Thread
{
public:
    FGVoiceThread(FGVoiceMgr *mgr) : _mgr(mgr) {}
    void run();
    void wake_up() { _jobs.signal(); }

private:
    void wait_for_jobs() { OpenThreads::ScopedLock<OpenThreads::Mutex> g(_mutex); _jobs.wait(&_mutex); }
    OpenThreads::Condition _jobs;
    OpenThreads::Mutex _mutex;

protected:
    FGVoiceMgr *_mgr;
};
#endif


class FGVoiceMgr::FGVoice : public SGPropertyChangeListener
{
public:
  FGVoice(FGVoiceMgr * mgr ) : _mgr(mgr) {}
  virtual ~FGVoice();
  virtual void speak( const std::string & msg ) = 0;
  virtual void update(double dt) = 0;
  void pushMessage( const std::string & m);
  bool speak();

protected:
  void valueChanged(SGPropertyNode *node);
  // register as change listener on the given node, and remember it so the
  // listener can be removed again in ~FGVoice()
  void listenOnTextNode(SGPropertyNode_ptr node);

  FGVoiceMgr *_mgr;
  SGPropertyNode_ptr _textNode;

#if defined(ENABLE_THREADS)
  SGLockedQueue<std::string> _msg;
#else
  std::queue<std::string> _msg;
#endif
};
