#include "p4ml_manager.h"

#define CHANGE_AGTR_ENABLE true
#define CC_ENABLE false
#define LOSS_RECOVERY_ENABLE true
#define LOSS_RECOVERY_LOG false

#define USE_HASH_AGTR_INDEX true

#define DEBUG_PER_PACKET_TIME false

uint32_t P4mlManager::host;
uint8_t P4mlManager::num_worker;
uint8_t P4mlManager::num_PS;
uint16_t P4mlManager::appID;
uint64_t P4mlManager::p4mlKey;
AppInfo* P4mlManager::app_info;

int P4mlManager::max_agtr_size_per_thread = 0;
int P4mlManager::UsedSwitchAGTRcount = 0;
int P4mlManager::_num_thread;

std::chrono::time_point<std::chrono::system_clock> P4mlManager::start;

ThreadInfo** P4mlManager::threadInfoQueue;
DMAcontext** P4mlManager::dmaContextQueue;
std::thread** P4mlManager::threadQueue;
std::thread** P4mlManager::pushPullthreadQueue;
std::thread** P4mlManager::quantizationthreadQueue;
std::queue<Job*>* P4mlManager::pushPulljobQueue;
std::queue<Job*>* P4mlManager::quantizejobQueue;
std::queue<Job*>* P4mlManager::dequantizejobQueue;
std::queue<agghdr*>* P4mlManager::pendingQueue;
uint64_t* P4mlManager::weightQueue;
std::queue<Job*> P4mlManager::finishQueue;
WindowManager* P4mlManager::window_manager;

HashTable* P4mlManager::hash_table;
int32_t** P4mlManager::quantizeBuffer;
bool** P4mlManager::isOverflow;

int finish_thread = 0;
int loop_times[56] = { 0 };

bool isSeen[20][1024001] = { 0 };
int forwardCounter[20] = { 0 };

float mean[1000];
float median[1000];

long long int p4ml_loss_packet[20] = { 0 };
long long int p4ml_total_packet[20] = { 0 };

bool P4mlManager::isForceForward = false;
int P4mlManager::forwardFrequency;
float P4mlManager::forwardRate;

std::mutex P4mlManager::_P4MLKey_mutex;
std::mutex P4mlManager::_print_mutex;
std::mutex P4mlManager::_queuePush_mutex;

std::chrono::high_resolution_clock::time_point recv_time;

int collision_time = 0;

P4mlManager::P4mlManager(uint32_t host, int num_worker, int appID, int num_PS)
{
    srand(time(NULL));
    this->host = host;
    this->p4mlKey = 0;
    this->appID = (uint16_t)appID;
    this->num_worker = (uint8_t)num_worker;
    this->num_PS = (uint8_t)num_PS;
    this->app_info = new AppInfo{
        .host = host,
        .appID = (uint16_t)appID,
        .num_worker = (uint8_t)num_worker,
        .num_PS = (uint8_t)num_PS
    };
}

uint64_t P4mlManager::GetNewKey()
{
    std::lock_guard<std::mutex> lock(_P4MLKey_mutex);
    return p4mlKey++;
}

int64_t P4mlManager::GetFinishKey()
{
    if (!finishQueue.empty()) {
        std::lock_guard<std::mutex> lock(_queuePush_mutex);
        Job* finish_job = finishQueue.front();
        uint64_t tmp_key = finish_job->key;
        // dequantizeAVX2((char*)finish_job->int_data, finish_job->len);
        finishQueue.pop();
        delete finish_job;
        return tmp_key;
    } else {
        return -1;
    }
}

double P4mlManager::GetLossRate()
{
    long long int loss = 0;
    long long int total = 0;
    for (int i = 0; i < _num_thread; i++) {
        loss += p4ml_loss_packet[i];
        total += p4ml_total_packet[i];
    }
    printf("Loss Rate: %lf\n", (double)loss / (double)total);
}

int P4mlManager::GetCollisionTimeAndClear()
{
    int tmp = collision_time;
    collision_time = 0;
    return tmp;
}

void P4mlManager::SetForceForward(float forward_rate)
{
    isForceForward = true;
    forwardRate = forward_rate;
    if (forward_rate == 0.75) {
        forwardFrequency = 4;
        printf("\n No 0.75 supported, exit.\n");
        exit(1);
    } else {
        forwardFrequency = 1 / forward_rate;
        printf("\nSet force forward, frequency = %d\n\n", forwardFrequency);
    }
}

void P4mlManager::SetMaxAgtrSizePerThread(int agtr)
{
    max_agtr_size_per_thread = agtr;
    printf("\nSet max_agtr_size_per_thread to %d...\n\n", agtr);
}

