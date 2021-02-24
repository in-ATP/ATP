#define MAX_ENTRIES_PER_PACKET 32
/*************************************************************************
 ***********************  H E A D E R S  *********************************
 *************************************************************************/

// 14Byte
header_type ethernet_t {
    fields {
        dstAddr   : 48;
        srcAddr   : 48;
        etherType : 16;
    }
}

// 20Byte
header_type ipv4_t {
    fields {
        version        : 4;
        ihl            : 4;
        dscp           : 6;
        ecn            : 2;
        totalLen       : 16;
        identification : 16;
        flags          : 3;
        fragOffset     : 13;
        ttl            : 8;
        protocol       : 8;
        hdrChecksum    : 16;
        srcAddr        : 32;
        dstAddr        : 32;
    }
}

header_type udp_t {
    fields {
        srcPort : 16;
        dstPort : 16;
        length_ : 16;
        checksum : 16;
    }
}

// 12Byte * 2
header_type p4ml_t {
    fields {
        bitmap         :  32;
        agtr_time      :  8;
        overflow       :  1;
        /* For multiple PS */
        PSIndex        :  2;
        /* For signle PS */
        // reserved       :  2;
        // isForceFoward  :  1;
        dataIndex      :  1; 
        ECN            :  1;
        isResend       :  1;
        isSWCollision  :  1;
        isACK          :  1;
        appIDandSeqNum :  32;  //in switchml.p4: this is used to find the bit location 
    }
}

header_type p4ml_agtr_index_t {
	fields{
	    agtr	:16;
    }
}

header_type bg_p4ml_t {
    fields {
        key            :  64;
        len_tensor     :  32;
        bitmap         :  32;
        agtr_time      :  8;
        reserved       :  4;
        ECN            :  1;
        isResend       :  1;
        isSWCollision  :  1;
        isACK          :  1;
        agtr           :  16;
        appIDandSeqNum :  32;  //in switchml.p4: this is used to find the bit location
    }
}

// 108Byte * 2
header_type entry_t {
    fields {
        data0         : 32 (signed);
        data1         : 32 (signed);
        data2         : 32 (signed);
        data3         : 32 (signed);
        data4         : 32 (signed);
        data5         : 32 (signed);
        data6         : 32 (signed);
        data7         : 32 (signed);
        data8         : 32 (signed);
        data9         : 32 (signed);
        data10        : 32 (signed);
        data11        : 32 (signed);
        data12        : 32 (signed);
        data13        : 32 (signed);
        data14        : 32 (signed);
        data15        : 32 (signed);
        data16        : 32 (signed);
        data17        : 32 (signed);
        data18        : 32 (signed);
        data19        : 32 (signed);
        data20        : 32 (signed);
        data21        : 32 (signed);
        data22        : 32 (signed);
        data23        : 32 (signed);
        data24        : 32 (signed);
        data25        : 32 (signed);
        data26        : 32 (signed);
        data27        : 32 (signed);
        data28        : 32 (signed);
        data29        : 32 (signed);
        data30        : 32 (signed);
//        data31        : 32 (signed);
    }
}

//12Byte * 2
// header_type entry2_t {
//     fields {
//         data27        : 32 (signed);
//         data28        : 32 (signed);
//         data29        : 32 (signed);
//         data30        : 32 (signed);
//         data31        : 32 (signed);
//     }
// }

/*************************************************************************
 ***********************  M E T A D A T A  *******************************
 *************************************************************************/

header_type p4ml_meta_t {
    fields {
        // P4ML
        isMyAppIDandMyCurrentSeq : 16;
        bitmap                   : 32;
        isAggregate              : 32;
        agtr_time                : 8;
        integrated_bitmap        : 32;
        current_agtr_time        : 8;
        agtr_index 	          	 : 32;
	    isDrop                   : 32; 
        inside_appID_and_Seq     : 1;
        value_one                : 1;
        qdepth                   : 16;
 	    seen_bitmap0		     : 8;
	    seen_isAggregate 	     : 8;
        is_ecn                   : 32;
	}
}

header_type p4ml_constant_t {
	fields{
        bitmap		:32;
        agtr_time	:8;
	}
}
