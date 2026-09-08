// HttplibHttpd.cxx -- a http daemon subsystem based on cpp-httplib
//
// SPDX-FileCopyrightText: 2026 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include "HttplibHttpd.hxx"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "FlightHistoryUriHandler.hxx"
#include "HTTPRequest.hxx"
#include "HTTPResponse.hxx"
#include "JsonUriHandler.hxx"
#include "MirrorPropertyTreeWebsocket.hxx"
#include "NavdbUriHandler.hxx"
#include "PkgUriHandler.hxx"
#include "PropertyChangeObserver.hxx"
#include "PropertyChangeWebsocket.hxx"
#include "PropertyUriHandler.hxx"
#include "RunUriHandler.hxx"
#include "ScreenshotUriHandler.hxx"
#include "Websocket.hxx"
#include "urihandler.hxx"

#include <Main/fg_props.hxx>
#include <Main/globals.hxx>

#include <simgear/debug/logstream.hxx>
#include <simgear/misc/sg_path.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/structure/SGSharedPtr.hxx>
#include <simgear/structure/event_mgr.hxx>

#include <httplib.h>

using namespace flightgear;
using namespace flightgear::http;
using std::string;

namespace {

// Small helper: stores the configured URI handlers and finds the one
// matching a request by prefix.
class URIHandlerMap : public std::vector<SGSharedPtr<URIHandler>>
{
public:
    SGSharedPtr<URIHandler> findHandler(const std::string& uri)
    {
        for (auto& handler : *this) {
            if (0 == uri.find(handler->getUri())) return handler;
        }
        return SGSharedPtr<URIHandler>();
    }
};

// The per-connection Connection implementation for the httplib transport.
//
// One instance is shared (via shared_ptr) across a request's initial
// handleRequest() call and every subsequent poll() call from the chunked
// content-provider loop below -- handlers like ScreenshotUriHandler stash
// per-request state via Connection::put()/get() in handleRequest() and
// expect to find it again in poll(), so this must be the same object each
// time, not a fresh one per callback invocation.
//
// write() has two modes: while a chunked content-provider iteration is in
// progress, bytes are forwarded straight to httplib's DataSink (set via
// beginPollIteration() before each poll() call). Otherwise they're
// buffered locally -- no current handler does this outside of poll(), but
// it keeps the Connection contract meaningful if one ever does.
class HttplibConnection : public Connection
{
public:
    void write(const char* data, size_t len) override
    {
        _bytesWrittenThisCall += len;
        if (_sink) {
            if (!_sink->write(data, len)) _sinkWriteOk = false;
            return;
        }
        _buffered.append(data, len);
    }

    // Call before each poll() iteration: points write() at this callback's
    // DataSink and resets the "did poll() write anything?" counter.
    void beginPollIteration(httplib::DataSink* sink)
    {
        _sink = sink;
        _bytesWrittenThisCall = 0;
    }

    bool sinkWriteOk() const { return _sinkWriteOk; }
    size_t bytesWrittenThisCall() const { return _bytesWrittenThisCall; }

private:
    httplib::DataSink* _sink = nullptr;
    bool _sinkWriteOk = true;
    size_t _bytesWrittenThisCall = 0;
    std::string _buffered;
};

// Adapts flightgear::http::WebsocketWriter onto a live httplib::ws::WebSocket.
// send()/close() map directly. Ping/pong opcodes are dropped rather than
// forwarded -- httplib's ws::WebSocket runs its own periodic ping/pong
// heartbeat internally
class HttplibWebsocketWriter : public WebsocketWriter
{
public:
    explicit HttplibWebsocketWriter(httplib::ws::WebSocket& ws)
        : _ws(ws)
    {
    }

