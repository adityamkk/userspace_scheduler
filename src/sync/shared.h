#pragma once

#include "../common/common.h"
#include "atomic.h"

// Intermediate Control Block
template <typename T>
class ICB {
public:
    T* value;
    Atomic<uint32_t> shared_count;
    Atomic<uint32_t> total_count;
    ICB(T* ptr) : value(ptr), shared_count(1), total_count(1) {}
};

template <typename T>
class SharedPtr;
template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
    ICB<T>* icb;
public:
    SharedPtr(T* ptr) {
        if (ptr == nullptr) {
            icb = nullptr;
        } else {
            icb = new ICB<T>(ptr);
        }
    }

    ~SharedPtr() {
        if (icb != nullptr) {
            uint32_t scount = icb->shared_count.get() != 0 ? icb->shared_count.add_fetch(-1) : 0; // TODO: WRONG
            uint32_t tcount = icb->total_count.get() != 0 ? icb->total_count.add_fetch(-1) : 0; // TODO: WRONG
            if (scount == 0) {
                T* val = icb->value;
                icb->value = nullptr;
                delete val;
            }
            if (tcount == 0) {
                ICB<T>* my_icb = icb;
                icb = nullptr;
                delete my_icb;
            }
        }
    }

    SharedPtr(const SharedPtr& other) {
        if (other.icb == nullptr) {
            this->icb = nullptr;
        } else {
            other.icb->shared_count.fetch_add(1);
            other.icb->total_count.fetch_add(1);
            this->icb = other.icb;
        }
    }

    SharedPtr& operator=(const SharedPtr& other) {
        if (this == &other) {
            return this;
        }
        this->~SharedPtr();
        if (other.icb == nullptr) {
            this->icb = nullptr;
        } else {
            other.icb->shared_count.fetch_add(1);
            other.icb->total_count.fetch_add(1);
            this->icb = other->icb;
        }
    }

    T* operator->() {
        if (this->icb == nullptr) {
            PANIC("Tried to dereference a nullptr\n");
        }
        return this->icb->value;
    }

    const T* operator->() const {
        if (this->icb == nullptr) {
            PANIC("Tried to dereference a nullptr\n");
        }
        return this->icb->value;
    }

    bool operator==(const SharedPtr& other) {
        if (this->icb == nullptr && other->icb == nullptr) {
            return true;
        }
        if (this->icb == nullptr && other->icb != nullptr) {
            return false;
        }
        if (this->icb != nullptr && other->icb == nullptr) {
            return false;
        }
        return this->icb->value == other->icb->value;
    }

    const bool operator==(const SharedPtr& other) const {
        if (this->icb == nullptr && other->icb == nullptr) {
            return true;
        }
        if (this->icb == nullptr && other->icb != nullptr) {
            return false;
        }
        if (this->icb != nullptr && other->icb == nullptr) {
            return false;
        }
        return this->icb->value == other->icb->value;
    }

    WeakPtr<T> demote() {
        this->icb->total_count.fetch_add(1);
        return WeakPtr<T>(this->icb);
    }

    friend class WeakPtr<T>;
};

template <typename T>
class WeakPtr {
    ICB<T>* icb;
    WeakPtr(ICB<T>* ptr) {
        icb = ptr;
    }
public:
    ~WeakPtr() {
        if (icb != nullptr) {
            uint32_t tcount = icb->total_count.get() != 0 ? icb->total_count.add_fetch(-1) : 0; // TODO: Wrong
            if (tcount == 0) {
                ICB<T>* my_icb = icb;
                icb = nullptr;
                delete my_icb;
            }
        }
    }

    WeakPtr(const WeakPtr& other) {
        if (other == nullptr || other->icb == nullptr) {
            this->icb = nullptr;
        } else {
            other->icb->total_count.fetch_add(1);
            this->icb = other->icb;
        }
    }

    SharedPtr<T> promote() {
        // Atomically: if shared count is not 0, increment it
        if (icb == nullptr) {
            return SharedPtr<T>(nullptr);
        }
        uint32_t scount = 0;
        while (true) {
            scount = icb->shared_count.get();
            if (scount == 0) {
                // Failure
                return SharedPtr<T>(nullptr);
            }
            // Attempt to increment this counter via CAS
            if (icb->shared_count.compare_and_swap(scount, scount + 1)) {
                // It worked!
                SharedPtr<T> ptr = SharedPtr<T>(nullptr);
                ptr.icb = icb;
                return ptr;
            }
        }
    }

    friend class SharedPtr<T>;
};