void P4mlManager::SetUsedSwitchAGTRcount(int used_agtr) 
{
    UsedSwitchAGTRcount = used_agtr;
    printf("\nSet used agtr to %d... (all agtr: %d)\n\n", used_agtr, MAX_AGTR_COUNT);
}

void P4mlManager::main_receive_packet_loop(DMAcontext* dma_context,
    int32_t* data,
    int my_id)
{
    int msgs_completed;
    int total_resent = 0;
    int total_loss = 0;
    int total_dup_packet = 0;
    int total_last_tensor_packet = 0;
    int this_pos_to_send = max_agtr_size_per_thread;
    int resend_pos_to_send = dma_context->my_send_queue_length / 2;
    int total_packet = window_manager[my_id].total_ACK;
    int rand_index = 0;

    float timeout_value = 0.05;

    int window = max_agtr_size_per_thread;
    /* Loss simulation */
    int loss = 0;
    int resend_loss = 0;
    int timeout_loss = 0;

    int send_pointer = max_agtr_size_per_thread;

    int last_pending_front = 0;
    int pending_front_stuck_time = 0;

    int resend_waiting = 0;

    int finish_window_seq = max_agtr_size_per_thread;
    CC_manager cc_manager(max_agtr_size_per_thread);

    loop_times[my_id]++;
    // if(loop_times[my_id] % 1000 == 0)
    //     fprintf(stderr, "loop_times[ %d ]  %d finished\n", my_id, loop_times[my_id]);

    char* send_region = (char*)dma_context->send_region;

    while (window_manager[my_id].last_ACK < window_manager[my_id].total_ACK) {
        cqe_snapshot_t cur_snapshot;
        msgs_completed = 0;
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        while (1) {
            std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            msgs_completed = receive_packet(dma_context, &cur_snapshot);

            if (DEBUG_PER_PACKET_TIME)
                recv_time = std::chrono::high_resolution_clock::now();
            
            if (msgs_completed) {    
                break;
            }

            if (LOSS_RECOVERY_ENABLE) {
                if (time_span.count() > timeout_value && msgs_completed == 0) {
                    uint16_t timeout_end_seq;

                    if (!pendingQueue[my_id].empty()) {
                        agghdr *pending_p4ml_header = pendingQueue[my_id].front();
                        timeout_end_seq = pending_p4ml_header->seq_num;
                    } else {
                        timeout_end_seq = window_manager[my_id].last_ACK + 1;
                        timeout_end_seq = timeout_end_seq + 1;
                        // printf("last ack: %d\n", window_manager[my_id].last_ACK);
                        // exit(1);
                    }

                    if (resend_pos_to_send > dma_context->my_send_queue_length - 2 * max_agtr_size_per_thread)
                        resend_pos_to_send = dma_context->my_send_queue_length / 2 + 1;
                    int resent_to_be_sent = 0;

                    if (LOSS_RECOVERY_LOG)
                        printf("[Thread %d] Timeout, %d~%d Resend!, LASTACK: %d\n", my_id, window_manager[my_id].last_ACK + 1, timeout_end_seq - 1, window_manager[my_id].last_ACK);

                    for (uint16_t timeout_seq = window_manager[my_id].last_ACK + 1; timeout_seq < timeout_end_seq; timeout_seq++) {
                        int offset = (timeout_seq - 1) * MAX_ENTRIES_PER_PACKET;
                        uint16_t switch_agtr_pos = hash_table->hash_map[threadInfoQueue[my_id]->agtr_start_pos + ((timeout_seq - 1) % max_agtr_size_per_thread)];
                        if (timeout_seq <= total_packet) {
                            // exit(1);
                            // set Terminated if last packet
                            make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send + resent_to_be_sent) * P4ML_LAYER_SIZE, pushPulljobQueue[my_id].front(), app_info, &switch_agtr_pos, &timeout_seq, &offset, true, false, isOverflow[my_id][timeout_seq]);
                            // p4ml_header_print_h((agghdr*)((char*)send_region + (resend_pos_to_send + resent_to_be_sent) * P4ML_LAYER_SIZE), "TIMEOUT TRIGGER");
                            resent_to_be_sent++;
                            total_resent++;

                            p4ml_loss_packet[my_id]++;
                        }
                    }

                    send_packet(dma_context, P4ML_LAYER_SIZE * resent_to_be_sent, resend_pos_to_send);
                    resend_pos_to_send += resent_to_be_sent;

                    t1 = std::chrono::high_resolution_clock::now();
                }
            }

            if (time_span.count() > 20.0 && msgs_completed == 0 && dma_context->total_received > 0) {
                fprintf(stderr, "Timeout happened this thread thread_id=%d, total_received=%d, total_sent=%d, total_loss=%d, total_resent=%d, last_ACK=%d, total_dup_recv=%d, total_last_tensor_packet_recv=%d, loop_time=%d\n",
                    my_id, dma_context->total_received, dma_context->total_sent, total_loss, total_resent, window_manager[my_id].last_ACK, total_dup_packet, total_last_tensor_packet, loop_times[my_id]);

                for (int i = 0; i < _num_thread; i++)
                    fprintf(stderr, "Timeout happened  thread_id=%d, total_received=%d, total_sent=%d, loop_time=%d\n", i, dmaContextQueue[i]->total_received, dmaContextQueue[i]->total_sent, loop_times[i]);

                exit(-1);
            }
        }

        timeout_value = 0.0002;
        /* circle alignment */
        if (this_pos_to_send + max_agtr_size_per_thread + max_agtr_size_per_thread > dma_context->my_send_queue_length / 2)
            this_pos_to_send = 0;

        int to_be_sent = 0;

        // printf("msgs_completed: %d, dma_context->total_received: %d\n", msgs_completed, dma_context->total_received);

        for (int msg = 0; msg < msgs_completed; msg++) {

            
            uint8_t* buf = &dma_context->mp_recv_ring[dma_context->ring_head * kAppRingMbufSize];

            agghdr* p4ml_header = reinterpret_cast<agghdr*>(buf + IP_ETH_UDP_HEADER_SIZE);
            if (DEBUG_PRINT_ALL_RECEIVING_PACKET)
                p4ml_header_print_h(p4ml_header, "RECEIVE");

            p4ml_header_ntoh_without_data(p4ml_header);
            bool is_ACK_packet = p4ml_header->flag & 0x01;
            bool is_resend_packet = p4ml_header->flag & 0x04;
            bool is_ecn_mark_packet = p4ml_header->flag & 0x08;
            bool is_collision_packet = p4ml_header->flag & 0x02;
            bool is_overflow_packet = p4ml_header->flag & 0x80;

            // If that is resend packet from last tensor, ignore it
            if (p4ml_header->key != pushPulljobQueue[my_id].front()->key) {
                total_last_tensor_packet++;
                dma_context->total_received--;

                dma_context->ring_head = (dma_context->ring_head + 1) % kAppNumRingEntries;
                dma_context->nb_rx_rolling++;
                if (dma_context->nb_rx_rolling == kAppStridesPerWQE) {
                    dma_context->nb_rx_rolling = 0;
                    int ret = dma_context->mp_wq_family->recv_burst(dma_context->mp_wq, &dma_context->mp_sge[dma_context->sge_idx], 1);
                    rt_assert(ret == 0);
                    dma_context->sge_idx = (dma_context->sge_idx + 1) % kAppRQDepth;
                }
                continue;
            }
            // If that is duplicate resend packet, ignore it
            if (window_manager[my_id].isACKed[p4ml_header->seq_num] && is_resend_packet) {
                total_dup_packet++;
                dma_context->total_received--;
                
                dma_postback(dma_context);
                continue;
            }

            // printf("packet %d receive\n", p4ml_header->seq_num);

            int isOverflowRequest = false;

            if (is_overflow_packet && is_ACK_packet && !is_resend_packet) {
                isOverflow[my_id][p4ml_header->seq_num] = 1;
                isOverflowRequest = true;
            }

            /* Receive Normal Packet */
            if (!window_manager[my_id].isACKed[p4ml_header->seq_num] && !isOverflowRequest) {
                if (is_collision_packet) {
                    collision_time++;
                    printf("collision???\n");
                    if (CHANGE_AGTR_ENABLE) {
                        /* Change agtr */
                        uint16_t new_agtr = p4ml_header->len_tensor;
                        if (!hash_table->isAlreadyDeclare[new_agtr]) {
                            // printf("old: %d, new: %d\n", p4ml_header->agtr, p4ml_header->len_tensor);
                            int hash_table_index = threadInfoQueue[my_id]->agtr_start_pos + ((p4ml_header->seq_num - 1) % max_agtr_size_per_thread);
                            hash_table->hash_map[hash_table_index] = p4ml_header->len_tensor;
                        } else {
                            //ignore it.
                        }

                    }
                }

                dma_context->first_receive_time[p4ml_header->seq_num] = recv_time;
                

                /* Update Model */
                if (!is_overflow_packet) 
                    for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                        p4ml_header->vector[i] = ntohl(p4ml_header->vector[i]);
                updateModel(p4ml_header, data, my_id);
                

                // if (is_overflow_packet) {
                //     updateModel(p4ml_header, (int32_t*) pushPulljobQueue[my_id].front()->float_data, my_id);
                // } else {
                //     for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                //         p4ml_header->vector[i] = ntohl(p4ml_header->vector[i]);
                //     updateModel(p4ml_header, pushPulljobQueue[my_id].front()->data, my_id);
                // }

                /* Update Window */
                window_manager[my_id].UpdateWindow(&p4ml_header->seq_num);

                /* If not Ready for next Seq sending, Enqueue */
                uint16_t next_seq_num = p4ml_header->seq_num + window;
                int next_offset = (next_seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                // printf("next_seq_num: %d\n", next_seq_num);

                if (next_seq_num > window_manager[my_id].last_ACK + window) {
                    // printf("%d: next seq_num: %d enqueue! [%d ~ %d] \n", p4ml_header->seq_num, next_seq_num, window_manager[my_id].last_ACK, window_manager[my_id].last_ACK + window);
                    agghdr* enqueue_header = (agghdr*)malloc(sizeof(agghdr));
                    memcpy(enqueue_header, p4ml_header, sizeof(agghdr));
                    pendingQueue[my_id].push(enqueue_header);
                }

                /* Send Next Packet */
                if (next_seq_num <= total_packet && next_seq_num <= window_manager[my_id].last_ACK + window && next_seq_num > send_pointer) {
                    // printf("next_seq_num: %d, send_pointer: %d\n", next_seq_num, send_pointer);

                    bool ForceForward = false;
                    if (isForceForward) {
                        if (forwardCounter[my_id] + 1 == forwardFrequency) {
                            ForceForward = true;
                            forwardCounter[my_id] = 0;
                        } else {
                            forwardCounter[my_id]++;
                        }
                    }
                    
                    int packet_to_process = next_seq_num - send_pointer;
                    // printf("packet to process: %d\n", packet_to_process);
                    // send more packet if window change
                    for (int i = packet_to_process - 1; i >= 0; i--) {
                        uint16_t process_next_seq_num = next_seq_num - i;

                        // printf("[%d] next_seq_num: %d, send_pointer: %d\n", p4ml_header->seq_num, process_next_seq_num, send_pointer);

                        uint16_t switch_agtr_pos = hash_table->hash_map[threadInfoQueue[my_id]->agtr_start_pos + ((process_next_seq_num - 1) % max_agtr_size_per_thread)];

                        // set Terminated if last packet
                        make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, pushPulljobQueue[my_id].front(), app_info, &switch_agtr_pos, &process_next_seq_num, &next_offset, false, ForceForward, isOverflow[my_id][process_next_seq_num]);
                        to_be_sent++;
                    }
                    send_pointer = next_seq_num;
                    // printf("send_pointer: %d\n", send_pointer);
                }

                int i = 0;
                /* Check If packet in Pending Queue is Ready to send */
                while (!pendingQueue[my_id].empty()) {
                    // printf("p4ml_header->seq_num: %d\n", p4ml_header->seq_num);
                    agghdr *pending_p4ml_header = pendingQueue[my_id].front();

                    i++;

                    if (window_manager[my_id].last_ACK < pending_p4ml_header->seq_num) {
                        // printf("%d Get %d from pending queue and window_manager[my_id].last_ACK = %d, quit\n", p4ml_header->seq_num, pending_p4ml_header->seq_num, window_manager[my_id].last_ACK);
                        break;
                    }

                    uint16_t next_seq_num = pending_p4ml_header->seq_num + window;
                    int next_offset = (next_seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                    // printf("[pending] org: %d, next_number: %d\n", pending_p4ml_header->seq_num, next_seq_num);
                    if (next_seq_num <= window_manager[my_id].last_ACK + window && next_seq_num > send_pointer) {

                        bool ForceForward = false;
                        if (isForceForward) {
                            if (forwardCounter[my_id] + 1 == forwardFrequency) {
                                ForceForward = true;
                                forwardCounter[my_id] = 0;
                            } else {
                                forwardCounter[my_id]++;
                            }
                        }

                        /* Copy to Send Region */
                        if (next_seq_num <= total_packet) {
                            int packet_to_process = abs(next_seq_num - send_pointer);
                            // printf("[pending] packet to process: %d\n", packet_to_process);

                            for (int i = packet_to_process - 1; i >= 0; i--) {
                                uint16_t process_next_seq_num = next_seq_num - i;
                                // printf("[Pending] seq_num trigger %d send next seq_num: %d\n", p4ml_header->seq_num, next_seq_num);
                                uint16_t switch_agtr_pos = hash_table->hash_map[threadInfoQueue[my_id]->agtr_start_pos + ((process_next_seq_num - 1) % max_agtr_size_per_thread)];

                                make_p4ml_layer_and_copy_to(send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, pushPulljobQueue[my_id].front(), app_info, &switch_agtr_pos, &process_next_seq_num, &next_offset, false, ForceForward, isOverflow[my_id][process_next_seq_num]);

                                to_be_sent++;
                            }
                            send_pointer = next_seq_num;
                            // printf("[pending] send_pointer: %d\n", send_pointer);
                        }
                        free(pending_p4ml_header);
                        pendingQueue[my_id].pop();
                    } else {
                        free(pending_p4ml_header);
                        pendingQueue[my_id].pop();
                    }
                }

                /* If force forward is configurated, expect will not packet loss */
                if (!isForceForward && LOSS_RECOVERY_ENABLE) {
                    if (!pendingQueue[my_id].empty()) {
                        agghdr *pending_p4ml_header = pendingQueue[my_id].front();
    
                        // printf("%d ~ %d [LASTACK %d]\n", window_manager[my_id].last_ACK + 1, pending_p4ml_header->seq_num, window_manager[my_id].last_ACK);
                        if (window_manager[my_id].last_ACK + 1 > resend_waiting) {

                            if (resend_pos_to_send > dma_context->my_send_queue_length - 2 * max_agtr_size_per_thread)
                                resend_pos_to_send = dma_context->my_send_queue_length / 2 + 1;
                            int resent_to_be_sent = 0;

                            if (LOSS_RECOVERY_LOG)
                                printf("[Thread %d] Seq %d trigger %d~%d Resend!\n", my_id, p4ml_header->seq_num, window_manager[my_id].last_ACK + 1, pending_p4ml_header->seq_num-1);

                            for (uint16_t resend_seq = window_manager[my_id].last_ACK + 1; resend_seq < pending_p4ml_header->seq_num; resend_seq++) {
                                int offset = (resend_seq - 1) * MAX_ENTRIES_PER_PACKET;
                                uint16_t switch_agtr_pos = hash_table->hash_map[threadInfoQueue[my_id]->agtr_start_pos + ((resend_seq - 1) % max_agtr_size_per_thread)];
                                if (resend_seq <= total_packet) {
                                    // exit(1);
                                    make_p4ml_layer_and_copy_to(send_region + (resend_pos_to_send + resent_to_be_sent) * P4ML_LAYER_SIZE, pushPulljobQueue[my_id].front(), app_info, &switch_agtr_pos, &resend_seq, &offset, true, false, isOverflow[my_id][resend_seq]);
                                    // p4ml_header_print(p4ml_header, "RESEND TRIGGER");
                                    resent_to_be_sent++;
                                    total_resent++;

                                    p4ml_loss_packet[my_id]++;
                                }
                                resend_waiting = resend_seq;
                            }
                            send_packet(dma_context, P4ML_LAYER_SIZE * resent_to_be_sent, resend_pos_to_send);
                            resend_pos_to_send += resent_to_be_sent;
                        }
                    }
                }

                if (CC_ENABLE) {
                    if (p4ml_header->seq_num == finish_window_seq) {
                        int new_window = cc_manager.adjustWindow(is_ecn_mark_packet);
                        if (send_pointer + new_window > window_manager[my_id].total_ACK)
                            window = window_manager[my_id].total_ACK - send_pointer;
                        else
                            window = new_window;
                        finish_window_seq += window;
                    }
                }
            }

            dma_postback(dma_context);
        }

        dma_update_snapshot(dma_context, cur_snapshot);

        if (msgs_completed < 0) {
            printf("Polling error\n");
            exit(1);
        }

        if (msgs_completed > 0 && to_be_sent) {
            send_packet(dma_context, P4ML_LAYER_SIZE * to_be_sent, this_pos_to_send);
            this_pos_to_send += to_be_sent;
        }
    }

    dma_context->total_sent = 0;
    dma_context->total_received = 0;
    
    if (!pendingQueue[my_id].empty()) {
        printf("PENDING QUEUE NOT EMPTY AFTER DONE.\n");
        while (!pendingQueue[my_id].empty()) {
            printf("%d ", pendingQueue[my_id].front()->seq_num);
            pendingQueue[my_id].pop();
        }
        printf("pendingQueue[my_id].size: %zu\n", pendingQueue[my_id].size());
        exit(1);
    }


    // if (my_id == 0)
    // fprintf(stderr, "[Finish log] thread_id=%d, total_received=%d, total_sent=%d, total_loss=%d, total_resent=%d, last_ACK=%d, total_dup_recv=%d, total_last_tensor_packet_recv=%d\n", \
    my_id, dma_context->total_received, dma_context->total_sent, total_loss, total_resent, window_manager[my_id].last_ACK, total_dup_packet, total_last_tensor_packet);
}

