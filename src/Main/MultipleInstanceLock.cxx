// SPDX-FileCopyrightText: 2025 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <cassert>
#include <cstdlib>

#if defined(SG_WINDOWS)
#define WIN32_LEAN_AND_MEAN // less crap :)
#include <Windows.h>
#else
#include <sys/file.h>
#include <unistd.h>
#endif

#include "MultipleInstanceLock.hxx"

#include <Main/sentryIntegration.hxx>

#include <GUI/MessageBox.hxx>

#if defined(HAVE_QT)
#include <QApplication>
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QTimer>
#endif

namespace {

#if defined(SG_WINDOWS)
static HANDLE static_fgMultipleInstanceMutex = nullptr;
static HANDLE static_fgLockReasonSharedMemory = nullptr;
#else
const std::string static_multiInstanceLockFile = "fgfs_exclusive.lock";
static int static_lockFileFd = -1;
#endif

enum LockStatus {
    LockFailed = 0,
    LockAlreadyLocked,
    LockOk
};

#if defined(SG_WINDOWS)

void windowsCleanup()
{
    if (static_fgLockReasonSharedMemory) {
        CloseHandle(static_fgLockReasonSharedMemory);
        static_fgLockReasonSharedMemory = nullptr;
    }
}

std::string windowsReadLockReason()
{
    if (!static_fgLockReasonSharedMemory) {
        static_fgLockReasonSharedMemory = OpenFileMappingA(FILE_MAP_READ, false, "org.flightgear.fgfs.reason");
        if (!static_fgLockReasonSharedMemory) {
            SG_LOG(SG_IO, SG_WARN, "Failed to open shared memory file for lock reason");
            return {};
        }

        std::atexit(windowsCleanup);
    }

    void* v = MapViewOfFile(static_fgLockReasonSharedMemory, FILE_MAP_READ, 0, 0, 0);
    if (!v) {
        SG_LOG(SG_IO, SG_ALERT, "Failed to mmap shared memory file for lock reason");
        return {};
    }

    std::string buf{reinterpret_cast<char*>(v)}; // assumes null-terminated
    SG_LOG(SG_IO, SG_INFO, "lock reason:" << buf);
    UnmapViewOfFile(v);
    return buf;
}


#endif

void writeLockReason(const std::string& s)

#if defined(SG_WINDOWS)
{
    assert(s.length() < 1024);
    if (!static_fgLockReasonSharedMemory) {
        static_fgLockReasonSharedMemory = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 1024, "org.flightgear.fgfs.reason");
        if (!static_fgLockReasonSharedMemory) {
            SG_LOG(SG_IO, SG_ALERT, "Failed to create shared memory file for lock writing");
            return;
        }

        std::atexit(windowsCleanup);
    }

    void* v = MapViewOfFile(static_fgLockReasonSharedMemory, FILE_MAP_WRITE, 0, 0, 0);
    if (!v) {
        SG_LOG(SG_IO, SG_ALERT, "Failed to mmap shared memory file for lock reason");
        return;
    }

    memcpy(v, s.c_str(), s.length());
    UnmapViewOfFile(v);
}
#else
{
    // write our reason to the lock file
    ::write(static_lockFileFd, s.data(), s.size());
}
#endif

