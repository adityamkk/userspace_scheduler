#pragma once

#include "threads.h"
#include "sync_queue.h"

namespace scheduler {
    extern void schedule(threads::TCB* tcb);
    extern threads::TCB* next();
};