#ifndef CC_MANAGER_H
#define CC_MANAGER_H

#define MAX_BYTES 100 * P4ML_PACKET_SIZE

#include "packet.h"
#include <iostream>
#include <stdint.h>
#include <stdio.h>

using namespace std;
#define do_div(n, base) ({            \
    uint32_t __base = (base);         \
    uint32_t __rem;                   \
    __rem = ((uint64_t)(n)) % __base; \
    (n) = ((uint64_t)(n)) / __base;   \
    __rem;                            \
})
#define GET_MIN(a, b) (a < b ? a : b)
#define GET_MAX(a, b) (a > b ? a : b)

class CC_manager {

public:
    CC_manager(int init_window)
    {
        cwnd_bytes = init_window * P4ML_PACKET_SIZE;
    }

    int adjustWindow(bool isECN)
    {
        if (isECN)
        {
            cwnd_bytes /= 2;
        }
        else
        {
            cwnd_bytes += 1500;
        }

        if (cwnd_bytes < P4ML_PACKET_SIZE)
            cwnd_bytes = P4ML_PACKET_SIZE;
        if (cwnd_bytes > MAX_BYTES)
            cwnd_bytes = MAX_BYTES;
        if (cwnd_bytes > P4ML_PACKET_SIZE)
            cwnd_bytes = (cwnd_bytes / P4ML_PACKET_SIZE) * P4ML_PACKET_SIZE;
        return cwnd_bytes / P4ML_PACKET_SIZE;
    }

private:
    uint64_t cwnd_bytes;
};

#endif