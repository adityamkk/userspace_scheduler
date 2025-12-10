#include "scheduler.h"

namespace scheduler {

    SyncQueue<threads::TCB> tcbQueue;

    // Puts a tcb in a scheduling data structure
    void schedule(threads::TCB* tcb) {
        // TODO: schedule TCB
        tcbQueue.push(tcb);
    }

    // Gets a tcb out of the scheduling data structure
    threads::TCB* next() {
        // TODO: get TCB
        return tcbQueue.pop();
    }
};