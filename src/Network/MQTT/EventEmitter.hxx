/*
 * SPDX-FileName: EventEmitter.hxx
 * SPDX-FileComment: Event emitter implementation with listener registration and event dispatch capabilities
 * SPDX-FileCopyrightText: Copyright (C) 2025 Torsten Dreyer - torsten (at) t3r (dot) de
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class EventEmitter
{
public:
    using ListenerID = uint64_t;
    using Listener = std::function<void()>;

    ListenerID on(const std::string& eventName, Listener listener)
    {
        ListenerID id = nextId++;
        listeners[eventName].emplace_back(id, std::move(listener));
        return id;
    }

    void off(const std::string& eventName, ListenerID id)
    {
        auto it = listeners.find(eventName);
        if (it != listeners.end()) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                     [id](const auto& pair) { return pair.first == id; }),
                      vec.end());
        }
    }

    void emit(const std::string& eventName)
    {
        auto it = listeners.find(eventName);
        if (it != listeners.end()) {
            for (const auto& pair : it->second) {
                pair.second();
            }
        }
    }

private:
    ListenerID nextId = 1;
    std::unordered_map<std::string, std::vector<std::pair<ListenerID, Listener>>> listeners;
};