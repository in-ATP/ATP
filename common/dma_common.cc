#define __USE_GNU

#include "dma_common.h"
#include <infiniband/verbs_exp.h>
#include <inttypes.h>
#include <linux/if_ether.h>
#include <netdb.h>
#include <netinet/in.h>
#include <rdma/rdma_cma.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


std::mutex ___print_mutex;
int my_send_queue_length = 2048;
int my_recv_queue_length = my_send_queue_length * 8;

unsigned char PS_FILTER_TEMPLATE_R[] = { 0x05, 0x04, 0x03, 0x02, 0x01, 0xFF };
unsigned char WORKER_FILTER_TEMPLATE_R[] = { 0x77, 0x77, 0x77, 0x77, 0x77, 0xFF };

DMAcontext* DMA_create(ibv_device* ib_dev, int thread_id, bool isPS)
{

    ibv_context* context = ibv_open_device(ib_dev);
    if (!context) {
        fprintf(stderr, "Couldn't get context for %s\n",
            ibv_get_device_name(ib_dev));
        exit(1);
    }
    ibv_pd* pd = ibv_alloc_pd(context);
    if (!pd) {
        fprintf(stderr, "Couldn't allocate PD\n");
        exit(1);
    }

    struct ibv_cq* rec_cq = ibv_create_cq(context, my_recv_queue_length + 1, NULL, NULL, 0);
    if (!rec_cq) {
        fprintf(stderr, "Couldn't create CQ %d\n", errno);
        exit(1);
    }

    struct ibv_cq* snd_cq = ibv_create_cq(context, my_send_queue_length + 1, NULL, NULL, 0);
    if (!snd_cq) {
        fprintf(stderr, "Couldn't create CQ %d\n", errno);
        exit(1);
    }

    struct ibv_qp* qp;
    struct ibv_exp_qp_init_attr* qp_init_attr = (struct ibv_exp_qp_init_attr*)malloc(sizeof(struct ibv_exp_qp_init_attr));

    memset(qp_init_attr, 0, sizeof(*qp_init_attr));
    qp_init_attr->comp_mask = IBV_EXP_QP_INIT_ATTR_PD | IBV_EXP_QP_INIT_ATTR_MAX_TSO_HEADER | IBV_EXP_QP_INIT_ATTR_INL_RECV;
    qp_init_attr->send_cq = snd_cq;
    qp_init_attr->recv_cq = rec_cq;
    qp_init_attr->qp_type = IBV_QPT_RAW_PACKET;

    qp_init_attr->pd = pd;
    qp_init_attr->cap.max_send_wr = my_send_queue_length + 1;
    qp_init_attr->cap.max_recv_wr = my_recv_queue_length + 1;
    qp_init_attr->cap.max_inline_data = 512;
    qp_init_attr->cap.max_send_sge = 1;
    qp_init_attr->cap.max_recv_sge = 1;
    qp_init_attr->max_tso_header = IP_ETH_UDP_HEADER_SIZE;
    qp_init_attr->max_inl_recv = 512;

    qp = ibv_exp_create_qp(context, qp_init_attr);
    //qp = ibv_create_qp(pd, qp_init_attr);
    if (!qp) {
        fprintf(stderr, "Couldn't create RSS QP\n");
        exit(1);
    }

    struct ibv_qp_attr qp_attr;
    int qp_flags;
    int ret;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_flags = IBV_QP_STATE | IBV_QP_PORT;
    qp_attr.qp_state = IBV_QPS_INIT;
    qp_attr.port_num = 1;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0) {
        fprintf(stderr, "failed modify qp to init\n");
        exit(1);
    }
    memset(&qp_attr, 0, sizeof(qp_attr));

    /* a. Move ring state to ready to receive, this is needed to be able to move ring to ready to send even if receive queue is not enabled */

    qp_flags = IBV_QP_STATE;
    qp_attr.qp_state = IBV_QPS_RTR;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0) {
        fprintf(stderr, "failed modify qp to receive\n");
        exit(1);
    }

    /* b. Move the ring to ready to send */

    qp_flags = IBV_QP_STATE;
    qp_attr.qp_state = IBV_QPS_RTS;
    ret = ibv_modify_qp(qp, &qp_attr, qp_flags);
    if (ret < 0) {
        fprintf(stderr, "failed modify qp to send\n");
        exit(1);
    }

    int send_buf_size = P4ML_PACKET_SIZE * my_send_queue_length;

    void* send_buf;

    //send_buf = malloc(send_buf_size);
    // send_buf = alloc_raw_pages(send_buf_size / EACH_HUGEPAGE_SIZE + 1, EACH_HUGEPAGE_SIZE);
    ib_malloc(&send_buf, send_buf_size);
    if (!send_buf) {
        fprintf(stderr, "Coudln't allocate send memory\n");
        exit(1);
    }

    struct ibv_mr* send_mr;
    send_mr = ibv_reg_mr(pd, send_buf, send_buf_size, IBV_ACCESS_LOCAL_WRITE);
    if (!send_mr) {
        fprintf(stderr, "Couldn't register recv mr\n");
        exit(1);
    }

    // Init CQ. Its size MUST be one so that we get two CQEs in mlx5.
    struct ibv_exp_cq_init_attr cq_init_attr;
    memset(&cq_init_attr, 0, sizeof(cq_init_attr));
    struct ibv_cq* mp_recv_cq = ibv_exp_create_cq(context, kAppRecvCQDepth / 2, nullptr, nullptr, 0, &cq_init_attr);
    assert(mp_recv_cq != nullptr);

    // Modify the RECV CQ to ignore overrun
    struct ibv_exp_cq_attr cq_attr;
    memset(&cq_attr, 0, sizeof(cq_attr));
    cq_attr.comp_mask = IBV_EXP_CQ_ATTR_CQ_CAP_FLAGS;
    cq_attr.cq_cap_flags = IBV_EXP_CQ_IGNORE_OVERRUN;
    rt_assert(ibv_exp_modify_cq(mp_recv_cq, &cq_attr, IBV_EXP_CQ_CAP_FLAGS) == 0);

    struct ibv_exp_wq_init_attr wq_init_attr;
    memset(&wq_init_attr, 0, sizeof(wq_init_attr));

    wq_init_attr.wq_type = IBV_EXP_WQT_RQ;
    wq_init_attr.max_recv_wr = kAppRQDepth;
    wq_init_attr.max_recv_sge = 1;
    wq_init_attr.pd = pd;
    wq_init_attr.cq = mp_recv_cq;

    wq_init_attr.comp_mask |= IBV_EXP_CREATE_WQ_MP_RQ;
    wq_init_attr.mp_rq.use_shift = IBV_EXP_MP_RQ_NO_SHIFT;
    wq_init_attr.mp_rq.single_wqe_log_num_of_strides = kAppLogNumStrides;
    wq_init_attr.mp_rq.single_stride_log_num_of_bytes = kAppLogStrideBytes;
    struct ibv_exp_wq* mp_wq = ibv_exp_create_wq(context, &wq_init_attr);
    assert(mp_wq != nullptr);

    // Change WQ to ready state
    struct ibv_exp_wq_attr wq_attr;
    memset(&wq_attr, 0, sizeof(wq_attr));
    wq_attr.attr_mask = IBV_EXP_WQ_ATTR_STATE;
    wq_attr.wq_state = IBV_EXP_WQS_RDY;
    rt_assert(ibv_exp_modify_wq(mp_wq, &wq_attr) == 0);

    // Get the RQ burst function
    enum ibv_exp_query_intf_status intf_status = IBV_EXP_INTF_STAT_OK;
    struct ibv_exp_query_intf_params query_intf_params;
    memset(&query_intf_params, 0, sizeof(query_intf_params));
    query_intf_params.intf_scope = IBV_EXP_INTF_GLOBAL;
    query_intf_params.intf = IBV_EXP_INTF_WQ;
    query_intf_params.obj = mp_wq;
    struct ibv_exp_wq_family* mp_wq_family = reinterpret_cast<struct ibv_exp_wq_family*>(
        ibv_exp_query_intf(context, &query_intf_params, &intf_status));
    assert(mp_wq_family != nullptr);

    // Create indirect table
    struct ibv_exp_rwq_ind_table_init_attr rwq_ind_table_init_attr;
    memset(&rwq_ind_table_init_attr, 0, sizeof(rwq_ind_table_init_attr));
    rwq_ind_table_init_attr.pd = pd;
    rwq_ind_table_init_attr.log_ind_tbl_size = 0; // Ignore hash
    rwq_ind_table_init_attr.ind_tbl = &mp_wq; // Pointer to RECV work queue
    rwq_ind_table_init_attr.comp_mask = 0;
    struct ibv_exp_rwq_ind_table* mp_ind_tbl = ibv_exp_create_rwq_ind_table(context, &rwq_ind_table_init_attr);
    assert(mp_ind_tbl != nullptr);

    // Create rx_hash_conf and indirection table for the QP
    uint8_t toeplitz_key[] = { 0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
        0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
        0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
        0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
        0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa };
    const int TOEPLITZ_RX_HASH_KEY_LEN = sizeof(toeplitz_key) / sizeof(toeplitz_key[0]);

    struct ibv_exp_rx_hash_conf rx_hash_conf;
    memset(&rx_hash_conf, 0, sizeof(rx_hash_conf));
    rx_hash_conf.rx_hash_function = IBV_EXP_RX_HASH_FUNC_TOEPLITZ;
    rx_hash_conf.rx_hash_key_len = TOEPLITZ_RX_HASH_KEY_LEN;
    rx_hash_conf.rx_hash_key = toeplitz_key;
    rx_hash_conf.rx_hash_fields_mask = IBV_EXP_RX_HASH_DST_PORT_UDP;
    rx_hash_conf.rwq_ind_tbl = mp_ind_tbl;

    struct ibv_exp_qp_init_attr mp_qp_init_attr;
    memset(&mp_qp_init_attr, 0, sizeof(mp_qp_init_attr));
    mp_qp_init_attr.comp_mask = IBV_EXP_QP_INIT_ATTR_CREATE_FLAGS | IBV_EXP_QP_INIT_ATTR_PD | IBV_EXP_QP_INIT_ATTR_RX_HASH;
    mp_qp_init_attr.rx_hash_conf = &rx_hash_conf;
    mp_qp_init_attr.pd = pd;
    mp_qp_init_attr.qp_type = IBV_QPT_RAW_PACKET;

    // Create the QP
    struct ibv_qp* mp_recv_qp = ibv_exp_create_qp(context, &mp_qp_init_attr);
    assert(mp_recv_qp != nullptr);

    size_t tx_ring_size = P4ML_LAYER_SIZE * kAppMaxPostlist;
    uint8_t* mp_send_ring;
    ib_malloc((void **)&mp_send_ring, tx_ring_size);
    rt_assert(mp_send_ring != nullptr);
    memset(mp_send_ring, 0, tx_ring_size);

    struct ibv_mr* mp_send_mr = ibv_reg_mr(pd, mp_send_ring, tx_ring_size, IBV_ACCESS_LOCAL_WRITE);
    rt_assert(mp_send_mr != nullptr);

    // Register RX ring memory
    uint8_t* mp_recv_ring;
    ib_malloc((void **)&mp_recv_ring, kAppRingSize);
    rt_assert(mp_recv_ring != nullptr);
    memset(mp_recv_ring, 0, kAppRingSize);

    struct ibv_mr* mp_mr = ibv_reg_mr(pd, mp_recv_ring, kAppRingSize, IBV_ACCESS_LOCAL_WRITE);
    rt_assert(mp_mr != nullptr);
    /////////////////////////////////////////////////////////////////////////////////////
    // install_flow_rule(mp_recv_qp, 30720 + thread_id);
    install_flow_rule(mp_recv_qp, thread_id, isPS);
    // This cast works for mlx5 where ibv_cq is the first member of mlx5_cq.
    auto* _mlx5_cq = reinterpret_cast<mlx5_cq*>(mp_recv_cq);
    rt_assert(kAppRecvCQDepth == std::pow(2, _mlx5_cq->cq_log_size));
    rt_assert(_mlx5_cq->buf_a.buf != nullptr);

    auto* mp_cqe_arr = reinterpret_cast<volatile mlx5_cqe64*>(_mlx5_cq->buf_a.buf);

    // Initialize the CQEs as if we received the last (kAppRecvCQDepth) packets
    // in the CQE cycle.
    static_assert(kAppStridesPerWQE >= kAppRecvCQDepth, "");
    for (size_t i = 0; i < kAppRecvCQDepth; i++) {
        mp_cqe_arr[i].wqe_id = htons(std::numeric_limits<uint16_t>::max());
        // Last CQE gets
        // * wqe_counter = (kAppStridesPerWQE - 1)
        // * snapshot_cycle_idx = (kAppCQESnapshotCycle - 1)
        mp_cqe_arr[i].wqe_counter = htons(kAppStridesPerWQE - (kAppRecvCQDepth - i));

        cqe_snapshot_t snapshot;
        snapshot_cqe(&mp_cqe_arr[i], snapshot);
        rt_assert(snapshot.get_cqe_snapshot_cycle_idx() == kAppCQESnapshotCycle - (kAppRecvCQDepth - i));
    }

    // The multi-packet RECVs. This must be done after we've initialized the CQE.
    struct ibv_sge* mp_sge = reinterpret_cast<struct ibv_sge*>(malloc(sizeof(struct ibv_sge) * kAppRQDepth));
    for (size_t i = 0; i < kAppRQDepth; i++) {
        size_t mpwqe_offset = i * (kAppRingMbufSize * kAppStridesPerWQE);
        mp_sge[i].addr = reinterpret_cast<uint64_t>(&mp_recv_ring[mpwqe_offset]);
        mp_sge[i].lkey = mp_mr->lkey;
        mp_sge[i].length = kAppRingMbufSize * kAppStridesPerWQE; //kAppRingSize;
        mp_wq_family->recv_burst(mp_wq, &mp_sge[i], 1);
    }

    printf("[Thread %d] Finish created QP - ", thread_id);
    printf("kAppRingMbufSize=%lu, kAppStridesPerWQE=%lu, kAppRingSize=%lu, kAppRQDepth=%lu\n", kAppRingMbufSize, kAppStridesPerWQE, kAppRingSize, kAppRQDepth);
    auto* cqe_arr = mp_cqe_arr;
    cqe_snapshot_t prev_snapshot;
    snapshot_cqe(&cqe_arr[kAppRecvCQDepth - 1], prev_snapshot);

    return new DMAcontext{
        .pd = pd,
        .ctx = context,
        .receive_cq = rec_cq,
        .send_cq = snd_cq,
        .send_mr = send_mr,
        .send_region = send_buf,
        .data_qp = qp,

        .mp_recv_qp = mp_recv_qp,
        .mp_recv_cq = mp_recv_cq,
        .mp_wq = mp_wq,
        .mp_wq_family = mp_wq_family,
        .mp_ind_tbl = mp_ind_tbl,
        .mp_cqe_arr = mp_cqe_arr,
        .mp_sge = mp_sge,
        .mp_recv_ring = mp_recv_ring,
        .mp_send_ring = mp_send_ring,
        .mp_send_mr = mp_send_mr,

        .id = thread_id,
        .total_received = 0,
        .total_sent = 0,
        .my_send_queue_length = my_send_queue_length,
        .my_recv_queue_length = my_recv_queue_length,

        .ring_head = 0,
        .nb_rx_rolling = 0,
        .sge_idx = 0,
        .cqe_idx = 0,
        .prev_snapshot = prev_snapshot,
        .isPS = isPS,
        .isMarkTimeStamp = false,
    };
}

