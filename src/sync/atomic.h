#pragma once

// Atomic class made by Copilot
template <typename T>
class Atomic {
    T value;

public:
    Atomic(T value = 0) : value(value) {}

    // Load value atomically
    T get() const {
        T result;
        if constexpr (sizeof(T) == 4) {
            // For 32-bit types, use lr.w (load-reserved word) to ensure atomicity
            __asm__ volatile(
                "lr.w %0, (%1)\n"
                : "=r"(result)
                : "r"(&value)
                : "memory");
        } else {
            // For other types, fall back to volatile read
            result = value;
        }
        return result;
    }

    // Store value atomically
    void set(T newval) {
        if constexpr (sizeof(T) == 4) {
            // For 32-bit types, use amoswap.w (atomic swap word) to store atomically
            __asm__ volatile(
                "amoswap.w zero, %0, (%1)\n"
                :
                : "r"(newval), "r"(&value)
                : "memory");
        } else {
            // For other types, fall back to volatile write
            value = newval;
        }
    }

    // Fetch-add: atomically add to value and return old value
    T fetch_add(T delta = 1) {
        T old_value;
        if constexpr (sizeof(T) == 4) {
            // For 32-bit types, use amoadd.w (atomic add word)
            __asm__ volatile(
                "amoadd.w %0, %1, (%2)\n"
                : "=r"(old_value)
                : "r"(delta), "r"(&value)
                : "memory");
        } else {
            // For other types, fall back to non-atomic (but this is a limitation)
            old_value = value;
            value += delta;
        }
        return old_value;
    }

    // Add-fetch: atomically add to value and return new value
    T add_fetch(T delta = 1) {
        if constexpr (sizeof(T) == 4) {
            // For 32-bit types, use amoadd.w and calculate new value
            T new_value;
            __asm__ volatile(
                "amoadd.w %0, %1, (%2)\n"
                : "=r"(new_value)
                : "r"(delta), "r"(&value)
                : "memory");
            // amoadd.w returns the old value, so add delta to get the new value
            return new_value + delta;
        } else {
            // For other types, fall back to non-atomic
            return (value += delta);
        }
    }

    // Compare-and-swap: atomically compare and swap if equal
    // Returns true if the swap was successful, false otherwise
    bool compare_and_swap(T expected, T newval) {
        if constexpr (sizeof(T) == 4) {
            // Use lr.w/sc.w for compare-and-swap
            T result;
            __asm__ volatile(
                "0: lr.w %0, (%1)\n"
                "   bne %0, %2, 1f\n"     // if value != expected, skip store
                "   sc.w %0, %3, (%1)\n"  // try to store newval
                "   bnez %0, 0b\n"        // if sc failed, retry
                "   li %0, 1\n"           // success: result = 1
                "   j 2f\n"
                "1: li %0, 0\n"           // failure: result = 0
                "2:\n"
                : "=&r"(result)
                : "r"(&value), "r"(expected), "r"(newval)
                : "memory");
            return result;
        } else {
            // For other types, fall back to non-atomic CAS
            if (value == expected) {
                value = newval;
                return true;
            }
            return false;
        }
    }
};