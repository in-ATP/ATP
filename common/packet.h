#ifndef PACKET_P4ML_H
#define PACKET_P4ML_H
#include <stdint.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdio>
#include <thread>
#include <mutex>
#include <inttypes.h>
#include <iostream>
#include <bitset>
#include <chrono>
#include "utils.h"
#include "p4ml_struct.h"

#define PS_FILTER_TEMPLATE 0x05, 0x04, 0x03, 0x02, 0x01, 0xFF
#define WORKER_FILTER_TEMPLATE 0x77, 0x77, 0x77, 0x77, 0x77, 0xFF

// #define SRC_MAC 0xb8, 0x59, 0x9f, 0x1d, 0x04, 0xf2 
#define SRC_MAC 0xe4, 0x1d, 0x2d, 0xf3, 0xdd, 0xcc
// #define DST_MAC 0xb8, 0x59, 0x9f, 0x0b, 0x30, 0x72

#define ETH_TYPE 0x07, 0x00

#define IP_HDRS 0x45, 0x00, 0x00, 0x54, 0x00, 0x00, 0x40, 0x00, 0x40, 0x01, 0xaf, 0xb6

#define SRC_IP 0x0d, 0x07, 0x38, 0x66

#define DST_IP 0x0d, 0x07, 0x38, 0x7f

#define SRC_PORT 0x67, 0x67

#define DST_PORT 0x78, 0x78

#define UDP_HDRS 0x00, 0x00, 0x00, 0x00

// Only a template, DST_IP will be modified soon
// This one is for sending
const unsigned char PS_IP_ETH_UDP_HEADER[] = { WORKER_FILTER_TEMPLATE, SRC_MAC, ETH_TYPE, IP_HDRS, SRC_IP, DST_IP };
const unsigned char WORKER_IP_ETH_UDP_HEADER[] = { PS_FILTER_TEMPLATE, SRC_MAC, ETH_TYPE, IP_HDRS, SRC_IP, DST_IP };

// P4ML_PACKET_SIZE = IP_ETH_HEADER_SIZE + P4ML_HEADER_SIZE + P4ML_DATA_SIZE
#define P4ML_PACKET_SIZE 308
#define P4ML_DATA_SIZE 248
#define P4ML_HEADER_SIZE 26
#define P4ML_LAYER_SIZE 274
#define IP_ETH_UDP_HEADER_SIZE 34

#define MAX_ENTRIES_PER_PACKET 62

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

#pragma pack(push, 1)
    struct agghdr {
        uint32_t bitmap;
        uint8_t num_worker;
        uint8_t flag;
        // reserved       :  2;
        // isForceFoward  :  1;

        /* Current version
        overflow       :  1;
        PSIndex        :  2;
        dataIndex      :  1;
        ECN            :  1;
        isResend       :  1;
        isSWCollision  :  1;
        isACK          :  1;
        */

        uint16_t appID;
        uint16_t seq_num;
        uint16_t agtr;
        uint16_t agtr2;
        int32_t vector[MAX_ENTRIES_PER_PACKET];
        uint64_t key;
        uint32_t len_tensor;
};
#pragma pack(pop)

static std::mutex _packet_print_mutex;