void send_packet(DMAcontext* dma_context, int chunk_size, uint64_t offset)
{
    int ret;

    struct ibv_sge sg;
    struct ibv_exp_send_wr wr, *bad_wr;
    // struct ibv_send_wr wr;
    // struct ibv_send_wr *bad_wr;

    memset(&sg, 0, sizeof(sg));
    sg.addr = (uintptr_t)((char*)dma_context->send_region + offset * P4ML_LAYER_SIZE);
    // printf("%d\n", sg.addr);
    sg.length = chunk_size;
    sg.lkey = dma_context->send_mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.wr_id = 0;
    wr.sg_list = &sg;
    wr.num_sge = 1;
    // wr.opcode     = IBV_WR_SEND;
    wr.exp_opcode = IBV_EXP_WR_TSO;
    wr.tso.mss = P4ML_LAYER_SIZE; // Maximum Segment Size example
    wr.tso.hdr_sz = IP_ETH_UDP_HEADER_SIZE; // ETH/IPv4/TCP header example
    char hdr[IP_ETH_UDP_HEADER_SIZE]; // ETH/IPv4/TCP header example
    if (dma_context->isPS)
        memcpy(hdr, PS_IP_ETH_UDP_HEADER, IP_ETH_UDP_HEADER_SIZE); // Assuming that the header buffer was define before.
    else
        memcpy(hdr, WORKER_IP_ETH_UDP_HEADER, IP_ETH_UDP_HEADER_SIZE); // Assuming that the header buffer was define before.

    hdr[5] = dma_context->id;
    // hdr[37] = dma_context->id;
    wr.tso.hdr = hdr; // There is no need to use malloc operation in this case, local definition of hdr is ok.
        //wr.exp_send_flags = IBV_SEND_INLINE;
    wr.exp_send_flags |= IBV_SEND_SIGNALED;

    if (DEBUG_PRINT_ALL_SENDING_PACKET)
        for (int i = 0; i < chunk_size / P4ML_LAYER_SIZE; i++) 
            p4ml_header_print_h((agghdr*)((char *)sg.addr + i * P4ML_LAYER_SIZE), "SEND");

    // mark first time sending timestamp
    if (dma_context->isMarkTimeStamp) {
        std::chrono::high_resolution_clock::time_point current_time = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < chunk_size / P4ML_LAYER_SIZE; i++) {
            agghdr* p4ml_header = (agghdr*)((char*)sg.addr + i * P4ML_LAYER_SIZE);
            if (!dma_context->isSent[ntohs(p4ml_header->seq_num)]) {
                dma_context->isSent[ntohs(p4ml_header->seq_num)] = true;
                dma_context->first_send_time[ntohs(p4ml_header->seq_num)] = current_time;
            } else {
                /* Resend may trigger */
            }
        }
    }

    // we dont need to wait cq cause received represent sent
    ret = ibv_exp_post_send(dma_context->data_qp, &wr, &bad_wr);
    if (ret < 0) {
        fprintf(stderr, "failed in post send\n");
        exit(1);
    }

    struct ibv_wc wc_send_cq[POLLING_SIZE];
    ibv_poll_cq(dma_context->send_cq, POLLING_SIZE, wc_send_cq);
    if (DEBUG_CHECK_SEND_RECEIVE_TOTAL)
        dma_context->total_sent += chunk_size / P4ML_LAYER_SIZE;
}

