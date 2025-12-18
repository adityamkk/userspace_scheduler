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
        for(size_t i = 0; i < sizeof(K); i++) {
            rolling_hash += ((uint8_t*)(&key))[i] * 33 + 1899879;
        }
        return rolling_hash;
    }
public:
    HashMap(uint32_t buckets) : num_buckets(buckets) {
        this->buckets = new SharedPtr<HashNode<K, V>>[buckets];
    }

    void put(K key, V value) {
        uint32_t index = hash(key) % num_buckets;\
        SharedPtr<HashNode<K, V>> head = this->buckets[index];
        this->buckets[index] = SharedPtr<HashNode<K, V>>(new HashNode<K, V>(key, value));
        this->buckets[index]->next = head;
    }

    V get(K key) {
        uint32_t index = hash(key) % num_buckets;
        SharedPtr<HashNode<K, V>> curr = this->buckets[index];
        while (curr != nullptr) {
            if (curr->key == key) {
                return curr->value;
            }
            curr = curr->next;
        }
        PANIC("No element found in hashmap that matches the provided key\n");
        return *(new V());
    }

    bool remove(K key) {
        uint32_t index = hash(key) % num_buckets;
        SharedPtr<HashNode<K, V>> curr = this->buckets[index];
        SharedPtr<HashNode<K, V>> prev(nullptr);
        while (curr != nullptr) {
            if (curr->key == key) {
                if (prev == nullptr) {
                    this->buckets[index] = curr->next;
                } else {
                    prev->next = curr->next;
                }
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }
};