LockStatus acquireLock()
{
#if defined(SG_WINDOWS)
    if (!static_fgMultipleInstanceMutex) {
        static_fgMultipleInstanceMutex = CreateMutexA(nullptr, FALSE, "org.flightgear.fgfs.exclusive");
        if (!static_fgMultipleInstanceMutex) {
            SG_LOG(SG_IO, SG_ALERT, "Failed to create exclusive-access mutex");
            return LockFailed;
        }

        const auto err = GetLastError();
        if (err && (err != ERROR_ALREADY_EXISTS)) {
            SG_LOG(SG_IO, SG_ALERT, "Failed to create exclusive-access mutex: " << GetLastError());
            return LockFailed;
        }
    }

    // acquire the mutex, so that other processes can check the status.
    const int result = WaitForSingleObject(static_fgMultipleInstanceMutex, 100);
    if (result == WAIT_TIMEOUT) {
        return LockAlreadyLocked;
    }

    if (result != WAIT_OBJECT_0) {
        // failed, somehow
        SG_LOG(SG_IO, SG_ALERT, "Failed to lock exclusive mutex:" << GetLastError());
        return LockFailed;
    }
#else
    SGPath lockPath(globals->get_fg_home(), static_multiInstanceLockFile);
    std::string ps = lockPath.utf8Str();
    static_lockFileFd = ::open(ps.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (static_lockFileFd < 0) {
        SG_LOG(SG_IO, SG_ALERT, "Failed to create lock file (" << lockPath << "):" << simgear::strutils::error_string(errno));
        return LockFailed;
    }
    int err = ::flock(static_lockFileFd, LOCK_EX | LOCK_NB);
    if (err < 0) {
        if (errno == EWOULDBLOCK) {
            return LockAlreadyLocked;
        }

        SG_LOG(SG_IO, SG_ALERT, "Failed to lock file (" << lockPath << "):" << simgear::strutils::error_string(errno));
        return LockFailed;
    }
#endif
    return LockOk;
}

void releaseLock(SGPath lockPath)
{
#if defined(SG_WINDOWS)
    ReleaseMutex(static_fgMultipleInstanceMutex);
    CloseHandle(static_fgMultipleInstanceMutex);
    static_fgMultipleInstanceMutex = nullptr;
    SG_LOG(SG_IO, SG_INFO, "Released lock");
#else
    int err = ::flock(static_lockFileFd, LOCK_UN);
    if (err < 0) {
        SG_LOG(SG_IO, SG_ALERT, "Failed to unlock file:" << simgear::strutils::error_string(errno));
    }

    ::close(static_lockFileFd);
    lockPath.remove();
#endif
}


std::optional<std::string> implIsLocked()
{
#if defined(SG_WINDOWS)
    if (!static_fgMultipleInstanceMutex) {
        static_fgMultipleInstanceMutex = OpenMutexA(SYNCHRONIZE, FALSE, "org.flightgear.fgfs.exclusive");
        if (!static_fgMultipleInstanceMutex) {
            // this is the common case: no other fgfs.exe has the lock
            // the mutex does not exist. Simple, we are done
            if (GetLastError() == ERROR_FILE_NOT_FOUND) {
                return {};
            }

            flightgear::fatalMessageBoxThenExit("Multiple copies of Flightgear initializing",
                                                "Unable to check if other copies of FlightGear are initializing. "
                                                "Please report this error.");
        }
    }

    // poll the named mutex
    auto result = WaitForSingleObject(static_fgMultipleInstanceMutex, 0);
    if (result == WAIT_OBJECT_0) {
        // we acquired it, release it and we're done
        // (there could be multiple read-only copies in this situation)
        ReleaseMutex(static_fgMultipleInstanceMutex);
        SG_LOG(SG_IO, SG_INFO, "isLocked: got the lock and released it");
        return {};
    }

    // failed to acquire the mutex, so assume another FGFS.exe has it locked
    return windowsReadLockReason();
#else
    SGPath lockPath(globals->get_fg_home(), static_multiInstanceLockFile);
    std::string ps = lockPath.utf8Str();
    static_lockFileFd = ::open(ps.c_str(), O_RDONLY, 0644);
    if (static_lockFileFd < 0) {
        if (errno == ENOENT) {
            return {}; // no such file, easy
        }

        SG_LOG(SG_IO, SG_ALERT, "Error opening lock file:" << simgear::strutils::error_string(errno));
        return {};
    }

    int err = ::flock(static_lockFileFd, LOCK_EX | LOCK_NB);
    if (err < 0) {
        if (errno == EWOULDBLOCK) {
            char buf[256];
            int r = ::read(static_lockFileFd, buf, 256);
            return std::string(buf, r);
        }

        SG_LOG(SG_IO, SG_ALERT, "Error querying lock file:" << simgear::strutils::error_string(errno));
        return {};
    }

    // release it again, so any *other* waiting copies can also succeed
    ::flock(static_lockFileFd, LOCK_UN);
    return {};
#endif
}

} // namespace

