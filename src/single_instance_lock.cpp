/**
  * Copyright (c) 2026 DefKorns (https://defkorns.github.io/LICENSE)
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  */

#include "single_instance_lock.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

void SingleInstanceLock::Acquire()
{
    static SingleInstanceLock instance;
    (void)instance;
}

SingleInstanceLock::SingleInstanceLock()
{
    TakeOverStaleInstance();
    std::ofstream(LockPath, std::ios::trunc) << getpid();
    std::signal(SIGTERM, &SingleInstanceLock::HandleSigTerm);
}

SingleInstanceLock::~SingleInstanceLock() { std::remove(LockPath); }

void SingleInstanceLock::TakeOverStaleInstance()
{
    std::ifstream in(LockPath);
    pid_t oldPid = 0;
    if(in.good())
        in >> oldPid;
    in.close();
    if(oldPid <= 0 || oldPid == getpid() || kill(oldPid, 0) != 0)
        return;
    kill(oldPid, SIGTERM);
    for(int i = 0; i < 40 && kill(oldPid, 0) == 0; ++i)
        usleep(50000);
    if(kill(oldPid, 0) == 0)
        kill(oldPid, SIGKILL);
}

void SingleInstanceLock::HandleSigTerm(int) { std::exit(0); }
