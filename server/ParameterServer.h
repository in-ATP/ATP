#include <iostream>
#include <ctime>
#include <cmath>
#include <random>
#include <arpa/inet.h>
#include <chrono>
#include <map>
#include <unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <cmath>
#include <algorithm>
#include <netinet/ip.h>
#include <set>
#include "../common/packet.h"
#include "../common/dma_common.h"
#include "../common/ThreadPool.h"
#include "../common/utils.h"
#include "../common/window_manager.h"
#include "../common/HashTable.h"

#define MAX_TENSOR_SIZE 1024000
// Lam: this one is useless since a PS can only handle 1app, to be mod.
#define MAX_APP_PER_THREAD 5
#define MAX_STORAGE_PER_APP_PER_THREAD 10
#define MAX_WORKER 16

#define MAX_THREAD_PER_APP 20

#define OVERFLOW_HANDLE false


union data_t {
    int32_t *data_int;
    float *data_float;
};

struct tensor_context {
    bool* isOccupy;
    bool* isCollision;
    bool* isFloat;
    bool isCompleted;
    data_t data;
    uint32_t len;
    uint64_t key;
    uint8_t num_worker;
    WindowManager* window_manager;
    std::chrono::time_point<std::chrono::system_clock> start_time;
};

void inline init_tensor(tensor_context* tensor, uint32_t len) {
    tensor->data.data_int = new int32_t[len]();
    tensor->isCompleted = true;
    tensor->isOccupy = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1]();
    tensor->isCollision = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1]();
    tensor->isFloat = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1]();
    tensor->len = 0;
    tensor->num_worker = 0;
    tensor->key = 0xffffffffffffffff;
    tensor->window_manager = new WindowManager[MAX_WORKER];
    for (int i = 0; i < MAX_WORKER; i++) {
        tensor->window_manager[i].isACKed = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1]();
        tensor->window_manager[i].total_ACK = MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1;
    }
}

int inline check_tensor_available(tensor_context* tensor, agghdr* p4ml_header, int thread_id) {
    // printf("*skey: %d, seq: %d\n", *skey, p4ml_header->seq_num);

    // Already have completed model and not retrieve
    if (tensor->isCompleted && p4ml_header->key != tensor->key) {
        int total_ACK = ceil((float)p4ml_header->len_tensor / MAX_ENTRIES_PER_PACKET);
        for (int i = 0; i < p4ml_header->num_worker; i++) 
            tensor->window_manager[i].Reset(total_ACK);
        // if (thread_id == 0)
        // printf("Reset tensors[%d] LAST_ACK: %d\n", *skey, tensor->window_manager[0].last_ACK);
        memset(tensor->data.data_int, 0, sizeof(int32_t) * p4ml_header->len_tensor);
        memset(tensor->isOccupy, 0, sizeof(bool) * (total_ACK + 1));
        memset(tensor->isCollision, 0, sizeof(bool) * (total_ACK + 1));
        memset(tensor->isFloat, 0, sizeof(bool) * (total_ACK + 1));
        tensor->num_worker = p4ml_header->num_worker; 
        tensor->len = p4ml_header->len_tensor;
        tensor->isCompleted = false;
        tensor->key = p4ml_header->key;
        // printf("Place %d available, real key = %d\n", *skey, tensors[*skey].key);
        return 1;
    } 
    return 0;
}

void inline makeTensorReadyforFloat(agghdr *p4ml_header, tensor_context *tensor_cnt) {
    int32_t* data = tensor_cnt->data.data_int;
    uint16_t *p_seq = &p4ml_header->seq_num;
    int32_t *p_model = p4ml_header->vector;
    uint32_t offset = (*p_seq - 1) * MAX_ENTRIES_PER_PACKET;
    
    /* Reset Data */
    memset(data + offset, 0, sizeof(int32_t) * MAX_ENTRIES_PER_PACKET);
    tensor_cnt->isOccupy[*p_seq] = false;
    
    /* Reset Bitmap */
    for (int i = 0; i < p4ml_header->num_worker; i++) {
        tensor_cnt->window_manager[i].isACKed[p4ml_header->seq_num] = 0;
    }
}

void inline updateModel(agghdr *p4ml_header, tensor_context *dst_place, bool isFloat) {
    int32_t* data = dst_place->data.data_int;
    uint16_t *p_seq = &p4ml_header->seq_num;
    uint32_t *tensor_len = &p4ml_header->len_tensor;
    int32_t *p_model = p4ml_header->vector;
    uint32_t offset = (*p_seq - 1) * MAX_ENTRIES_PER_PACKET;
    // printf("dst_place->isOccupy[%d]: %d\n", *p_seq - 1, dst_place->isOccupy[*p_seq - 1]);
    if (!dst_place->isOccupy[*p_seq]) {
        // printf("replace\n");
        if (offset < *tensor_len) {
            if (offset + MAX_ENTRIES_PER_PACKET > *tensor_len)
                memcpy(data + offset, p_model, sizeof(int32_t) * (*tensor_len % MAX_ENTRIES_PER_PACKET));
            else
                memcpy(data + offset, p_model, sizeof(int32_t) * MAX_ENTRIES_PER_PACKET);
        } else {
            printf("Update with offset %d > tensor length %d, something wrong.\n", offset, *tensor_len);
        }
        dst_place->isOccupy[*p_seq] = true;
    } else {
        // printf("addition\n");
        if (isFloat) {
            float* data = dst_place->data.data_float;
            float* p_model = (float*) p4ml_header->vector;

            if (offset < *tensor_len) {
                for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                    data[offset + i] += p_model[i];
            } else {
                printf("Update with offset %d > tensor length %d, something wrong.\n", offset, *tensor_len);
            }
        } else {
            if (offset < *tensor_len) {
                for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                    data[offset + i] +=  p_model[i];
            } else {
                printf("Update with offset %d > tensor length %d, something wrong.\n", offset, *tensor_len);
            }
        }
    }
}

void inline updateModel_force(agghdr *p4ml_header, tensor_context *dst_place) {
    int32_t* data = dst_place->data.data_int;
    uint16_t *p_seq = &p4ml_header->seq_num;
    uint32_t *tensor_len = &p4ml_header->len_tensor;
    int32_t *p_model = p4ml_header->vector;
    uint32_t offset = (*p_seq - 1) * MAX_ENTRIES_PER_PACKET;
    
    if (offset < *tensor_len) {
        if (offset + MAX_ENTRIES_PER_PACKET > *tensor_len)
            memcpy(data + offset, p_model, sizeof(int32_t) * (*tensor_len % MAX_ENTRIES_PER_PACKET));
        else
            memcpy(data + offset, p_model, sizeof(int32_t) * MAX_ENTRIES_PER_PACKET);
    } else {
        printf("Update with offset %d > tensor length %d, something wrong.\n", offset, *tensor_len);
    }
    dst_place->isOccupy[*p_seq] = true;
}