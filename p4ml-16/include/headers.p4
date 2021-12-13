/*******************************************************************************
 * BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
 *
 * Copyright (c) 2019-present Barefoot Networks, Inc.
 *
 * All Rights Reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of
 * Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
 * technical concepts contained herein are proprietary to Barefoot Networks, Inc.
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination of
 * this information or reproduction of this material is strictly forbidden unless
 * prior written permission is obtained from Barefoot Networks, Inc.
 *
 * No warranty, explicit or implicit is provided, unless granted under a written
 * agreement with Barefoot Networks, Inc.
 *
 ******************************************************************************/

#ifndef _HEADERS_
#define _HEADERS_

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;

typedef bit<16> ether_type_t;
const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;
const ether_type_t ETHERTYPE_PFC = 16w0x8803;//0x8808

typedef bit<8> ip_protocol_t;
const ip_protocol_t IP_PROTOCOLS_IPV4 = 4;
const ip_protocol_t IP_PROTOCOLS_UDP = 17;
const ip_protocol_t IP_PROTOCOLS_TCP = 6;

typedef bit<16> udp_port_t;
const udp_port_t UDP_PORT_ROCEV2 = 4791;

typedef bit<8> rocev2_protocol_t;
const rocev2_protocol_t ROCEV2_READ_REQEUST = 12;
const rocev2_protocol_t ROCEV2_READ_RESPONSE_FIRST = 13;
const rocev2_protocol_t ROCEV2_READ_RESPONSE_MIDDLE = 14;
const rocev2_protocol_t ROCEV2_READ_RESPONSE_LAST = 15;
const rocev2_protocol_t ROCEV2_READ_RESPONSE_ONLY = 16;
const rocev2_protocol_t ROCEV2_WRITE_REQUEST_ONLY = 10;
const rocev2_protocol_t ROCEV2_WRITE_REQUEST_FIRST = 6;
const rocev2_protocol_t ROCEV2_WRITE_REQUEST_MIDDLE = 7;
const rocev2_protocol_t ROCEV2_WRITE_REQUEST_LAST = 8;
const rocev2_protocol_t ROCEV2_WRTIE_RESPONSE_ACK = 17;

typedef bit<24> dst_qp_t;
typedef bit<8> rocev2_placeholder_t;
typedef bit<32> rocev2_crc_t;



/**/
typedef bit<8>  pkt_type_t;
const pkt_type_t PKT_TYPE_NORMAL = 1;
const pkt_type_t PKT_TYPE_MIRROR = 2;

#if __TARGET_TOFINO__ == 1
typedef bit<6> mirror_type_t;
#else
typedef bit<4> mirror_type_t;
#endif
//const mirror_type_t MIRROR_TYPE_I2E = 1;
//const mirror_type_t MIRROR_TYPE_E2E = 2;
#define SWITCH_MIRROR_TYPE_PORT 5
#define SWITCH_MIRROR_TYPE_SQ_PAUSE 10
#define SWITCH_MIRROR_TYPE_CUT_PAYLOAD 9


typedef int<32> data_type_t;
struct pair_t {
        data_type_t first;
        data_type_t second;
}


header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> ether_type;
}

header ipv4_h {
    bit<4> version;
    bit<4> ihl;
    bit<8> diffserv;
    bit<16> total_len;
    bit<16> identification;
    bit<3> flags;
    bit<13> frag_offset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

header udp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> length;
    bit<16> checksum;
}

header p4ml_h{
    bit<32> bitmap;
    bit<8> agtr_time; 
    bit<1> overflow;
     /* For multiple PS */
    bit<2> PSIndex;
        /* For signle PS */
        // reserved       :  2;
        // isForceFoward  :  1;
    bit<1> dataIndex;
    bit<1> ECN;
    bit<1> isResend;
    bit<1> isSWCollision;
    bit<1> isACK;
    bit<32> appIDandSeqNum;  //in switchml.p4: this is used to find the bit location

}
header p4ml_agtr_index_h{
    bit<16> agtr;
	// bit<1> pad;
}

header entry_h{
data_type_t        data0 ; 
data_type_t        data1 ;
data_type_t        data2 ;
data_type_t        data3 ;
data_type_t        data4 ;
data_type_t        data5 ;
data_type_t        data6 ;
data_type_t        data7 ;
data_type_t        data8 ;
data_type_t        data9 ;
data_type_t        data10;
data_type_t        data11;
data_type_t        data12;
data_type_t        data13;
data_type_t        data14;
data_type_t        data15;
data_type_t        data16;
data_type_t        data17;
data_type_t        data18;
data_type_t        data19;
data_type_t        data20;
data_type_t        data21;
data_type_t        data22;
data_type_t        data23;
data_type_t        data24;
data_type_t        data25;
data_type_t        data26;
data_type_t        data27;
data_type_t        data28;
data_type_t        data29;
data_type_t        data30;
//bit<32>        data31;
}

header p4ml_meta_h{
// P4ML
 	bit<1>          isMyAppIDandMyCurrentSeq;
    bit<7>          pad                     ;
    bit<32>         bitmap                  ;
 	bit<32>         isAggregate             ;
 	bit<8>          agtr_time               ;
 	bit<32>         integrated_bitmap       ;
 	bit<32>         agtr_index              ;
 	// bit<32>         isDrop                  ;
 	// bit<1>          inside_appID_and_Seq    ;
 	// bit<1>          value_one               ;
 	
 	// bit<1>          need_send_out           ;
 	// bit<6>          pad                     ;
 	bit<16>         qdepth                  ;
 	// bit<8>          seen_bitmap0            ;
 	// bit<8>          seen_isAggregate        ;
 	bit<8>          is_ecn                  ; 
}


header p4ml_bg_h {
	// bit<64>        key             ; 
	// bit<32>        len_tensor      ; 
	// bit<32>        bitmap          ; 
	// bit<8>         agtr_time       ; 
	// bit<4>         reserved        ; 
	// bit<1>         ECN             ; 
	// bit<1>         isResend        ; 
	// bit<1>         isSWCollision   ; 
	// bit<1>         isACK           ; 
	// bit<16>        agtr            ; 
	// bit<32>        appIDandSeqNum  ;   
}

#endif /* _HEADERS_ */