size_t receive_packet(DMAcontext *dma_context, cqe_snapshot_t* new_snapshot)
{
    // cqe_snapshot_t new_snapshot;
    // cur_snapshot = new_snapshot;
    snapshot_cqe(&dma_context->mp_cqe_arr[dma_context->cqe_idx], *new_snapshot);
    const size_t delta = get_cycle_delta(dma_context->prev_snapshot, *new_snapshot);

    if (!(delta == 0 || delta >= kAppNumRingEntries)) {
        if (DEBUG_CHECK_SEND_RECEIVE_TOTAL)
            dma_context->total_received += delta;
        return delta;
    }
    else 
        return 0;
    // return delta;
}

void dma_postback(DMAcontext *dma_context)
{
    dma_context->ring_head = (dma_context->ring_head + 1) % kAppNumRingEntries;
    dma_context->nb_rx_rolling++;
    if (dma_context->nb_rx_rolling == kAppStridesPerWQE)
    {
        dma_context->nb_rx_rolling = 0;
        int ret = dma_context->mp_wq_family->recv_burst(dma_context->mp_wq, &dma_context->mp_sge[dma_context->sge_idx], 1);
        rt_assert(ret == 0);
        dma_context->sge_idx = (dma_context->sge_idx + 1) % kAppRQDepth;
    }
}

