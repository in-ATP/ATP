#ifndef HASHTABLE_H
#define HASHTABLE_H
#include <bits/stdc++.h>
#include "packet.h"
#include "utils.h"
#define CRCPOLY_LE 0xedb88320

class HashTable {

public:
    HashTable(int size);
    void HashNew_linear(int index);
    int HashNew_crc(uint16_t appID, uint16_t index);
    int HashNew_predefine();
    void HashNew_separate(uint16_t appID, uint16_t index);
    uint16_t* hash_map;
    bool isAlreadyDeclare[MAX_AGTR_COUNT];

private:
    int used_size;
    uint32_t crc32_le(uint32_t crc, unsigned char const* p, size_t len);
    int predefine_agtr_list[MAX_AGTR_COUNT];

    // These for predefine Hash

    // These two for Linear Hash
    uint16_t hash_function();
    uint16_t hash_pos;

};

#endif