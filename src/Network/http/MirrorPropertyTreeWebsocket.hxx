// MirrorPropertyTreeWebsocket.hxx -- A websocket for mirroring a property sub-tree
//
// Written by James Turner, started November 2016.
//
// SPDX-FileCopyrightText: 2016 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Websocket.hxx"

#include <simgear/props/props.hxx>
#include <simgear/timing/timestamp.hxx>

#include <vector>
#include <memory>

namespace flightgear {
namespace http {

    class MirrorTreeListener;

class MirrorPropertyTreeWebsocket : public Websocket
{
public:
    MirrorPropertyTreeWebsocket(const std::string& path);
    ~MirrorPropertyTreeWebsocket() override;

    void close() override;
    void handleRequest(const HTTPRequest & request, WebsocketWriter & writer) override;
    void poll(WebsocketWriter & writer) override;

private:
    void checkNodeExists();

    friend class MirrorTreeListener;

    std::string _rootPath;
    SGPropertyNode_ptr _subtreeRoot;
    std::unique_ptr<MirrorTreeListener> _listener;
    int _minSendInterval;
    SGTimeStamp _lastSendTime;
};

}
} // namespace flightgear