void dma_update_snapshot(DMAcontext *dma_context, cqe_snapshot_t new_snapshot)
{
    dma_context->prev_snapshot = new_snapshot;
    dma_context->cqe_idx = (dma_context->cqe_idx + 1) % kAppRecvCQDepth;
}

const char* ibv_wc_opcode_str(enum ibv_wc_opcode opcode)
{
    switch (opcode) {
    case IBV_EXP_WC_SEND:
        return "IBV_WC_SEND";
    case IBV_EXP_WC_RDMA_WRITE:
        return "IBV_WC_RDMA_WRITE";
    case IBV_EXP_WC_RDMA_READ:
        return "IBV_WC_RDMA_READ";
    case IBV_WC_COMP_SWAP:
        return "IBV_WC_COMP_SWAP";
    case IBV_WC_FETCH_ADD:
        return "IBV_WC_FETCH_ADD";
    case IBV_WC_BIND_MW:
        return "IBV_WC_BIND_MW";
        /* receive-side: inbound completion */
    case IBV_EXP_WC_RECV:
        return "IBV_WC_RECV";
    case IBV_EXP_WC_RECV_RDMA_WITH_IMM:
        return "IBV_WC_RECV_RDMA_WITH_IMM";
    default:
        return "IBV_WC_UNKNOWN";
    }
}

