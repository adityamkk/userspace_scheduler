#pragma once

#include "common.h"
#include "../sync/shared.h"

template <typename K, typename V>
class HashNode {
public:
    K key;
    V value;
    SharedPtr<HashNode<K, V>> next;

    HashNode(K key, V value) {
        this->key = key;
        this->value = value;
        this->next = nullptr;
    }
};

template <typename K, typename V>
class HashMap {
    uint32_t num_buckets;
    SharedPtr<HashNode<K, V>>* buckets;
    uint32_t hash(K key) {
        uint32_t rolling_hash = 0;
        for(int i = 0; i < sizeof(K); i++) {
            rolling_hash += ((uint8_t*)(&key))[i] * 33 + 1899879;
        }
        return rolling_hash;
    }
public:
    HashMap(uint32_t buckets) : num_buckets(buckets) {
        buckets = new SharedPtr<HashNode<K, V>>[buckets];
    }

    void put(K key, V value) {
        uint32_t index = hash(key) % num_buckets;
        SharedPtr<HashNode<K, V>> head = buckets[index];
        buckets[index] = SharedPtr<HashNode<K, V>>(new HashNode<K, V>(key, value));
        buckets[index]->next = head;
    }

    V get(K key) {
        uint32_t index = hash(key) % num_buckets;
        SharedPtr<HashNode<K, V>> curr = buckets[index];
        while (curr != nullptr) {
            if (curr->key == key) {
                return curr->value;
            }
            curr = curr->next;
        }
    }
};