void P4mlManager::PushPull(uint64_t key, char* float_data, int len, int cmd)
{
    Job* job = new Job{
        .key = key,
        .float_data = (float*) float_data,
        .int_data = NULL,
        .len = (uint32_t)len,
        .cmd = cmd
    };

    /* Load Balance */
    uint64_t smallestWeight = weightQueue[0];
    int queueToGo = 0;
    for (int i = 1; i < _num_thread; i++) {
        if (weightQueue[i] < smallestWeight) {
            smallestWeight = weightQueue[i];
            queueToGo = i;
        }
    }

    /* If someone nearly overflow, all minus the smallest one */
    if (weightQueue[queueToGo] > UINT64_MAX - 2 * MAX_TENSOR_SIZE)
        for (int i = 0; i < _num_thread; i++)
            weightQueue[i] = weightQueue[i] - weightQueue[queueToGo];

    weightQueue[queueToGo] += len;
    // printf("% "PRIu64" \n", weightQueue[queueToGo]);
    // printf("Job %d Get, Send to Queue %d.\n", key, queueToGo);
    
    if (FLOATING_POINT_INPUT) 
        job->int_data = quantizeBuffer[queueToGo];
    else 
        job->int_data = (int32_t*) job->float_data;

    // pushPulljobQueue[queueToGo].push(job);
    quantizejobQueue[queueToGo].push(job);
}