// Install a flow rule
void install_flow_rule(struct ibv_qp* qp, uint16_t thread_id, bool isPS)
{
    static constexpr size_t rule_sz = sizeof(ibv_exp_flow_attr) + sizeof(ibv_exp_flow_spec_eth) + sizeof(ibv_exp_flow_spec_ipv4_ext);

    uint8_t* flow_rule = new uint8_t[rule_sz];
    memset(flow_rule, 0, rule_sz);
    uint8_t* buf = flow_rule;

    auto* flow_attr = reinterpret_cast<struct ibv_exp_flow_attr*>(flow_rule);
    flow_attr->type = IBV_EXP_FLOW_ATTR_NORMAL;
    flow_attr->size = rule_sz;
    flow_attr->priority = 0;
    flow_attr->num_of_specs = 1;
    flow_attr->port = 1;
    flow_attr->flags = 0;
    flow_attr->reserved = 0;
    buf += sizeof(struct ibv_exp_flow_attr);

    // Ethernet - all wildcard
    auto* eth_spec = reinterpret_cast<struct ibv_exp_flow_spec_eth*>(buf);
    eth_spec->type = IBV_EXP_FLOW_SPEC_ETH;
    eth_spec->size = sizeof(struct ibv_exp_flow_spec_eth);
    buf += sizeof(struct ibv_exp_flow_spec_eth);

    const unsigned char R_SRC_MAC[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned char R_DST_MAC[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    if (isPS)
        memcpy(R_DST_MAC, PS_FILTER_TEMPLATE_R, sizeof(R_DST_MAC));
    else
        memcpy(R_DST_MAC, WORKER_FILTER_TEMPLATE_R, sizeof(R_DST_MAC));

    R_DST_MAC[5] = thread_id;
    
    const unsigned char R_SRC_MAC_MASK[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    const unsigned char R_DST_MAC_MASK[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    memcpy(eth_spec->val.dst_mac, R_DST_MAC, sizeof(R_DST_MAC));
    memcpy(eth_spec->val.src_mac, R_SRC_MAC, sizeof(R_SRC_MAC));
    memcpy(eth_spec->mask.dst_mac, R_DST_MAC_MASK, sizeof(R_DST_MAC_MASK));
    memcpy(eth_spec->mask.src_mac, R_SRC_MAC_MASK, sizeof(R_SRC_MAC_MASK));
    eth_spec->val.vlan_tag = 0;
    eth_spec->mask.ether_type = 0;

    rt_assert(ibv_exp_create_flow(qp, flow_attr) != nullptr);
}

// Install a UDP destination port--based flow rule
void install_udp_flow_rule(struct ibv_qp* qp, uint16_t dst_port)
{
    static constexpr size_t rule_sz = sizeof(ibv_exp_flow_attr) + sizeof(ibv_exp_flow_spec_eth) + sizeof(ibv_exp_flow_spec_ipv4_ext) + sizeof(ibv_exp_flow_spec_tcp_udp);

    uint8_t* flow_rule = new uint8_t[rule_sz];
    memset(flow_rule, 0, rule_sz);
    uint8_t* buf = flow_rule;

    auto* flow_attr = reinterpret_cast<struct ibv_exp_flow_attr*>(flow_rule);
    flow_attr->type = IBV_EXP_FLOW_ATTR_NORMAL;
    flow_attr->size = rule_sz;
    flow_attr->priority = 0;
    flow_attr->num_of_specs = 1;
    flow_attr->port = 1;
    flow_attr->flags = 0;
    flow_attr->reserved = 0;
    buf += sizeof(struct ibv_exp_flow_attr);

    // Ethernet - all wildcard
    auto* eth_spec = reinterpret_cast<struct ibv_exp_flow_spec_eth*>(buf);
    eth_spec->type = IBV_EXP_FLOW_SPEC_ETH;
    eth_spec->size = sizeof(struct ibv_exp_flow_spec_eth);
    buf += sizeof(struct ibv_exp_flow_spec_eth);

    // IPv4 - all wildcard
    auto* spec_ipv4 = reinterpret_cast<struct ibv_exp_flow_spec_ipv4_ext*>(buf);
    spec_ipv4->type = IBV_EXP_FLOW_SPEC_IPV4_EXT;
    spec_ipv4->size = sizeof(struct ibv_exp_flow_spec_ipv4_ext);
    buf += sizeof(struct ibv_exp_flow_spec_ipv4_ext);

    // UDP - match dst port
    auto* udp_spec = reinterpret_cast<struct ibv_exp_flow_spec_tcp_udp*>(buf);
    udp_spec->type = IBV_EXP_FLOW_SPEC_UDP;
    udp_spec->size = sizeof(struct ibv_exp_flow_spec_tcp_udp);
    udp_spec->val.dst_port = htons(dst_port);
    udp_spec->mask.dst_port = 0xffffu;
    udp_spec->mask.dst_port = 0;

    rt_assert(ibv_exp_create_flow(qp, flow_attr) != nullptr);
}

void snapshot_cqe(volatile mlx5_cqe64* cqe, cqe_snapshot_t& cqe_snapshot)
{
    while (true) {
        uint16_t wqe_id_0 = cqe->wqe_id;
        uint16_t wqe_counter_0 = cqe->wqe_counter;
        memory_barrier();
        uint16_t wqe_id_1 = cqe->wqe_id;

        if (likely(wqe_id_0 == wqe_id_1)) {
            cqe_snapshot.wqe_id = ntohs(wqe_id_0);
            cqe_snapshot.wqe_counter = ntohs(wqe_counter_0);
            return;
        }
    }
}

size_t get_cycle_delta(const cqe_snapshot_t& prev, const cqe_snapshot_t& cur)
{
    size_t prev_idx = prev.get_cqe_snapshot_cycle_idx();
    size_t cur_idx = cur.get_cqe_snapshot_cycle_idx();
    assert(prev_idx < kAppCQESnapshotCycle && cur_idx < kAppCQESnapshotCycle);

    return ((cur_idx + kAppCQESnapshotCycle) - prev_idx) % kAppCQESnapshotCycle;
}