    int writeToWebsocket(int opcode, const char* data, size_t len) override
    {
        bool ok;
        switch (opcode) {
        case 1: // text
            ok = _ws.send(std::string(data, len));
            break;
        case 2: // binary
            ok = _ws.send(data, len);
            break;
        case 8: // connection close
            _ws.close();
            ok = true;
            break;
        case 9:  // ping
        case 10: // pong
            // handled internally by httplib's own heartbeat; nothing to do.
            ok = true;
            break;
        default:
            ok = false;
            break;
        }
        return ok ? static_cast<int>(len) : -1;
    }

private:
    httplib::ws::WebSocket& _ws;
};

HTTPRequest buildRequest(const httplib::Request& req)
{
    HTTPRequest request;
    request.Method = req.method;
    request.Uri = req.path;
    request.HttpVersion = req.version;
    request.remoteAddress = req.remote_addr;
    request.remotePort = req.remote_port;
    request.localAddress = req.local_addr;
    request.localPort = req.local_port;
    request.Content = req.body;

    for (const auto& kv : req.params) {
        request.RequestVariables[kv.first] = kv.second;
    }
    for (const auto& kv : req.headers) {
        request.HeaderVariables[kv.first] = kv.second;
    }
    return request;
}

} // namespace

class HttplibHttpd::HttplibHttpdPrivate
{
public:
    explicit HttplibHttpdPrivate(SGPropertyNode_ptr configNode)
        : configNode(configNode)
    {
    }

    ~HttplibHttpdPrivate()
    {
        stopListening();
    }

    void stopListening()
    {
        if (listenThread.joinable()) {
            server.stop();
            listenThread.join();
        }
    }

