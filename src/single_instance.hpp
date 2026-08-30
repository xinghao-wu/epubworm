#pragma once

#include <filesystem>

class SingleInstanceLock {
public:
    // Opens `lockAbs` and attempts to acquire an exclusive, non-blocking lock.
    // Construction succeeds when another process owns the lock, but
    // `isFirstInstance()` will return false. Throws `std::system_error` when
    // the lock file cannot be safely opened, inspected, or locked.
    explicit SingleInstanceLock(const std::filesystem::path& lockAbs);

    // Releases an acquired lock by closing its file descriptor.
    ~SingleInstanceLock();

    SingleInstanceLock(const SingleInstanceLock&) = delete;
    SingleInstanceLock& operator=(const SingleInstanceLock&) = delete;
    SingleInstanceLock(SingleInstanceLock&&) = delete;
    SingleInstanceLock& operator=(SingleInstanceLock&&) = delete;

    // Returns true when this object acquired the lock, meaning no earlier
    // instance currently owns it.
    [[nodiscard]] bool isFirstInstance() const noexcept;

private:
    int lockFd{-1};
};

// Returns a per-user lock path, preferring the XDG runtime directory.
[[nodiscard]] std::filesystem::path getInstanceLockPath();