// void P4mlManager::PushTaskToThread(uint64_t key, char *data, int len, int cmd, int queueToGo) 
// {
//     Job *job = new Job{
//         .key = key,
//         .float_data = (float*) float_data,
//         .data = (int32_t *)data,
//         .len = (uint32_t)len,
//         .cmd = cmd
//     };

//     // printf("% "PRIu64" \n", weightQueue[queueToGo]);
//     // printf("Job %d Get, Send to Queue %d.\n", key, queueToGo);
//     pushPulljobQueue[queueToGo].push(job);
// }

void P4mlManager::init_threadPool(int num_thread)
{
    _num_thread = num_thread;
    /* Let fix each thread use 800 agtr */
    if (!max_agtr_size_per_thread)
        max_agtr_size_per_thread = 100;
    if (!UsedSwitchAGTRcount)
        UsedSwitchAGTRcount = MAX_AGTR_COUNT;
    printf("max_agtr_size_per_thread: %d\n", max_agtr_size_per_thread);
    printf("used agtr: %d (all:%d) \n", UsedSwitchAGTRcount, MAX_AGTR_COUNT);
    threadInfoQueue = new ThreadInfo*[num_thread];
    dmaContextQueue = new DMAcontext*[num_thread];
    weightQueue = new uint64_t[num_thread]();
    threadQueue = new std::thread*[num_thread];
    pushPullthreadQueue = new std::thread*[num_thread];
    quantizationthreadQueue = new std::thread*[num_thread];
    pushPulljobQueue = new std::queue<Job*>[num_thread];
    quantizejobQueue = new std::queue<Job*>[num_thread];
    dequantizejobQueue = new std::queue<Job*>[num_thread];
    pendingQueue = new std::queue<agghdr*>[num_thread];
    window_manager = new WindowManager[num_thread];

    hash_table = new HashTable(UsedSwitchAGTRcount);
    quantizeBuffer = new int32_t*[num_thread];
    isOverflow = new bool*[num_thread];

    // Start from zero
    for (int i = 0; i < num_thread * max_agtr_size_per_thread; i++)  {
        if (USE_HASH_AGTR_INDEX)
            hash_table->HashNew_crc(appID, i);
        else 
            hash_table->HashNew_linear(i);
        // printf("[%d] %d \n", i, hash_table->hash_map[i]);
    }

    struct ibv_device** dev_list;
    struct ibv_device* ib_dev;
    dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        perror("Failed to get devices list");
        exit(1);
    }

    ib_dev = dev_list[1];
    if (!ib_dev) {
        fprintf(stderr, "IB device not found\n");
        exit(1);
    }

    for (int i = 0; i < num_thread; i++) {
        threadInfoQueue[i] = new ThreadInfo{
            .thread_id = i,
            .agtr_start_pos = max_agtr_size_per_thread * i,
        };
        dmaContextQueue[i] = DMA_create(ib_dev, i + ((appID - 1) * MAX_THREAD_PER_APP), false);
        pushPullthreadQueue[i] = new std::thread(PushPullLoop, i);
        quantizationthreadQueue[i] = new std::thread(QuantizationLoop, i);
        window_manager[i].isACKed = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        dmaContextQueue[i]->isSent = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        dmaContextQueue[i]->first_send_time = new std::chrono::high_resolution_clock::time_point[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        dmaContextQueue[i]->first_receive_time = new std::chrono::high_resolution_clock::time_point[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];

        quantizeBuffer[i] = new int32_t[MAX_TENSOR_SIZE];
        isOverflow[i] = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];

        if (DEBUG_PER_PACKET_TIME)
            dmaContextQueue[i]->isMarkTimeStamp = true;
    }

    printf("using: %s\n", ibv_get_device_name(ib_dev));
}