    void runOnMainThread(const std::function<void()>& fn)
    {
        std::mutex m;
        std::condition_variable cv;
        bool done = false;

        globals->get_event_mgr()->addEvent(
            "httpd-main-thread-hop",
            [&]() {
                fn();
                {
                    std::lock_guard<std::mutex> lock(m);
                    done = true;
                }
                cv.notify_one();
            },
            0.0, /* delay */ true /* sim time */);

        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&] { return done; });
    }

    // Dispatches one request to whichever URIHandler matches, or
    // fallback to static files
    httplib::Server::HandlerResponse handleRequest(const httplib::Request& req, httplib::Response& res)
    {
        SGSharedPtr<URIHandler> handler = uriHandler.findHandler(req.path);
        if (!handler.valid()) return httplib::Server::HandlerResponse::Unhandled;

        HTTPRequest request = buildRequest(req);
        HTTPResponse response;
        response.Header["Server"] = string("FlightGear/") + FLIGHTGEAR_VERSION + " cpp-httplib/" CPPHTTPLIB_VERSION;
        response.Header["Cache-Control"] = "no-cache";
        {
            char buf[64];
            time_t now = time(nullptr);
            strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
            response.Header["Date"] = buf;
        }

        auto connection = std::make_shared<HttplibConnection>();

        // Two handlers get a one-shot hop to the main thread rather than
        // calling straight into the property tree like everything else here:
        // - ScreenshotUriHandler's first call installs an osg::Camera::DrawCallback
        // - RunUriHandler executes arbitrary named commands via SGCommandMgr::execute()
        bool done = false;
        if (dynamic_cast<ScreenshotUriHandler*>(handler.get()) || dynamic_cast<RunUriHandler*>(handler.get())) {
            runOnMainThread([&] { done = handler->handleRequest(request, response, connection.get()); });
        } else {
            done = handler->handleRequest(request, response, connection.get());
        }

        if (done) {
            res.status = response.StatusCode;
            for (const auto& kv : response.Header) {
                if (kv.first.empty() || kv.second.empty()) continue;
                res.set_header(kv.first, kv.second);
            }
            res.set_content(response.Content, response.Header.count("Content-Type")
                                                  ? response.Header.at("Content-Type")
                                                  : "text/plain");
            return httplib::Server::HandlerResponse::Handled;
        }

        // Request not done: this is the poll()-needed path. In practice only
        // ScreenshotUriHandler ever returns false here with a working
        // continuation
        res.status = response.StatusCode;
        string contentType = response.Header.count("Content-Type") ? response.Header.at("Content-Type") : "";
        for (const auto& kv : response.Header) {
            // Content-Type goes to set_chunked_content_provider() below instead:
            // unlike set_content(), it emplace()s rather than replacing, so
            // setting it here too would leave a duplicate, empty header.
            if (kv.first.empty() || kv.second.empty() || kv.first == "Content-Type") continue;
            res.set_header(kv.first, kv.second);
        }
        SGSharedPtr<URIHandler> handlerRef = handler; // captured by value below
        res.set_chunked_content_provider(
            contentType,
            [handlerRef, connection](size_t /*offset*/, httplib::DataSink& sink) -> bool {
                connection->beginPollIteration(&sink);
                bool pollDone = handlerRef->poll(connection.get());
                if (!connection->sinkWriteOk()) return false; // client disconnected
                if (pollDone) return false;                   // finished, stop the stream
                if (connection->bytesWrittenThisCall() == 0) {
                    // Nothing to send yet -- avoid busy-waiting the worker thread.
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                }
                return true; // call me again
            });
        return httplib::Server::HandlerResponse::Handled;
    }

    // Runs one WebSocket connection's whole lifetime. httplib gives each
    // WebSocket connection its own dedicated, blocking thread;
    // that thread here becomes
    // the inbound-message read loop. A second, connection-local thread
    // periodically calls poll()
    void runWebsocket(std::unique_ptr<Websocket> websocket, httplib::ws::WebSocket& ws)
    {
        HttplibWebsocketWriter writer(ws);
        std::mutex callMutex;
        std::atomic<bool> stop{false};

        std::thread pollThread([&] {
            while (!stop.load()) {
                {
                    std::lock_guard<std::mutex> lock(callMutex);
                    if (ws.is_open()) websocket->poll(writer);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });

        std::string msg;
        while (true) {
            httplib::ws::ReadResult result = ws.read(msg);
            if (result == httplib::ws::ReadResult::Fail) break;

            HTTPRequest request;
            request.Content = msg;
            std::lock_guard<std::mutex> lock(callMutex);
            websocket->handleRequest(request, writer);
        }

        stop = true;
        pollThread.join();
        websocket->close();
    }

    SGPropertyNode_ptr configNode;
    httplib::Server server;
    std::thread listenThread;
    URIHandlerMap uriHandler;
};

HttplibHttpd::HttplibHttpd(SGPropertyNode_ptr configNode)
    : d(std::make_unique<HttplibHttpdPrivate>(configNode))
{
}

HttplibHttpd::~HttplibHttpd() = default;

void HttplibHttpd::bind()
{
    SGPropertyNode_ptr configNode = d->configNode;
    SGPropertyNode_ptr n = configNode->getNode("uri-handler");
    if (n.valid()) {
        string uri;
        if (!(uri = n->getStringValue("screenshot")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding screenshot uri handler at " << uri);
            d->uriHandler.push_back(new ScreenshotUriHandler(uri));
        }
        if (!(uri = n->getStringValue("property")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding property uri handler at " << uri);
            d->uriHandler.push_back(new PropertyUriHandler(uri));
        }
        if (!(uri = n->getStringValue("json")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding json uri handler at " << uri);
            d->uriHandler.push_back(new JsonUriHandler(uri));
        }
        if (!(uri = n->getStringValue("pkg")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding pkg uri handler at " << uri);
            d->uriHandler.push_back(new PkgUriHandler(uri));
        }
        if (!(uri = n->getStringValue("flighthistory")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding flighthistory uri handler at " << uri);
            d->uriHandler.push_back(new FlightHistoryUriHandler(uri));
        }
        if (!(uri = n->getStringValue("run")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding run uri handler at " << uri);
            d->uriHandler.push_back(new RunUriHandler(uri));
        }
        if (!(uri = n->getStringValue("navdb")).empty()) {
            SG_LOG(SG_NETWORK, SG_INFO, "httpd: adding navdb uri handler at " << uri);
            d->uriHandler.push_back(new NavdbUriHandler(uri));
        }
    }

    httplib::Server& server = d->server;

    server.set_pre_routing_handler([this](const httplib::Request& req, httplib::Response& res) {
        return d->handleRequest(req, res);
    });

    server.WebSocket("/PropertyListener", [this](const httplib::Request&, httplib::ws::WebSocket& ws) {
        SG_LOG(SG_NETWORK, SG_INFO, "new PropertyChangeWebsocket");
        PropertyChangeObserver observer;
        d->runWebsocket(std::make_unique<PropertyChangeWebsocket>(&observer), ws);
    });
    server.WebSocket("/PropertyTreeMirror/(.*)", [this](const httplib::Request& req, httplib::ws::WebSocket& ws) {
        const string path = req.matches[1].str();
        SG_LOG(SG_NETWORK, SG_INFO, "new MirrorPropertyTreeWebsocket for: " << path);
        d->runWebsocket(std::make_unique<MirrorPropertyTreeWebsocket>(path), ws);
    });

    n = configNode->getNode("options");
    int listeningPort = 8080;
    if (n.valid()) {
        const string fgRoot = fgGetString("/sim/fg-root");
        string docRoot = n->getStringValue("document-root", fgRoot);
        if (docRoot.empty() || docRoot[0] != '/') docRoot.insert(0, "/").insert(0, fgRoot);
        server.set_mount_point("/", docRoot);

        listeningPort = n->getIntValue("listening-port", listeningPort);

        // URL rewrites map directly onto httplib's mount points (a URL prefix
        // served from a given directory)
        {
            string rewrites = n->getStringValue("url-rewrites", "");
            string_list rwl = simgear::strutils::split(rewrites, ",");
            rwl.push_back(string("/aircraft-dir/=") + fgGetString("/sim/aircraft-dir") + "/");
            rwl.push_back(string("/fg-home/=") + fgGetString("/sim/fg-home") + "/");
            rwl.push_back(string("/fg-root/=") + fgGetString("/sim/fg-root") + "/");
            for (const auto& entry : rwl) {
                string_list rw = simgear::strutils::split(entry, "=");
                if (rw.size() != 2) {
                    SG_LOG(SG_NETWORK, SG_WARN, "invalid entry '" << entry << "' in url-rewrites ignored.");
                    continue;
                }
                string mountPoint = rw[0];
                SGPath targetPath(rw[1]);
                string dir = targetPath.isAbsolute() ? rw[1] : fgRoot + "/" + rw[1];
                if (!server.set_mount_point(mountPoint, dir)) {
                    SG_LOG(SG_NETWORK, SG_WARN, "httpd: failed to mount '" << mountPoint << "' -> '" << dir << "'");
                }
            }
        }

        if (n->getStringValue("enable-directory-listing", "yes") == "yes") {
            SG_LOG(SG_NETWORK, SG_WARN,
                   "httpd: enable-directory-listing has no effect with the cpp-httplib backend ");
        }

        string indexFiles = n->getStringValue("index-files", "index.html");
        if (indexFiles != "index.html") {
            SG_LOG(SG_NETWORK, SG_WARN,
                   "httpd: index-files='" << indexFiles
                                          << "' ignored -- cpp-httplib backend always serves index.html");
        }

        long idleTimeoutMs = n->getLongValue("idle-timeout-ms", 30000);
        server.set_keep_alive_timeout(std::chrono::milliseconds(idleTimeoutMs));
        server.set_read_timeout(std::chrono::milliseconds(idleTimeoutMs));
        server.set_write_timeout(std::chrono::milliseconds(idleTimeoutMs));

        string extraMimeTypes = n->getStringValue("extra-mime-types", "");
        for (const auto& entry : simgear::strutils::split(extraMimeTypes, ",")) {
            string_list kv = simgear::strutils::split(entry, "=");
            if (kv.size() != 2) continue;
            server.set_file_extension_and_mimetype_mapping(kv[0], kv[1]);
        }

        string accessLogFile = n->getStringValue("access-log-file", "");
        if (!accessLogFile.empty()) {
            auto log = std::make_shared<std::ofstream>(accessLogFile, std::ios::app);
            server.set_logger([log](const httplib::Request& req, const httplib::Response& res) {
                char buf[64];
                time_t now = time(nullptr);
                strftime(buf, sizeof(buf), "%d/%b/%Y:%H:%M:%S %z", localtime(&now));
                (*log) << req.remote_addr << " - - [" << buf << "] \"" << req.method << " " << req.path << " "
                       << req.version << "\" " << res.status << " " << res.body.size() << "\n";
                log->flush();
            });
        }
    }

    if (!server.bind_to_port("0.0.0.0", listeningPort)) {
        SG_LOG(SG_NETWORK, SG_ALERT, "httpd: failed to bind to port " << listeningPort);
        return;
    }
    d->listenThread = std::thread([this] { d->server.listen_after_bind(); });

    configNode->setBoolValue("running", true);
    SG_LOG(SG_NETWORK, SG_INFO, "httpd: cpp-httplib backend listening on port " << listeningPort);
}

void HttplibHttpd::unbind()
{
    d->configNode->setBoolValue("running", false);
    d->stopListening();
    d->uriHandler.clear();
}

void HttplibHttpd::update(double)
{
}
