
#ifndef P4ML_MANAGER_H
#define P4ML_MANAGER_H

#include "../common/dma_common.h"
#include "../common/packet.h"
#include "../common/utils.h"
#include "../common/window_manager.h"
#include "../common/HashTable.h"
#include "../common/quantize.h"
#include "../common/p4ml_struct.h"
#include <arpa/inet.h>
#include <assert.h>
#include <bits/stdc++.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <iostream>
#include <limits.h>
#include <netinet/ip.h>
#include <queue>
#include <random>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#define FLOATING_POINT_INPUT false

#define ONLY_DO_QUAN false

#define OVERFLOW_THRESHOLD 213
#define UNDERFLOW_THRESHOLD -213

#define P4ML_KEY_TOTAL 500000
#define MAX_TENSOR_SIZE 1024000

#define MAX_THREAD_PER_APP 20

class P4mlManager {
public:
    P4mlManager(uint32_t host, int num_worker, int appID, int num_PS);
    // ~P4mlManager();

    void init_threadPool(int num_thread);
    void PushPull(uint64_t key, char* data, int len, int cmd);
    static void PushPullLoop(int thread_id);
    static void QuantizationLoop(int thread_id);

    void PushTaskToThread(uint64_t key, char *data, int len, int cmd, int thread_id);

    uint64_t GetNewKey();
    int64_t GetFinishKey();
    double GetLossRate();
    int GetCollisionTimeAndClear();
    void SetForceForward(float forward_rate);
    void SetMaxAgtrSizePerThread(int max_agtr_size_per_thread);
    void SetUsedSwitchAGTRcount(int used_agtr);

private:
    static uint32_t host;
    static uint8_t num_worker;
    static uint8_t num_PS;
    static uint16_t appID;
    static uint64_t p4mlKey;
    static AppInfo* app_info;
    
    static int max_agtr_size_per_thread;
    static int UsedSwitchAGTRcount;
    static int _num_thread;
    static std::chrono::time_point<std::chrono::system_clock> start;
    static ThreadInfo** threadInfoQueue;
    static DMAcontext** dmaContextQueue;
    static std::thread** threadQueue;
    static std::thread** pushPullthreadQueue;
    static std::queue<Job*>* pushPulljobQueue;
    static std::thread** quantizationthreadQueue;
    static std::queue<Job*>* quantizejobQueue;
    static std::queue<Job*>* dequantizejobQueue;

    static WindowManager* window_manager;
    static std::queue<Job*> finishQueue;
    static std::queue<agghdr*>* pendingQueue;
    static uint64_t* weightQueue;

    // static uint16_t* hash_map;
    static HashTable* hash_table;
    static int32_t** quantizeBuffer;
    static bool** isOverflow;

    static bool isForceForward;
    static int forwardFrequency;
    static float forwardRate;

    static std::mutex Resource_mutex;
    static std::mutex _P4MLKey_mutex;
    static std::mutex _print_mutex;
    static std::mutex _queuePush_mutex;

    static void main_receive_packet_loop(DMAcontext* dma_context, int32_t* data, int my_id);
    static void updateModel(agghdr* p4ml_header, int32_t* data, int my_id);
};

inline void P4mlManager::updateModel(agghdr* p4ml_header, int32_t* data, int my_id)
{
    uint16_t* p_seq = &p4ml_header->seq_num;
    uint32_t* tensor_len = &pushPulljobQueue[my_id].front()->len;

    int32_t* p_model = p4ml_header->vector;
    uint32_t offset = (*p_seq - 1) * MAX_ENTRIES_PER_PACKET;
    if (offset < *tensor_len) {
        if (offset + MAX_ENTRIES_PER_PACKET > *tensor_len)
            memcpy(data + offset, p_model, sizeof(int32_t) * (*tensor_len % MAX_ENTRIES_PER_PACKET));
        else
            memcpy(data + offset, p_model, sizeof(int32_t) * MAX_ENTRIES_PER_PACKET);
    }
}

#endif //P4ML_MANAGER_H