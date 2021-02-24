#ifndef DMA_COMMON_H
#define DMA_COMMON_H

#include "mlx5_defs.h"
#include "packet.h"
#include "utils.h"
#include <assert.h>
#include <cmath>
#include <inttypes.h>
#include <net/if.h> //ifreq
#include <netdb.h>
#include <netinet/in.h>
#include <rdma/rdma_cma.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/socket.h>

#define POLLING_SIZE 400
#define ENTRY_SIZE 256 /* maximum size of each buffer */
#define PORT_NUM 1

#define DEBUG_PRINT_ALL_SENDING_PACKET false
#define DEBUG_PRINT_ALL_RECEIVING_PACKET false

#define DEBUG_CHECK_SEND_RECEIVE_TOTAL false

static constexpr size_t kAppRecvCQDepth = 8;
static constexpr size_t kAppRQDepth = 4; // Multi-packet RQ depth

static constexpr size_t kAppLogNumStrides = 9;
static constexpr size_t kAppLogStrideBytes = 9;
static constexpr size_t kAppMaxPostlist = 512;

static constexpr bool kAppVerbose = false;
static constexpr bool kAppCheckContents = true; // Check buffer contents

/// Size of one ring message buffer
static constexpr size_t kAppRingMbufSize = (1ull << kAppLogStrideBytes);

/// Number of strides in one multi-packet RECV WQE
static constexpr size_t kAppStridesPerWQE = (1ull << kAppLogNumStrides);

/// Packets after which the CQE snapshot cycles
static constexpr size_t kAppCQESnapshotCycle = 65536 * kAppStridesPerWQE;

/// Total number of entries in the RX ring
static constexpr size_t kAppNumRingEntries = (kAppStridesPerWQE * kAppRQDepth);

static constexpr size_t kAppRingSize = (kAppNumRingEntries * kAppRingMbufSize);

/// A consistent snapshot of CQE fields in host endian format
struct cqe_snapshot_t {
    uint16_t wqe_id;
    uint16_t wqe_counter;

    /// Return this packet's index in the CQE snapshot cycle
    size_t get_cqe_snapshot_cycle_idx() const
    {
        return wqe_id * kAppStridesPerWQE + wqe_counter;
    }

    std::string to_string()
    {
        std::ostringstream ret;
        ret << "[ID " << std::to_string(wqe_id) << ", counter "
            << std::to_string(wqe_counter) << "]";
        return ret.str();
    }
};

struct DMAcontext {
    struct ibv_pd* pd;
    struct ibv_context* ctx;
    struct ibv_cq* receive_cq;
    struct ibv_cq* send_cq;
    struct ibv_mr* send_mr;
    void* send_region;
    struct ibv_qp* data_qp;

    struct ibv_qp* mp_recv_qp;
    struct ibv_cq* mp_recv_cq;
    struct ibv_exp_wq* mp_wq;
    struct ibv_exp_wq_family* mp_wq_family;
    struct ibv_exp_rwq_ind_table* mp_ind_tbl;
    volatile mlx5_cqe64* mp_cqe_arr;
    struct ibv_sge* mp_sge;
    uint8_t* mp_recv_ring;
    uint8_t* mp_send_ring;
    struct ibv_mr* mp_send_mr;

    // for connection
    int id;
    int total_received;
    int total_sent;
    int my_send_queue_length;
    int my_recv_queue_length;

    size_t ring_head;
    size_t nb_rx_rolling;
    size_t sge_idx;
    size_t cqe_idx;

    cqe_snapshot_t prev_snapshot;

    bool isPS;

    // // For window adjustment
    bool isMarkTimeStamp;
    bool* isSent;
    std::chrono::high_resolution_clock::time_point* first_send_time;
    std::chrono::high_resolution_clock::time_point* first_receive_time;
};

DMAcontext* DMA_create(ibv_device* ib_dev, int thread_id, bool isPS);
const char* ibv_wc_opcode_str(enum ibv_wc_opcode opcode);
void send_packet(DMAcontext* dma_context, int packet_size, uint64_t offset);
size_t receive_packet(DMAcontext *dma_context, cqe_snapshot_t* new_snapshot);
void dma_postback(DMAcontext *dma_context);
void dma_update_snapshot(DMAcontext *dma_context, cqe_snapshot_t new_snapshot);
void dma_context_print(DMAcontext* dma_context, const char* caption);

// Install a UDP destination port--based flow rule
void install_flow_rule(struct ibv_qp* qp, uint16_t thread_id, bool isPS);
void install_udp_flow_rule(struct ibv_qp* qp, uint16_t dst_port);
void snapshot_cqe(volatile mlx5_cqe64* cqe, cqe_snapshot_t& cqe_snapshot);
size_t get_cycle_delta(const cqe_snapshot_t& prev, const cqe_snapshot_t& cur);
#endif
