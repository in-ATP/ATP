#include "ParameterServer.h"

tensor_context *tensors;

int max_agtr_size_per_thread;
int UsedSwitchAGTRcount = MAX_AGTR_COUNT;
std::mutex _dma_mutex;
struct ibv_device **dev_list;
struct ibv_device *ib_dev;
ThreadPool* workQueue;
std::mutex __print_mutex;
std::mutex _init_mutex;
int num_thread;
int print_count = 0;
int appID;

long long int receive_in_sec[20] = {0};
bool receive_byte_reset_flag[20] = {0};

bool is_completed_p4ml_key[1024000] = {0};

int next_agtr[MAX_AGTR_COUNT] = {-1};
HashTable* hash_table;

int packet_full_count = 0;
int packet_partial_count = 0;
int packet_all_forward_count = 0;
int packet_partial_total_count = 0;

#define MAX_MEASUREMENT_KEY 12000
int full_packet_count[MAX_MEASUREMENT_KEY][16518] = { 0 };
int resend_packet_count[MAX_MEASUREMENT_KEY][16518] = { 0 };


DMAcontext** global_dma_contexts;

void main_receive_packet_loop(DMAcontext* dma_context, int thread_id) {
    int msgs_completed = 0;
    int this_pos_to_send = 0;
    int total_last_tensor_packet = 0;
    int imm_pos_to_send = dma_context->my_send_queue_length / 2;
    bool app_init[MAX_APP_PER_THREAD] = {0};
    
    /* Loss */
    int loss = 0;

    int rand_index = 0;
    int total_loss = 0;

    // app start from 1
    int* tensors_pos_of_app = new int[MAX_APP_PER_THREAD + 1];
    for (int i = 1; i <= MAX_APP_PER_THREAD; i++) {
        tensors_pos_of_app[i] = thread_id * MAX_STORAGE_PER_APP_PER_THREAD * MAX_APP_PER_THREAD + (i - 1) * MAX_STORAGE_PER_APP_PER_THREAD;
    }


    while (1) {

        cqe_snapshot_t cur_snapshot;
        msgs_completed = 0;
        
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        while(1) {

            // if (receive_byte_reset_flag[thread_id]) {
            //     receive_in_sec[thread_id] = 0;
            //     receive_byte_reset_flag[thread_id] = false;
            // }
            
            std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
            
            msgs_completed = receive_packet(dma_context, &cur_snapshot);
            if (msgs_completed) {    
                break;
            }
            if (time_span.count() > 20.0 && msgs_completed == 0 && dma_context->total_received > 0) {
                std::lock_guard<std::mutex> lock(_dma_mutex);
                fprintf(stderr, "Timeout happened this thread_id=%d, total_received=%d, total_sent=%d, last_ACK=%d, total_last_tensor_packet_recv=%d\n",
                    thread_id, global_dma_contexts[thread_id]->total_received, global_dma_contexts[thread_id]->total_sent, tensors[tensors_pos_of_app[1]].window_manager[0].last_ACK, total_last_tensor_packet);
                for (int i = 0; i < num_thread; i++)
                    fprintf(stderr, "Timeout happened at thread_id=%d, total_received=%d, total_sent=%d\n", i, global_dma_contexts[i]->total_received, global_dma_contexts[i]->total_sent);

                for (uint64_t i = 0; i < MAX_MEASUREMENT_KEY; i++) {
                    for (uint16_t j = 1; j <= ceil((float)MAX_TENSOR_SIZE/MAX_ENTRIES_PER_PACKET); j++) {
                        if (full_packet_count[i][j]) {
                            packet_full_count++;
                        } else if (resend_packet_count[i][j]) {
                            packet_partial_count++;
                            packet_partial_total_count += resend_packet_count[i][j];
                        } else {
                            packet_all_forward_count++;
                            // printf("i:%d, j:%d\n", i, j);
                        }
                    }
                }
                printf("%d, %d, %d, %d\n", packet_full_count, packet_partial_count, packet_all_forward_count, packet_partial_total_count);

                int seen_agtrs = 0;
                for (int i = 0; i < MAX_AGTR_COUNT; i++)
                    if (hash_table->isAlreadyDeclare[i])
                        seen_agtrs++;
                printf("Seen agtrs: %d\n", seen_agtrs);

                exit(-1);
            }
        }
        
        int to_be_sent = 0;
        if (this_pos_to_send + max_agtr_size_per_thread + max_agtr_size_per_thread > dma_context->my_send_queue_length / 2)
            this_pos_to_send = 0;

        // printf("%d packets received.\n", msgs_completed);
        for(int msg=0; msg < msgs_completed; msg++) {
            // std::chrono::high_resolution_clock::time_point packet_start = std::chrono::high_resolution_clock::now();
            uint8_t* buf = &dma_context->mp_recv_ring[dma_context->ring_head * kAppRingMbufSize];

            agghdr* p4ml_header = reinterpret_cast<agghdr*>(buf + IP_ETH_UDP_HEADER_SIZE);

            //check ecn mark
            // bool is_ecn_mark_packet = p4ml_header->flag & 0x08;
            // if (is_ecn_mark_packet)
            //     printf("ECN mark found.\n");
            if (DEBUG_PRINT_ALL_RECEIVING_PACKET)
                p4ml_header_print_h(p4ml_header, "Receive");

            bool isTerminated_packet = p4ml_header->flag & 0x02;
            bool isResend_packet = p4ml_header->flag & 0x04;
            bool isOverflow_packet = p4ml_header->flag & 0x80;
            
            // exit(1);
            p4ml_header_ntoh(p4ml_header);
            /* Move AppID index */
            int appID = p4ml_header->appID;
            if (!app_init[appID]) {
                app_init[appID] = true;
            } else {
                if (p4ml_header->key != tensors[tensors_pos_of_app[appID]].key && tensors[tensors_pos_of_app[appID]].isCompleted) {
                    // p4ml_header_print(p4ml_header, "ERROR PACKET");
                    // printf("tensors_pos_of_app[appID] from %d to %d\n", tensors_pos_of_app[appID], tensors_pos_of_app[appID]+1);
                    tensors_pos_of_app[appID]++;
                    if (tensors_pos_of_app[appID] == thread_id * MAX_APP_PER_THREAD * MAX_STORAGE_PER_APP_PER_THREAD + MAX_STORAGE_PER_APP_PER_THREAD * (appID))
                        tensors_pos_of_app[appID] = tensors_pos_of_app[appID] - MAX_STORAGE_PER_APP_PER_THREAD;
                }
            }

            if (!hash_table->isAlreadyDeclare[p4ml_header->agtr]) 
                hash_table->isAlreadyDeclare[p4ml_header->agtr] = true;

            /* Check if Collision packet */
            bool is_collision_packet = p4ml_header->flag & 0x02;

            if (is_collision_packet) {
                tensors[tensors_pos_of_app[appID]].isCollision[p4ml_header->seq_num] = true;
                // p4ml_header_print(p4ml_header, "COLLISION PACKET");
                // exit(1);
            }

            int my_tensors_pos = tensors_pos_of_app[appID];

            check_tensor_available(&tensors[my_tensors_pos], p4ml_header, thread_id);

            // char * eth_ip_header = (char*) dma_context->send_region + wc_recv_id * ENTRY_SIZE;
            // uint8_t swap[6];
            // for (int i = 0; i < 6; i++) {
            //     swap[i] = eth_ip_header[i];
            //     eth_ip_header[i] = eth_ip_header[i+6];
            //     eth_ip_header[i+6] = swap[i];
            // }

            if (OVERFLOW_HANDLE) {
                // Check Switch Overflow but not Host Overflow
                if (!isOverflow_packet)
                    for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                        if (p4ml_header->vector[i] == INT32_MAX || p4ml_header->vector[i] == INT32_MIN)
                        {
                            if (p4ml_header->vector[i] == INT32_MIN)
                                p4ml_header_print(p4ml_header, "Switch Overflow");
                            isOverflow_packet = true;
                        }
                        
            // p4ml_header_print(p4ml_header, "Receive");
                if (isOverflow_packet) {
                    /* Clean Integer Data */
                    if (!tensors[my_tensors_pos].isFloat[p4ml_header->seq_num]) {
                        // printf("ReadyForFloat\n");
                        makeTensorReadyforFloat(p4ml_header, &tensors[my_tensors_pos]);
                        tensors[my_tensors_pos].isFloat[p4ml_header->seq_num] = true;
                    }
                }

                /* Floating point request packet */
                bool sendFloatRequest = false;
                if (isOverflow_packet && !isResend_packet)
                    sendFloatRequest = true;
                if (!isOverflow_packet && isResend_packet && tensors[my_tensors_pos].isFloat[p4ml_header->seq_num])
                    sendFloatRequest = true;

                if (sendFloatRequest) {
                    /* Do floating point request */
                    /* Send back request to everyone immediately */
                    p4ml_header_hton_without_data(p4ml_header);
                    memcpy((char*) dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE), (char*) buf + IP_ETH_UDP_HEADER_SIZE, P4ML_LAYER_SIZE);
                    /* then send ACK */
                    p4ml_header_setACK((agghdr*)((char*)dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE)));
                    p4ml_header_setOverflowRequest((agghdr*)((char*)dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE)));
                    p4ml_header_resetIndex((agghdr*)((char*)dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE)));

                    // p4ml_header_print_h((agghdr*)((char*)dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE)), "Overflow Sendback PACKET");
                    send_packet(dma_context, P4ML_LAYER_SIZE, imm_pos_to_send);
                    imm_pos_to_send++;
                    if (imm_pos_to_send == dma_context->my_send_queue_length - 1)
                        imm_pos_to_send = dma_context->my_send_queue_length / 2 + 1;

                    /* Push Back */
                    dma_postback(dma_context);
                    continue;
                }
            }

            /* Check Full Packet */
            bool isFullPacket = (1 << p4ml_header->num_worker) - 1 == p4ml_header->bitmap? 1:0;

            
            if (receive_byte_reset_flag[thread_id]) {
                receive_in_sec[thread_id] = 0;
                receive_byte_reset_flag[thread_id] = false;
            }

            /* if full packet, update directly. */
            if (isFullPacket) {
                // printf("%d: full packet - seq %d update model.\n", p4ml_header->key, p4ml_header->seq_num);
                updateModel_force(p4ml_header, &tensors[my_tensors_pos]);
                for (int i = 0; i < p4ml_header->num_worker; i++) 
                    tensors[my_tensors_pos].window_manager[i].UpdateWindow(&p4ml_header->seq_num);

                if (p4ml_header->key < MAX_MEASUREMENT_KEY) {
                    if (isResend_packet) {
                        resend_packet_count[p4ml_header->key][p4ml_header->seq_num]++;
                    } else {
                        full_packet_count[p4ml_header->key][p4ml_header->seq_num]++;
                    }
                }
            } else {


                bool type_consistent = false;
                if (tensors[my_tensors_pos].isFloat[p4ml_header->seq_num] && isOverflow_packet)
                    type_consistent = true;
                if (!tensors[my_tensors_pos].isFloat[p4ml_header->seq_num] && !isOverflow_packet)
                    type_consistent = true;
                
                if (type_consistent)  {

                    if (p4ml_header->key < MAX_MEASUREMENT_KEY) {
                        if (isResend_packet)
                            resend_packet_count[p4ml_header->key][p4ml_header->seq_num]++;
                    }
                    // printf("seq %d Partial packet receive.\n", p4ml_header->seq_num);
                    // p4ml_header_print(p4ml_header, "Partial PACKET");
                    int valid_bit = 1;
                    bool need_to_update = true;
                    // check if update is needed
                    for (int i = 0; i < p4ml_header->num_worker; i++) {
                        if (valid_bit & p4ml_header->bitmap) {
                            if (tensors[my_tensors_pos].window_manager[i].isACKed[p4ml_header->seq_num]) {
                                // p4ml_header_print(p4ml_header, "ERROR PACKET");
                                // printf("[thread %d][worker %d]'s gredient is already integrated in PS, %d.\n", thread_id, i, p4ml_header->seq_num);
                                need_to_update = false;
                                break;
                            }
                        }   
                        valid_bit <<= 1;    
                    }

                    if (need_to_update) {
                        // printf("need to update\n");
                        int valid_bit = 1;
                        for (int i = 0; i < p4ml_header->num_worker; i++) {
                            if (valid_bit & p4ml_header->bitmap) { 
                                // TODO: Update Window will cause BUG, to be fix (floating point need reset ACK)
                                tensors[my_tensors_pos].window_manager[i].UpdateWindow(&p4ml_header->seq_num);
                            }
                            valid_bit <<= 1;
                        }
                        updateModel(p4ml_header, &tensors[my_tensors_pos], isOverflow_packet);
                    }
                
                }
            }
            // if any of the worker doesn't complete slot
            bool is_slot_completed = true;
            for (int i = 0; i < p4ml_header->num_worker; i++) 
                if (!tensors[my_tensors_pos].window_manager[i].isACKed[p4ml_header->seq_num]) 
                    is_slot_completed = false;
            // printf("packet receive %d\n", p4ml_header->seq_num);
            if (is_slot_completed) {
                p4ml_header->bitmap = 1;
                
                uint16_t new_agtr;

                // if collsiion is happened.
                if (tensors[my_tensors_pos].isCollision[p4ml_header->seq_num] == true) {
                    // Check if new agtr is already hashed
                    if (next_agtr[p4ml_header->agtr] == -1) {
                        int new_hash_agtr = hash_table->HashNew_predefine();
                        // if get any of AGTR from hash
                        if (new_hash_agtr != -1) {
                            new_agtr = new_hash_agtr;
                            next_agtr[p4ml_header->agtr] = new_agtr;
                            hash_table->hash_map[p4ml_header->agtr] = new_agtr;
                            // printf("old: %d -> new: %d\n", p4ml_header->agtr, new_agtr);
                        } else {
                            // if all of the AGTR is used, full
                            // keep original AGTR
                            // printf("Change Agtr fail, full.\n");
                            new_agtr = p4ml_header->agtr;
                        }
                    } else {
                        //TODO: Separate APP
                        new_agtr = next_agtr[p4ml_header->agtr];
                        // printf("New hash - already: %d\n", new_agtr);
                        // printf("[hashed] old: %d -> new: %d\n", p4ml_header->agtr, new_agtr);
                    }

                    p4ml_header_setLengthFieldToAgtr(p4ml_header, new_agtr);
                    p4ml_header_setCollisionBit(p4ml_header);
                } else {
                    p4ml_header_resetCollisionBit(p4ml_header);
                }

                int offset = (p4ml_header->seq_num - 1) * MAX_ENTRIES_PER_PACKET;
                
                p4ml_header_hton_without_data(p4ml_header);

                    if (!isOverflow_packet)
                        for (int i = 0; i < MAX_ENTRIES_PER_PACKET; i++)
                            tensors[my_tensors_pos].data.data_int[offset + i] = htonl(tensors[my_tensors_pos].data.data_int[offset + i]);

                // /* Give higher priority to Resend packet */
                if (isResend_packet) {
                    // TODO: PACKET LOSS HANDLING FOR DOUBLE PACKET 
                    // printf("Immediately send back Resend packet %d\n", ntohl(p4ml_header->seq_num));
                    memcpy((char*) dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE), (char*) buf + IP_ETH_UDP_HEADER_SIZE, P4ML_HEADER_SIZE - 12);
                    memcpy((char*) dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE) + P4ML_HEADER_SIZE - 12, tensors[my_tensors_pos].data.data_int + offset, P4ML_DATA_SIZE);
                    memcpy((char*) dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE) + 14 + P4ML_DATA_SIZE, (char*) buf + IP_ETH_UDP_HEADER_SIZE + P4ML_DATA_SIZE + 14, 12);
                    /* then send ACK */
                    p4ml_header_setACK((agghdr*)((char*)dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE)));
                    p4ml_header_resetIndex((agghdr*)((char*)dma_context->send_region + (imm_pos_to_send * P4ML_LAYER_SIZE)));

                    send_packet(dma_context, P4ML_LAYER_SIZE, imm_pos_to_send);
                    imm_pos_to_send++;
                    if (imm_pos_to_send == dma_context->my_send_queue_length - 1)
                        imm_pos_to_send = dma_context->my_send_queue_length / 2 + 1;

                } else {
                    memcpy((char*) dma_context->send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE, (char*) buf + IP_ETH_UDP_HEADER_SIZE, P4ML_HEADER_SIZE - 12);
                    memcpy((char*) dma_context->send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE + P4ML_HEADER_SIZE - 12, tensors[my_tensors_pos].data.data_int + offset, P4ML_DATA_SIZE);
                    memcpy((char*) dma_context->send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE + 14 + P4ML_DATA_SIZE, (char*) buf + IP_ETH_UDP_HEADER_SIZE + P4ML_DATA_SIZE + 14, 12);
                    /* then send ACK */
                    p4ml_header_setACK((agghdr*)((char*)dma_context->send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE));
                    p4ml_header_resetIndex((agghdr*)((char*)dma_context->send_region + (this_pos_to_send + to_be_sent) * P4ML_LAYER_SIZE));

                    to_be_sent++;
                }
                // printf("to_be_sent: %d\n", to_be_sent);

                if (tensors[tensors_pos_of_app[appID]].num_worker > 0) {
                    bool this_tensor_finished = true;
                    for (int i = 0; i < tensors[tensors_pos_of_app[appID]].num_worker; i++)
                        if (tensors[tensors_pos_of_app[appID]].window_manager[i].last_ACK < tensors[tensors_pos_of_app[appID]].window_manager[i].total_ACK)
                            this_tensor_finished = false;

                    if (this_tensor_finished && !tensors[tensors_pos_of_app[appID]].isCompleted) {
                        // printf("[Thread %d] Tensor %d at %d Completed.\n", thread_id, tensors[tensors_pos_of_app[appID]].key, tensors_pos_of_app[appID]);
                        tensors[tensors_pos_of_app[appID]].isCompleted = true;
                        rand_index = 0;
                        // dma_context->total_received = 0;
                        // dma_context->total_sent = 0;
                    }
                }
            }

            /* Push Back */
            dma_postback(dma_context);
        }
        
        dma_update_snapshot(dma_context, cur_snapshot);

        if (msgs_completed < 0) {
            printf("Polling error\n");
            exit(1);
        }

        if (msgs_completed > 0) {
            dma_context->total_received += msgs_completed;
            if (receive_byte_reset_flag[thread_id]) {
                receive_in_sec[thread_id] = msgs_completed;
                receive_byte_reset_flag[thread_id] = false;
            }
            else
                receive_in_sec[thread_id] += msgs_completed;
            if (to_be_sent > 0) {
                send_packet(dma_context, P4ML_LAYER_SIZE * to_be_sent, this_pos_to_send);
            }
            this_pos_to_send += to_be_sent;
            // Let assume the last packet will not loss        
        }
        
    }
}


