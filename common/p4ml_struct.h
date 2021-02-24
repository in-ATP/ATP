#ifndef P4ML_STRUCT_H
#define P4ML_STRUCT_H
#include <inttypes.h>

#include "packet.h"

struct ThreadInfo
{
    int thread_id;
    int agtr_start_pos;
};

struct Job
{
    uint64_t key;
    float *float_data;
    int32_t *int_data;
    uint32_t len;
    int cmd;
};

struct AppInfo
{
    uint32_t host;
    uint16_t appID;
    uint8_t num_worker;
    uint8_t num_PS;
};

#endif