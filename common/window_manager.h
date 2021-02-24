#ifndef SLIDING_W_H
#define SLIDING_W_H

#include "packet.h"
#include "CC_manager.h"
#define RESEND_TRIGGER 1

class WindowManager {
    public:
        bool* isACKed;
        /* This three variable is completely useless, but
        when deleting it, the performance will drop from 46Gbps to 40Gbps.. */
        bool* isSent;
        std::chrono::high_resolution_clock::time_point* send_time;
        std::chrono::high_resolution_clock::time_point* receive_time;
        /* */
        int total_ACK;
        int last_ACK;

        WindowManager() {
            last_ACK = 0;
        }

        bool inline UpdateWindow(uint16_t* seq_num)
        {
            bool isLastAckUpdated = false;
            isACKed[*seq_num] = true;
            while (isACKed[last_ACK + 1]) {
                last_ACK++;
                isLastAckUpdated = true;
            }
            return isLastAckUpdated;
        }

        int inline Reset(int packet_total)
        {
            last_ACK = 0;
            total_ACK = packet_total;
            memset(isACKed, 0, sizeof(bool) * packet_total + 1);
        }
};

#endif