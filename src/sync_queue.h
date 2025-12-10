#pragma once

#include "common.h"
#include "spinlock.h"

template <typename T>
struct SyncQueueData {
    T* data;
    SyncQueueData* next;

    SyncQueueData(T* ptr, SyncQueueData* next) {
        this->data = ptr;
        this->next = next;
    }
};

template <typename T>
class SyncQueue {
private:
    SyncQueueData<T>* head;
    SyncQueueData<T>* tail;
    Spinlock qlock;
public:
    SyncQueue() : head(nullptr), tail(nullptr), qlock() {}

    void push(T* ptr) {
        qlock.lock();
        if (this->tail == nullptr) {
            head = new SyncQueueData<T>(ptr, nullptr);
            tail = head;
        } else {
            tail->next = new SyncQueueData<T>(ptr, nullptr);
            tail = tail->next;
        }
        qlock.unlock();
    }

    T* pop() {
        T* val = nullptr;
        qlock.lock();
        if (this->head != nullptr) {
            val = head->data;
            head = head->next;
            if (head == nullptr) {
                tail = nullptr;
            }
        }
        qlock.unlock();
        return val;
    }
};