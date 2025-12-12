#pragma once

#include "../common/common.h"
#include "../common/hashmap.h"
#include "mutex.h"

template <typename K, typename V>
class SyncMap {
    HashMap<K, V> map;
    Mutex mapLock;

    SyncMap(uint32_t num_buckets) : map(num_buckets), mapLock() {}

    void put(K key, V value) {
        mapLock.lock();
        map.put(key, value);
        mapLock.unlock();
    }

    V get(K key) {
        mapLock.lock();
        V value = map.get(key);
        mapLock.unlock();
        return value;
    }
};