void inline make_p4ml_layer_and_copy_to(void* payload, Job* job_info, AppInfo* app_info, uint16_t* agtr, uint16_t* seq_num, int* offset, bool isResend, bool isForceForward, bool isOverflow)
{
    agghdr* agg_header = (agghdr*)payload;
    agghdr* p4ml_header = agg_header;
    agg_header->key = job_info->key;
    agg_header->len_tensor = htonl(job_info->len);
    agg_header->bitmap = htonl(1 << (app_info->host));
    agg_header->num_worker = app_info->num_worker;
    agg_header->appID = htons(app_info->appID);
    agg_header->flag = 0;
    agg_header->agtr = htons(*agtr);
    //TODO: clarify this and UsedSwitchAGTRcount
    agg_header->agtr2 = htons(*agtr + MAX_AGTR_COUNT);
    agg_header->seq_num = htons(*seq_num);

    agg_header->flag = ((job_info->key % app_info->num_PS)) << 5;

    if (isResend) 
        agg_header->flag |= 4;
        
    if (isForceForward)
        agg_header->flag |= 32;

    if (isOverflow) 
        agg_header->flag |= 128;
    // PS Index
    // agg_header->flag |= (*num_PS << 5);
    // printf("to PS: %d\n", ((*key % *num_PS)+1));

    int32_t* used_data;
    if (isOverflow) {
        used_data = (int32_t*) job_info->float_data;
    }
    else 
        used_data = (int32_t*) job_info->int_data;

    int32_t* send_data;
    if (*offset + MAX_ENTRIES_PER_PACKET > job_info->len) {
        int32_t* tmp = new int32_t[MAX_ENTRIES_PER_PACKET]();
        memcpy(tmp, used_data + *offset, sizeof(int32_t) * (job_info->len % MAX_ENTRIES_PER_PACKET));
        send_data = tmp;
        delete tmp;
    } else {
        send_data = used_data + *offset;
    }

    // p4ml_header_print_h(agg_header, "Make");
}

// void inline make_packet_and_copy_to(void* payload, uint64_t* key, uint32_t* len_tensor, uint32_t* workerID, uint8_t* num_worker, uint16_t* appID, uint16_t* agtr, uint16_t* seq_num, int32_t* data, bool isResend, bool isForceForward, uint8_t* num_PS, int thread_id)
// {
//     char* eth_ip_header = (char*)payload;
//     memcpy(payload, IP_ETH_UDP_HEADER, sizeof(IP_ETH_UDP_HEADER));
//     eth_ip_header[5] = thread_id;
//     make_p4ml_layer_and_copy_to((char*)payload + sizeof(IP_ETH_UDP_HEADER), key, len_tensor, workerID, num_worker, appID, agtr, seq_num, data, isResend, isForceForward, num_PS);
// }

void inline p4ml_header_ntoh(agghdr* p_p4ml)
{
    p_p4ml->len_tensor = ntohl(p_p4ml->len_tensor);
    p_p4ml->bitmap = ntohl(p_p4ml->bitmap);
    p_p4ml->seq_num = ntohs(p_p4ml->seq_num);
    p_p4ml->agtr = ntohs(p_p4ml->agtr);
    p_p4ml->agtr2 = ntohs(p_p4ml->agtr2);
    p_p4ml->appID = ntohs(p_p4ml->appID);
    int32_t* p_model = p_p4ml->vector;

    /* if not float */
    if (!(p_p4ml->flag & 0x80)) {
        for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
            p_model[i] = ntohl(p_model[i]);
    }
}

void inline p4ml_header_ntoh_without_data(agghdr* p_p4ml)
{
    p_p4ml->len_tensor = ntohl(p_p4ml->len_tensor);
    p_p4ml->bitmap = ntohl(p_p4ml->bitmap);
    p_p4ml->seq_num = ntohs(p_p4ml->seq_num);
    p_p4ml->agtr = ntohs(p_p4ml->agtr);
    p_p4ml->agtr2 = ntohs(p_p4ml->agtr2);
    p_p4ml->appID = ntohs(p_p4ml->appID);
    // // p_p4ml->last_ack = ntohl(p_p4ml->last_ack);
    int32_t* p_model = p_p4ml->vector;
}

void inline p4ml_header_hton_without_data(agghdr* p_p4ml)
{
    p_p4ml->len_tensor = htonl(p_p4ml->len_tensor);
    p_p4ml->bitmap = htonl(p_p4ml->bitmap);
    p_p4ml->seq_num = htons(p_p4ml->seq_num);
    p_p4ml->agtr = htons(p_p4ml->agtr);
    p_p4ml->agtr2 = htons(p_p4ml->agtr2);
    p_p4ml->appID = htons(p_p4ml->appID);
    // // p_p4ml->last_ack = htonl(p_p4ml->last_ack);
}

