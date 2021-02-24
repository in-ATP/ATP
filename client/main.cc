#include "p4ml_manager.h"

#define ENABLE_LOG true

uint32_t* init_model(int size) {
    uint32_t* tmp = new uint32_t[size];
    for (int i = 0; i < size; i++)
        tmp[i] = i+1;
    return tmp;
}

float* init_model_float(int size) {
    float* tmp = new float[size];
    for (int i = 0; i < size; i++) {
        tmp[i] = (i+1.0) / 10000000.0;
        // tmp[i] = (i + 1.0) / 10000.0;
        // printf("%f ", tmp[i]);
    }
    // tmp[63] = 200;
    return tmp;
}

float* init_model_float_with_overflow(int size) {
    float* tmp = new float[size];
    for (int i = 0; i < size; i++) {
        tmp[i] = (i+1.0) / 10000000.0;
    }
    for (int i = 0; i < 100; i++) {
        int rand_num = rand() % size;
        if (rand_num > size / 2)
            tmp[rand_num] = 200;
        else 
            tmp[rand_num] = 100;
        // printf("rand!!! %d\n", rand_num);
    }
    return tmp;
}


std::shared_ptr<P4mlManager> _p4ml_manager;

int main(int argc, char *argv[])
{
    bindingCPU(0);

    if (argc < 5) {
        printf("\nUsage %s [MyID] [Num of Worker] [AppID] [Num of PS]\n\n", argv[0]);
        exit(1);
    }

    int host = atoi(argv[1]); 
    int num_worker = atoi(argv[2]); 
    int appID = atoi(argv[3]); 
    int num_PS = atoi(argv[4]);

    //int host = 0;
    // int num_worker = 2;
    // int appID = 1;

    _p4ml_manager = std::shared_ptr<P4mlManager>(new P4mlManager(host, num_worker, appID, num_PS));
    
    /* Here for int size to send per thread */
    /* ex. 25600 = 32*800 = 1 Round */
    int size = 1024000;
    int thread_to_use = 12;
    int loop_time = 1000;

    if (argc > 5) {
        std::string option = argv[5];
        if (option == "-a") {
            int num_agtr = atoi(argv[6]);
            _p4ml_manager->SetMaxAgtrSizePerThread(num_agtr);
        } 
        if (option == "-f") {
            float forward_rate = atof(argv[6]);
            _p4ml_manager->SetForceForward(forward_rate);
        }
        if (option == "-l") {
            loop_time = atof(argv[6]);
        }
        if (option == "-aa") {
            int num_used_agtr = atoi(argv[6]);
            _p4ml_manager->SetUsedSwitchAGTRcount(num_used_agtr);
        }
    }

    /* (40) Threads in thread pool */
    /* MAX_AGTR (32000) / 40 = 800 Agtr per thread */
    _p4ml_manager->init_threadPool(thread_to_use);

    // _p4ml_manager->SetForceForward(0.25);
    // _p4ml_manager->SetMaxAgtrSizePerThread(50);

    int finish_counter = loop_time * thread_to_use;
    uint32_t** tensor = new uint32_t*[thread_to_use * loop_time];

    printf("\nModel initializing...\n");
    // for (int i = 0; i < thread_to_use * loop_time; i++)
    for (int i = 0; i < 1; i++)
        if (FLOATING_POINT_INPUT)
            tensor[i] = (uint32_t*) init_model_float_with_overflow(size);
        else 
            tensor[i] = init_model(size);
        
    printf("\nModel initialized completed. Start sending...\n\n");

    std::chrono::time_point<std::chrono::system_clock> timer = std::chrono::high_resolution_clock::now();

    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    
    for (int j = 0; j < loop_time; j++) {
    /* thread to use */
        for (int i = 0; i < thread_to_use; i++) {
            uint64_t key = _p4ml_manager->GetNewKey();
            _p4ml_manager->PushPull(key, (char*) tensor[0], size, 1);
        }
    }


    int total_sent = 0;

    while (finish_counter > 0) {
        int64_t tmp_key = _p4ml_manager->GetFinishKey();
        if (tmp_key >= 0) {
            finish_counter--;
            total_sent++;
        }

        if (ENABLE_LOG) {
            std::chrono::time_point<std::chrono::system_clock> current_time =
                std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> time_span =
                std::chrono::duration_cast<std::chrono::duration<double>>(current_time - timer);
            std::chrono::duration<double> total_time =
                std::chrono::duration_cast<std::chrono::duration<double>>(current_time - t1);
            if (time_span.count() >= 1) {
                // printf("Tensor left: %d, ", finish_counter);
                // printf("total send %" PRIu64 " bytes, time %lf, throughput: %lf\n", total_sent * 32000 * 194, total_time, total_sent * 6062.5 / 1024.0 / 1024.0 * 8.0 / 1.0);
                // printf("%lf\n", total_sent * 6062.5 / 1024.0 / 1024.0 * 8.0 / 1.0);
                // int tmp = _p4ml_manager->GetCollisionTimeAndClear();
                // if (tmp)
                //     printf("%d\n", tmp);
                // printf("%d\n", _p4ml_manager->GetCollisionTimeAndClear());
                printf("%lf\n", (float)total_sent * (16517 * P4ML_PACKET_SIZE) / 1024 / 1024 / 1024 * 8);
                total_sent = 0;
                timer = current_time;
            }
        }
    }
    _p4ml_manager->GetLossRate();
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();    
    std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    double transmit_size_in_m = (double)((double)size * loop_time * thread_to_use / (float)MAX_ENTRIES_PER_PACKET) * P4ML_PACKET_SIZE / 1024 / 1024;
    double total_time = time_span.count();
    double throughput = (transmit_size_in_m / 1024 * 8 ) / total_time;
    printf("Finish all %d Tensors,\n  Time = %lf s,\n  Total Size = %lf MB,\n  Throughput: %lf Gbps\n\n", thread_to_use * loop_time, total_time, transmit_size_in_m, throughput);
}