void Start(int thread_id) {
    bindingCPU(thread_id + 16);
    DMAcontext* dma_context;
    {
        std::lock_guard<std::mutex> lock(_dma_mutex);

        dma_context = DMA_create(ib_dev, thread_id + ((appID - 1) * MAX_THREAD_PER_APP), true);
        // dma_context->isSent = new bool[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        // dma_context->send_time = new std::chrono::high_resolution_clock::time_point[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        // dma_context->receive_time = new std::chrono::high_resolution_clock::time_point[MAX_TENSOR_SIZE / MAX_ENTRIES_PER_PACKET + 1];
        global_dma_contexts[thread_id] = dma_context;
    }

    main_receive_packet_loop(dma_context, thread_id); 
    
    sleep(1000);
}

int main(int argc, char *argv[]) {
    bindingCPU(15);
    srand(time(NULL));
    // num_thread = atoi(argv[1]);

    appID = atoi(argv[1]);
    // Lam: this one is for experiment, disable temporary
    // if (argv[1])
    //     UsedSwitchAGTRcount = atoi(argv[1]);
    // else
    //     UsedSwitchAGTRcount = MAX_AGTR_COUNT;
    num_thread = 12;

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

    /* Init Thread */
    workQueue = new ThreadPool(num_thread, [](){});
    max_agtr_size_per_thread = 250;
    global_dma_contexts = new DMAcontext*[num_thread];
    printf("\nUsedSwitchAGTRcount: %d\n\n", UsedSwitchAGTRcount);
    printf("max_agtr_size_per_thread: %d\n\n", max_agtr_size_per_thread);

    printf("Overflow Handled: %s\n\n", OVERFLOW_HANDLE? "TRUE":"FALSE");
    /* Init tensors capacity */
    tensors = new tensor_context[MAX_APP_PER_THREAD * MAX_STORAGE_PER_APP_PER_THREAD * num_thread];
    printf("\nTensors memory pre-allocate...\n");
    for (int i = 0; i < MAX_APP_PER_THREAD * MAX_STORAGE_PER_APP_PER_THREAD * num_thread; i++)
        init_tensor(&tensors[i], MAX_TENSOR_SIZE);

    hash_table = new HashTable(UsedSwitchAGTRcount);
    printf("\nHash table creating...\n\n");
    memset(next_agtr, -1, sizeof(int) * MAX_AGTR_COUNT);
    
    for (int i = 0; i < num_thread; i++)
        workQueue->enqueue(Start, i);

    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::system_clock> timer = std::chrono::high_resolution_clock::now();
    while (1) {
        std::chrono::time_point<std::chrono::system_clock> current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - timer);
        std::chrono::duration<double> total_time = std::chrono::duration_cast<std::chrono::duration<double>>(current_time - t1);
        if (time_span.count() >= 1) {
            // printf("############################################\n");
            double total_bandwidth = 0.0;
            for (int i = 0; i < num_thread; i++) {
                // printf("[thread %d] %lf Gbps.\n", i, receive_in_sec[i] * 194.0 / 1024.0 / 1024.0 / 1024.0 * 8.0);
                total_bandwidth += receive_in_sec[i] * 194.0 / 1024.0 / 1024.0 / 1024.0 * 8.0;
                receive_byte_reset_flag[i] = true;
                // receive_in_sec[i] = 0;
            }

            
            // total_sent = 0;
            timer = current_time;
        }
    }

    sleep(10000000);

}
