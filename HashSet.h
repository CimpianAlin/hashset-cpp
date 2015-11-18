// You should probably not be using this.  This is only useful in environments
// without std lib and having specific serialization and memory requirements.
// Instead consider using `hash_set` which is a more generic implementation with templates.

#pragma once

#include <stdint.h>
#include <math.h>
#include "hashItem.h"
#include <string.h>
#include <stdio.h>

template<class T>
class HashSet {
public:
  typedef uint64_t (*HashSetFnPtr)(const T *hashItem);

  HashSet(uint32_t bucketCount)
    : bucketCount(bucketCount), buckets(nullptr), _size(0) {
    if (bucketCount != 0) {
      buckets = new HashItem<T>*[bucketCount];
      memset(buckets, 0, sizeof(HashItem<T>*) * bucketCount);
    }
  }

  ~HashSet() {
    cleanup();
  }

  /**
   * Adds the specified data if it doesn't exist
   * A copy of the data will be created, so the memory used to do the add doesn't need to stick around.
   *
   * @param newHashItem The node to add
   * @return true if the data was added
   */
  bool add(const T &itemToAdd) {
    uint64_t hash = itemToAdd.hash();
    HashItem<T> *hashItem = buckets[hash % bucketCount];
    if (!hashItem) {
      hashItem = new HashItem<T>();
      hashItem->hashItemStorage = new T(itemToAdd);
      buckets[hash % bucketCount] = hashItem;
      _size++;
      return true;
    }

    while (true) {
      if (*hashItem->hashItemStorage == itemToAdd) {
        return false;
      }
      if (!hashItem->next) {
        HashItem<T> *createdHashItem = new HashItem<T>();
        createdHashItem->hashItemStorage = new T(itemToAdd);
        hashItem->next = createdHashItem;
        break;
      }
      hashItem = hashItem->next;
    }

    _size++;
    return true;
  }

  /**
   * Determines if the specified data exists in the set or not`
   * @param data The binary data to check
   * @param len The length of the binary data to acheck
   * @return true if the data found
   */
  bool exists(const T &dataToCheck) {
    uint64_t hash = dataToCheck.hash();
    HashItem<T> *hashItem = buckets[hash % bucketCount];
    if (!hashItem) {
      return false;
    }

    while (hashItem) {
      if (*hashItem->hashItemStorage == dataToCheck) {
        return true;
      }
      hashItem = hashItem->next;
    }

    return false;
  }

  /**
   * Finds the specific data in the hash set.
   * This is useful because sometimes it contains more context
   * than the object used for the lookup.
   * @param data The binary data to check
   * @param len The length of the binary data to acheck
   * @return The data stored in the hash set or nullptr if none is found.
   */
  T * find(const T &dataToCheck) {
    uint64_t hash = dataToCheck.hash();
    HashItem<T> *hashItem = buckets[hash % bucketCount];
    if (!hashItem) {
      return nullptr;
    }

    while (hashItem) {
      if (*hashItem->hashItemStorage == dataToCheck) {
        return hashItem->hashItemStorage;
      }
      hashItem = hashItem->next;
    }

    return nullptr;
  }


  uint32_t size() {
    return _size;
  }

  /**
   * Serializes the parsed data and bloom filter data into a single buffer.
   * @param size The size is returned in the out parameter if it's needed to write to a file.
   * @return The returned buffer should be deleted by the caller.
   */
  char * serialize(uint32_t &size) {
     size = 0;
     size += serializeBuckets(nullptr);
     char *buffer = new char[size];
     memset(buffer, 0, size);
     serializeBuckets(buffer);
     return buffer;
  }

  /**
   * Deserializes the buffer.
   * Memory passed in will be used by this instance directly without copying it in.
   */
  bool deserialize(char *buffer, uint32_t bufferSize) {
    cleanup();
    uint32_t pos = 0;
    if (!hasNewlineBefore(buffer, bufferSize)) {
      return false;
    }
    sscanf(buffer + pos, "%x", &bucketCount);
    buckets = new HashItem<T> *[bucketCount];
    memset(buckets, 0, sizeof(HashItem<T>*) * bucketCount);
    pos += strlen(buffer + pos) + 1;
    if (pos >= bufferSize) {
      return false;
    }
    for (uint32_t i = 0; i < bucketCount; i++) {
      HashItem<T> *lastHashItem = nullptr;
      while (*(buffer + pos) != '\0') {
        if (pos >= bufferSize) {
          return false;
        }

        HashItem<T> *hashItem = new HashItem<T>();
        hashItem->hashItemStorage = new T();
        uint32_t deserializeSize = hashItem->hashItemStorage->deserialize(buffer + pos, bufferSize - pos);
        pos += deserializeSize;
        if (pos >= bufferSize || deserializeSize == 0) {
          return false;
        }

        _size++;

        if (lastHashItem) {
          lastHashItem->next = hashItem;
        } else {
          buckets[i] = hashItem;
        }
        lastHashItem = hashItem;
      }
      pos++;
    }
    return true;
  }

private:
  bool hasNewlineBefore(char *buffer, uint32_t bufferSize) {
    char *p = buffer;
    for (uint32_t i = 0; i < bufferSize; ++i) {
      if (*p == '\0')
        return true;
      p++;
    }
    return false;
  }

  void cleanup() {
    if (buckets) {
      for (uint32_t i = 0; i < bucketCount; i++) {
        HashItem<T> *hashItem = buckets[i];
        while (hashItem) {
          HashItem<T> *tempHashItem = hashItem;
          hashItem = hashItem->next;
          delete tempHashItem;
        }
      }
      delete[] buckets;
      buckets = nullptr;
      _size = 0;
    }
  }

  uint32_t serializeBuckets(char *buffer) {
    uint32_t totalSize = 0;
    char sz[512];
    totalSize += 1 + sprintf(sz, "%x", bucketCount);
    if (buffer) {
      memcpy(buffer, sz, totalSize);
    }
    for (uint32_t i = 0; i < bucketCount; i++) {
      HashItem<T> *hashItem = buckets[i];
      while (hashItem) {
        if (buffer) {
          totalSize += hashItem->hashItemStorage->serialize(buffer + totalSize);
        } else {
          totalSize += hashItem->hashItemStorage->serialize(nullptr);
        }
        hashItem = hashItem->next;
      }
      if (buffer) {
        buffer[totalSize] = '\0';
      }
      // Second null terminator to show next bucket
      totalSize++;
    }
    return totalSize;
  }

  uint32_t bucketCount;
  HashItem<T> **buckets;
  uint32_t _size;
};