void P4mlManager::PushPullLoop(int thread_id)
{
    bindingCPU(thread_id + 1);
    ThreadInfo* thread_info = threadInfoQueue[thread_id];
    DMAcontext* dma_context = dmaContextQueue[thread_id];
    int agtr_size = max_agtr_size_per_thread;
    int my_id = thread_id;
    int agtr_start_pos = thread_info->agtr_start_pos;
    char* send_region = (char*)dma_context->send_region;

    while (1) {
        if (!pushPulljobQueue[thread_id].empty()) {
            Job* job = pushPulljobQueue[thread_id].front();
            uint64_t key = job->key;
            float* float_data = job->float_data;
            int32_t* data = job->int_data;
            uint32_t tensor_len = job->len;

            // if (ONLY_DO_QUAN) quantizeAVX2((char*)job->int_data, tensor_len);

            int total_packet = ceil((float)tensor_len / MAX_ENTRIES_PER_PACKET);
            p4ml_total_packet[my_id] += total_packet;
            window_manager[thread_id].Reset(total_packet);
            memset(dma_context->isSent, 0, sizeof(bool) * window_manager[my_id].total_ACK + 1);

            //     if (thread_id == 0){
            // for (int i = 0; i < 1000; i++) 
            //     printf("%.7f ", float_data[i]);
            // printf("\n##########################\n");
            // }
            memset(isOverflow[thread_id], 0, sizeof(bool) * total_packet + 1);
            // Floating point -> Integer
            if (FLOATING_POINT_INPUT) { 
                quantizeAVX2to((char *)data, (char *)float_data, tensor_len);
                // quantizeAVX2((char *)data, tensor_len);

                /* Check Overflow */
                for (int i = 0; i < total_packet; i++) {
                    for (int j = 0; j < MAX_ENTRIES_PER_PACKET; j++) {
                        // printf("float_data[%d] overflow: %f\n", i * MAX_ENTRIES_PER_PACKET + j, float_data[i * MAX_ENTRIES_PER_PACKET + j]);
                        if (i * MAX_ENTRIES_PER_PACKET + j < MAX_TENSOR_SIZE &&
                            (float_data[i * MAX_ENTRIES_PER_PACKET + j] > OVERFLOW_THRESHOLD || 
                            float_data[i * MAX_ENTRIES_PER_PACKET + j] < UNDERFLOW_THRESHOLD)) {
                            //Seq Num
                            isOverflow[thread_id][i+1] = 1;
                            // printf("%d: float_data[%d] overflow: %.7f\n", i+1, i * MAX_ENTRIES_PER_PACKET + j, float_data[i * MAX_ENTRIES_PER_PACKET + j]);
                            continue;
                        }
                    }
                }
            }

            // for (int i = 0; i < 1000; i++)
            //     printf("%d ", data[i]);
            // printf("\n##########################\n");
            // exit(1);
            // for (int i = 0; i < 1000; i++) 
            //     printf("%d ", data[i]);
            // // exit(1);
            // // fprintf(stderr, "%lld: thread_id=%d, tensor_len=%d, agg_size=%d\n", key, thread_id, tensor_len, agtr_size);
            // printf("\n##########################\n");
            for (int i = 0; i < tensor_len; i++)
                data[i] = htonl(data[i]);

            // SEQ number start from 1
            uint16_t seq_num = 0;

            int num_first_time_sending;
            if (max_agtr_size_per_thread * MAX_ENTRIES_PER_PACKET > tensor_len)
                num_first_time_sending = ceil((float)tensor_len / MAX_ENTRIES_PER_PACKET);
            else
                num_first_time_sending = max_agtr_size_per_thread;

            // the first round sending
            for (int i = 0; i < num_first_time_sending; i++) {
                seq_num++;
                int offset = (seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                uint16_t switch_agtr_pos = hash_table->hash_map[agtr_start_pos + i];
                // This thread have first time sending
                if (seq_num <= total_packet) {

                    bool ForceForward = false;
                    if (isForceForward) {
                        if (forwardCounter[my_id] + 1 == forwardFrequency) {
                            ForceForward = true;
                            forwardCounter[my_id] = 0;
                        } else {
                            forwardCounter[my_id]++;
                        }
                    }
                    
                    make_p4ml_layer_and_copy_to(send_region + P4ML_LAYER_SIZE * i, pushPulljobQueue[my_id].front(), app_info, &switch_agtr_pos, &seq_num, &offset, false, ForceForward, isOverflow[my_id][seq_num]);
                } else {
                }
            }

            for (int j = 0; j < num_first_time_sending; j++) {
                send_packet(dma_context, P4ML_LAYER_SIZE, j);
            }

            // send_packet(dma_context, P4ML_LAYER_SIZE * num_first_time_sending, 0);

            main_receive_packet_loop(dma_context, data, my_id);

            /* For Per Packet */
            if (DEBUG_PER_PACKET_TIME && thread_id == 0) {
                double total_time = 0.0;
                double time[32001] = {0.0};
                double min_value = 100000.0;
                for (int i = 1; i <= total_packet; i++) {
                    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(dma_context->first_receive_time[i] - dma_context->first_send_time[i]);
                    time[i] = time_span.count();
                    // printf("%d: %lf\n", i, time[i]);
                    total_time += time[i];
                    if (time[i] < min_value)
                        min_value = time[i];
                }
                std::sort(time+1, time+32001);
                mean[loop_times[my_id]] = total_time/32000.0;
                median[loop_times[my_id]] = time[16000];
                printf("mean: %lf, median: %lf, min_value: %lf\n", total_time/32000.0, time[16000], min_value);
            }
            // printf("\n##########################\n");
            // for (int i = 0; i < 620; i++)
            //     printf("%d ", data[i]);
            // printf("\n##########################\n");
            if (FLOATING_POINT_INPUT) {
                dequantizeAVX2to((char*)float_data, (char *)data, tensor_len);

                for (int i = 1; i <= total_packet; i++) {
                    if (isOverflow[thread_id][i]) {
                        if ( i * MAX_ENTRIES_PER_PACKET <= tensor_len)
                            memcpy(float_data + MAX_ENTRIES_PER_PACKET * (i-1), data + MAX_ENTRIES_PER_PACKET * (i-1), sizeof(int32_t) * MAX_ENTRIES_PER_PACKET);
                        else
                            memcpy(float_data + MAX_ENTRIES_PER_PACKET * (i-1), data + MAX_ENTRIES_PER_PACKET * (i-1), sizeof(int32_t) * (tensor_len % MAX_ENTRIES_PER_PACKET));
                    }
                }
                for (int i = 0; i < tensor_len; i++) {
                    if (!(abs(float_data[i] - ((i+1.0) / 10000000.0) * 2) < 0000001 || (float_data[i] >= 100 && float_data[i] <= 400)))  {
                        printf("[thread %d - loop time %d] %d: %.7f %.7f [%.7f]\n", my_id, loop_times[my_id], i, float_data[i], ((i+1.0) / 10000000.0) * 2, abs(float_data[i] - ((i+1.0) / 10000000.0) * 2));
                        exit(1);
                    }
                }
            }

            // if (ONLY_DO_QUAN) dequantizeAVX2((char*)data, tensor_len);
            // {
            //     std::lock_guard<std::mutex> lock(_queuePush_mutex);
            //     // printf("%d to Finish Queue\n", key);
            //     finishQueue.push(pushPulljobQueue[thread_id].front());
            // }
            // pushPulljobQueue[thread_id].pop();
            dequantizejobQueue[thread_id].push(pushPulljobQueue[thread_id].front());
            pushPulljobQueue[thread_id].pop();
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
    }
}

void P4mlManager::QuantizationLoop(int thread_id) {
    int my_id = thread_id;

    while (1) {
        if (!dequantizejobQueue[thread_id].empty()) {
            Job *job = dequantizejobQueue[thread_id].front();
            uint64_t key = job->key;
            float *float_data = job->float_data;
            int32_t *data = job->int_data;
            uint32_t tensor_len = job->len;

            if (ONLY_DO_QUAN) dequantizeNaive((char*)data, tensor_len);

            {
                std::lock_guard<std::mutex> lock(_queuePush_mutex);
                // printf("%d to Finish Queue\n", key);
                finishQueue.push(dequantizejobQueue[thread_id].front());
            }
            dequantizejobQueue[thread_id].pop();
        }

        if (!quantizejobQueue[thread_id].empty()) {
            Job *job = quantizejobQueue[thread_id].front();
            uint64_t key = job->key;
            float *float_data = job->float_data;
            int32_t *data = job->int_data;
            uint32_t tensor_len = job->len;

            if (ONLY_DO_QUAN) quantizeNaive((char*)job->int_data, tensor_len);

            pushPulljobQueue[thread_id].push(quantizejobQueue[thread_id].front());
            quantizejobQueue[thread_id].pop();
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
    }
}