void inline p4ml_header_setACK(agghdr* p4ml_header)
{
    p4ml_header->flag |= 1;
}

void inline p4ml_header_setOverflow(agghdr* p4ml_header)
{
    p4ml_header->flag |= 128;
}

void inline p4ml_header_setOverflowRequest(agghdr* p4ml_header)
{
    p4ml_header->flag |= 128;
    p4ml_header->flag &= ~(4);
}

void inline p4ml_header_setCollisionBit(agghdr* p4ml_header)
{
    p4ml_header->flag |= 2;
}

void inline p4ml_header_setLengthFieldToAgtr(agghdr* p4ml_header, uint16_t new_agtr)
{
    p4ml_header->len_tensor = new_agtr;
}

void inline p4ml_header_resetIndex(agghdr* p4ml_header)
{
    p4ml_header->flag &= ~(16);
}

void inline p4ml_header_resetCollisionBit(agghdr* p4ml_header)
{
    p4ml_header->flag &= ~(2);
}

void inline p4ml_header_print(agghdr *p4ml_header, const char *caption)
{
    std::lock_guard<std::mutex> lock(_packet_print_mutex);
    printf("[%s] \n key: %" PRIu64 ", len_tensor: %u, "
           "bitmap: " BYTE_TO_BINARY_PATTERN ", num_worker: %u, appID: %u, "
           "agtr: %u, agtr2: %u, seq_num: %u, isACK: %d, dataIndex: %d,"
           "isResend: %d, isOverflow: %d, data: ",
        caption, p4ml_header->key, p4ml_header->len_tensor,
        BYTE_TO_BINARY(p4ml_header->bitmap), p4ml_header->num_worker, p4ml_header->appID,
        p4ml_header->agtr, p4ml_header->agtr2, p4ml_header->seq_num,
        p4ml_header->flag & 1 ? 1 : 0, p4ml_header->flag & 16 ? 1 : 0, p4ml_header->flag & 4 ? 1 : 0,
        p4ml_header->flag & 128 ? 1 : 0);

    // is Overflow?
    if (p4ml_header->flag & 128)
        // is ACK?  isn't Resend?
        if (p4ml_header->flag & 1 && !(p4ml_header->flag & 4))
            printf("REQUEST - CARELESS.");
        else
            for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                printf("%.7f ", ntohf((p4ml_header->vector)[i]));
    else
        for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
            printf("%d ", p4ml_header->vector[i]);
    printf("\n");
}

void inline p4ml_header_print_h(agghdr *p4ml_header, const char *caption)
{
    std::lock_guard<std::mutex> lock(_packet_print_mutex);
    printf("[%s] \n key: %" PRIu64 ", len_tensor: %u, "
           "bitmap: " BYTE_TO_BINARY_PATTERN ", num_worker: %u, appID: %u, "
           "agtr: %u, agtr2: %u, seq_num: %u, isACK: %d, dataIndex: %d,"
           "isResend: %d, isOverflow: %d, data: ",
        caption, p4ml_header->key, ntohl(p4ml_header->len_tensor),
        BYTE_TO_BINARY(ntohl(p4ml_header->bitmap)), p4ml_header->num_worker, ntohs(p4ml_header->appID),
        ntohs(p4ml_header->agtr), ntohs(p4ml_header->agtr2), ntohs(p4ml_header->seq_num),
        p4ml_header->flag & 1 ? 1 : 0, p4ml_header->flag & 16 ? 1 : 0, p4ml_header->flag & 4 ? 1 : 0,
        p4ml_header->flag & 128 ? 1 : 0);

    // is Overflow?
    if (p4ml_header->flag & 128)
        // is ACK?  isn't Resend?
        if (p4ml_header->flag & 1 && !(p4ml_header->flag & 4))
            printf("REQUEST - CARELESS.");
        else 
            for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                printf("%.7f ", ((float *)(p4ml_header->vector))[i]);
    else
        for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
            printf("%d ", ntohl(p4ml_header->vector[i]));
    printf("\n");
}

#endif