namespace flightgear {

static std::unique_ptr<ExclusiveInstanceLock> static_theLock;

ExclusiveInstanceLock* ExclusiveInstanceLock::createInstance(const std::string& reason)
{
    assert(static_theLock == nullptr);
    const auto r = acquireLock();
    if (r == LockFailed) {
        fatalMessageBoxThenExit("Multiple copies of Flightgear initializing",
                                "Failed to initialise locking for data files.");
    } else if (r == LockAlreadyLocked) {
        fatalMessageBoxThenExit("Multiple copies of Flightgear initializing",
                                "Multiple copies of FlightGear are trying to initialise the same data files. "
                                "This means something has gone badly wrong: please report this error.");
    }

    static_theLock.reset(new ExclusiveInstanceLock(reason));
    return static_theLock.get();
}

ExclusiveInstanceLock* ExclusiveInstanceLock::instance()
{
    return static_theLock.get();
}

void ExclusiveInstanceLock::destroyInstance()
{
    static_theLock.reset();
}

ExclusiveInstanceLock::ExclusiveInstanceLock(std::string reason)
    : _reason(reason)
{
#if !defined(SG_WINDOWS)
    _lockPath = SGPath(globals->get_fg_home(), static_multiInstanceLockFile);
#endif
    writeLockReason(_reason);
}

ExclusiveInstanceLock::~ExclusiveInstanceLock()
{
    // depending on C++ destruction order, we can't access globals here,
    // so we save the path at constructor time
    releaseLock(_lockPath);
}

std::optional<std::string> ExclusiveInstanceLock::isLocked()
{
    if (static_theLock) {
        // we are the primary copy (the one holding the exclusive lock)
        return {};
    }

    return implIsLocked();
}

void ExclusiveInstanceLock::updateReason(const std::string& reason)
{
    assert(static_theLock.get() == this);
    _reason = reason;
    writeLockReason(_reason);
}

bool ExclusiveInstanceLock::showWaitDialog()
{
#if defined(HAVE_QT)
    const char* waitForOtherMsg = QT_TRANSLATE_NOOP("initNavCache", "Another copy of FlightGear is preparing data files. Waiting for it to finish.");
    QString m = qApp->translate("initNavCache", waitForOtherMsg);

    const auto wflags = Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::MSWindowsFixedSizeDialogHint;

    addSentryBreadcrumb("showing wait for other process dialog", "info");

    QProgressDialog waitDialog(m,
                               QString() /* cancel text */,
                               0, 0, Q_NULLPTR,
                               wflags);
    waitDialog.setWindowModality(Qt::WindowModal);
    waitDialog.setMinimumWidth(600);
    waitDialog.setAutoReset(false);
    waitDialog.setAutoClose(false);
    waitDialog.show();

    QTimer updateTimer;
    updateTimer.setInterval(500);
    bool done = false;

    QObject::connect(&updateTimer, &QTimer::timeout, [&waitDialog, &done]() {
        auto lockedReason = isLocked();
        if (!lockedReason.has_value()) {
            waitDialog.done(0);
            done = true;
            return;
        }
    });

    updateTimer.start(); // timer won't actually run until we process events
    waitDialog.exec();
    updateTimer.stop();

    if (!done) {
        addSentryBreadcrumb("wait on other process abandoned by user", "info");
        return false;
    }

    addSentryBreadcrumb("done waiting for other process lock dialog", "info");
#else
    // rely on the fgIdleFunction state==1 code, to block startup until the primary copy is done
    SG_LOG(SG_GUI, SG_INFO, "ExclusiveInstanceLock::showWaitDialog: no Qt support, will let splash screen block instead.");
    return true;
#endif
    return true;
}


} // namespace flightgear
