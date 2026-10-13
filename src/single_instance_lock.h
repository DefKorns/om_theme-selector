/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#ifndef SINGLE_INSTANCE_LOCK_H_
#define SINGLE_INSTANCE_LOCK_H_

// Keeps one theme_manager on /dev/fb0, taking over a stale instance instead of rejecting the new launch.
class SingleInstanceLock
{
public:
    SingleInstanceLock(const SingleInstanceLock &) = delete;
    SingleInstanceLock & operator=(const SingleInstanceLock &) = delete;

    static void Acquire();

private:
    static constexpr const char * LockPath = "/tmp/theme_manager.lock";

    SingleInstanceLock();
    ~SingleInstanceLock();

    static void TakeOverStaleInstance();
    static void HandleSigTerm(int);
};

#endif
