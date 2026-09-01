#include "single_instance.hpp"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace fs = std::filesystem;

SingleInstanceLock::SingleInstanceLock(const fs::path& lockAbs) {
    const int fd{open(lockAbs.c_str(),
                      O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW,
                      S_IRUSR | S_IWUSR)};
    if (fd == -1) {
        throw std::system_error{errno, std::generic_category(),
                                "failed to open instance lock"};
    }

    struct stat lockInfo{};
    if (fstat(fd, &lockInfo) == -1) {
        const int error{errno};
        close(fd);
        throw std::system_error{error, std::generic_category(),
                                "failed to inspect instance lock"};
    }
    if (!S_ISREG(lockInfo.st_mode) || lockInfo.st_uid != geteuid()) {
        close(fd);
        throw std::system_error{EPERM, std::generic_category(),
                                "instance lock is not a regular file owned by "
                                "the current user"};
    }

    while (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        const int error{errno};
        if (error == EINTR) {
            continue;
        }
        close(fd);
        if (error == EWOULDBLOCK) {
            return;
        }
        throw std::system_error{error, std::generic_category(),
                                "failed to acquire instance lock"};
    }

    lockFd = fd;
}

SingleInstanceLock::~SingleInstanceLock() {
    if (lockFd != -1) {
        close(lockFd);
    }
}

bool SingleInstanceLock::isFirstInstance() const noexcept {
    return lockFd != -1;
}

fs::path getInstanceLockPath() {
    const char* const xdgRuntimeDir{std::getenv("XDG_RUNTIME_DIR")};
    if (xdgRuntimeDir != nullptr) {
        const fs::path runtimeAbs{xdgRuntimeDir};
        if (runtimeAbs.is_absolute()) {
            return runtimeAbs / "epubworm.lock";
        }
    }

    return fs::temp_directory_path()
           / ("epubworm-" + std::to_string(geteuid()) + ".lock");
}
