/*
 * P4PS
 * /

/*************************************************************************
 ***********************  R E G I S T E R  *******************************
 *************************************************************************/

blackbox stateful_alu cleaning_agtr_time {
    reg: agtr_time;

    update_lo_1_value   :  0;
}

blackbox stateful_alu cleaning_ecn {
    reg: ecn_register;

    update_lo_1_value   :  0;
}

blackbox stateful_alu cleaning_bitmap {
    reg: bitmap;

    update_lo_1_value   :  0;
}

blackbox stateful_alu read_write_bitmap {
    reg: bitmap;

    output_dst            :  mdata.bitmap;

    output_value          :  register_lo;

    update_lo_1_value     :  register_lo | p4ml.bitmap;
}

blackbox stateful_alu read_write_bitmap_resend {
    reg: bitmap;

    output_dst            :  mdata.bitmap;

    output_value          :  register_lo;

    update_lo_1_value     :  0;
}

// if same application, output appID, if not, not output (zero)
blackbox stateful_alu check_app_id_and_seq {
    reg: appID_and_Seq;

    condition_lo           :  p4ml.appIDandSeqNum == register_lo;
    // The agtr is empty
    condition_hi           :  register_lo == 0;

    update_lo_1_predicate  :  condition_lo or condition_hi;
    update_lo_1_value      :  p4ml.appIDandSeqNum;

    output_predicate       :  condition_lo or condition_hi;
    output_dst             :  mdata.isMyAppIDandMyCurrentSeq;
    output_value           :  p4ml.appIDandSeqNum;
}

blackbox stateful_alu check_app_id_and_seq_resend {
    reg: appID_and_Seq;

    condition_lo           :  p4ml.appIDandSeqNum == register_lo;

    update_lo_1_predicate  :  condition_lo;
    update_lo_1_value      :  0;

    output_predicate       :  condition_lo;
    output_dst             :  mdata.isMyAppIDandMyCurrentSeq;
    output_value           :  register_lo;
}

blackbox stateful_alu clean_app_id_and_seq {
    reg: appID_and_Seq;

    condition_lo           :  p4ml.appIDandSeqNum == register_lo;

    update_lo_1_predicate  :  condition_lo;
    update_lo_1_value      :  0;

    output_predicate       :  condition_lo;
    output_dst             :  mdata.isMyAppIDandMyCurrentSeq;
    output_value           :  p4ml.appIDandSeqNum;
}

blackbox stateful_alu check_agtrTime {
    reg: agtr_time;

    condition_lo           :  mdata.isAggregate != 0;
    output_dst             :  mdata.current_agtr_time; 

    update_lo_1_predicate  :  condition_lo;
    update_lo_1_value      :  register_lo + 1;

    update_lo_2_predicate  :  not condition_lo;
    update_lo_2_value      :  register_lo;

    output_value           :  alu_lo;
}

blackbox stateful_alu check_resend_agtrTime {
    reg: agtr_time;

    condition_lo           :  mdata.isAggregate != 0;
    // fake, force forward
    output_dst             :  mdata.current_agtr_time; 

    update_lo_1_predicate  :  condition_lo;
    update_lo_1_value      :  0;

    update_lo_2_predicate  :  not condition_lo;
    update_lo_2_value      :  0;

    output_value           :  p4ml.agtr_time;
}

blackbox stateful_alu do_comp_qdepth {
    reg: dqueue_alert_threshold;

    condition_lo           :  eg_intr_md.deq_qdepth >= register_lo;
    // fake, force forward
    output_predicate       :  condition_lo;
    output_dst             :  mdata.qdepth; 
    output_value           :  eg_intr_md.deq_qdepth;
    initial_register_lo_value : 1000;
}

blackbox stateful_alu do_check_ecn {
    reg: ecn_register;

 	condition_lo		   :  register_lo == 1; 
  
    update_lo_1_value      :  register_lo | mdata.is_ecn;

	output_predicate 	   :  condition_lo;
    output_value           :  mdata.value_one;
    output_dst             :  p4ml.ECN;
}

/*************************************************************************
 **************  I N G R E S S   P R O C E S S I N G   *******************
 *************************************************************************/

/*
 * Actions 
 */

action process_bitmap() {
    read_write_bitmap.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action process_bitmap_resend() {
    read_write_bitmap_resend.execute_stateful_alu(p4ml_agtr_index.agtr);
}


action check_aggregate_and_forward() {
    // this is is for aggregation needed checking
    bit_andcb(mdata.isAggregate, p4ml.bitmap, mdata.bitmap);
    bit_or(mdata.integrated_bitmap, p4ml.bitmap, mdata.bitmap);
}

action clean_agtr_time() {
    cleaning_agtr_time.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action clean_ecn() {
    cleaning_ecn.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action clean_bitmap() {
    cleaning_bitmap.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action multicast(group) {
    modify_field(ig_intr_md_for_tm.mcast_grp_a, group);
}

action check_appID_and_seq() {
    check_app_id_and_seq.execute_stateful_alu(p4ml_agtr_index.agtr);
    //modify_field(mdata.qdepth, 0);   
}

action check_appID_and_seq_resend() {
    check_app_id_and_seq_resend.execute_stateful_alu(p4ml_agtr_index.agtr);
 //   modify_field(mdata.qdepth, 0);   
}

action clean_appID_and_seq() {
    clean_app_id_and_seq.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action check_agtr_time() {
    check_agtrTime.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action check_resend_agtr_time() {
    check_resend_agtrTime.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action modify_packet_bitmap() {
    modify_field(p4ml.bitmap, mdata.integrated_bitmap);
}

action do_qdepth() {
    do_comp_qdepth.execute_stateful_alu(0);
}

action modify_ecn() {
    modify_field(p4ml.ECN, 1);
}

action mark_ecn() {
    bit_or(mdata.is_ecn, mdata.qdepth, mdata.is_ecn);
}

action modify_ipv4_ecn() {
    modify_field(ipv4.ecn, 3);
}

action check_ecn() {
    do_check_ecn.execute_stateful_alu(p4ml_agtr_index.agtr);
}

action setup_ecn() {
    modify_field(mdata.is_ecn, 1);    
}

action tag_collision_incoming() {
    modify_field(p4ml.isSWCollision, 1);
    // modify_field(p4ml.bitmap, mdata.isMyAppIDandMyCurrentSeq);
}

action set_egr(egress_spec) {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, egress_spec);
    // increase_p4ml_counter.execute_stateful_alu(ig_intr_md.ingress_port);
}

action set_egr_and_set_index(egress_spec) {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, egress_spec);
    modify_field(p4ml.dataIndex, 1);
    // increase_p4ml_counter.execute_stateful_alu(ig_intr_md.ingress_port);
}

action nop()
{
}

action drop_pkt() {
    drop();
}

action increase_counter() {
    increase_p4ml_counter.execute_stateful_alu(0);
}

table bitmap_table {
    actions {
        process_bitmap;
    }
    default_action: process_bitmap();
    size : 1;
}

table bitmap_resend_table {
    actions {
        process_bitmap_resend;
    }
    default_action: process_bitmap_resend();
    size : 1;
}

table bitmap_aggregate_table {
    actions {
        check_aggregate_and_forward;
    }
    default_action: check_aggregate_and_forward();
    size : 1;
}

table agtr_time_table {
    actions {
        check_agtr_time;
    }
    default_action: check_agtr_time();
    size : 1;
}

table agtr_time_resend_table {
    actions {
        check_resend_agtr_time;
    }
    default_action: check_resend_agtr_time();
    size : 1;
}

table immd_outPort_table {
    reads {
        p4ml.appIDandSeqNum mask 0xFFFF0000: exact;
    }
    actions {
        set_egr;
    }
}

table outPort_table {
    reads {
        p4ml.appIDandSeqNum mask 0xFFFF0000: exact;
        ig_intr_md.ingress_port: exact;
        p4ml.dataIndex: exact;
        p4ml.PSIndex: exact;
    }
    actions {
		nop;
        set_egr;
        set_egr_and_set_index;
        drop_pkt;
    }
    default_action: drop_pkt();
}

table bg_outPort_table {
    reads {
        // useless here, just can't use default action for variable
        p4ml_bg.isACK : exact;
    }
    actions {
        set_egr;
		nop;
    }
}

table multicast_table {
    reads {
        p4ml.isACK: exact;
        p4ml.appIDandSeqNum mask 0xFFFF0000: exact;
        ig_intr_md.ingress_port: exact;
        p4ml.dataIndex: exact;
    }
    actions {
        multicast; drop_pkt; set_egr_and_set_index;
    }
    default_action: drop_pkt();
}

@pragma stage 3
table clean_agtr_time_table {
    actions {
        clean_agtr_time;
    }
    default_action: clean_agtr_time();
    size : 1;
}

table clean_ecn_table {
    actions {
        clean_ecn;
    }
    default_action: clean_ecn();
    size : 1;
}


table clean_bitmap_table {
    actions {
        clean_bitmap;
    }
    default_action: clean_bitmap();
    size : 1;
}

/* Counter */
register p4ml_counter {
    width : 32;
    instance_count :1;
}

blackbox stateful_alu increase_p4ml_counter {
    reg: p4ml_counter;

    update_lo_1_value   :  register_lo + 1 ;
}

table forward_counter_table {
        actions {
        increase_counter;
    }
    default_action: increase_counter();
}

table appID_and_seq_table {
        actions {
        check_appID_and_seq;
    }
    default_action: check_appID_and_seq();
}

table appID_and_seq_resend_table {
        actions {
        check_appID_and_seq_resend;
    }
    default_action: check_appID_and_seq_resend();
}

table clean_appID_and_seq_table {
        actions {
        clean_appID_and_seq;
    }
    default_action: clean_appID_and_seq();
}

table modify_packet_bitmap_table {
    reads {
        p4ml.dataIndex: exact;
    }
        actions {
        modify_packet_bitmap; nop;
    }
    default_action: nop();
}

table qdepth_table {
    actions {
        do_qdepth;
    }
    default_action: do_qdepth();
}

table modify_ecn_table {
    actions {
        modify_ecn;
    }
    default_action: modify_ecn();
}

table mark_ecn_ipv4_table {
    actions {
        modify_ipv4_ecn;
    }
    default_action: modify_ipv4_ecn();
}

table ecn_mark_table {
    actions {
        mark_ecn;
    }
    default_action: mark_ecn();
}

table ecn_register_table {
    actions {
        check_ecn;
    }
    default_action: check_ecn();
}

table setup_ecn_table {
    actions {
        setup_ecn;
    }
    default_action: setup_ecn();
}

table forward {
    reads {
        ethernet.dstAddr : exact;
    }
    actions {
        set_egr; nop; drop_pkt;
    }
    default_action: drop_pkt();
}

table drop_table {
    reads {
        ig_intr_md.ingress_port: exact;
        p4ml.dataIndex : exact;
    }
    actions {
        drop_pkt; set_egr; set_egr_and_set_index;
    }
    default_action: drop_pkt();
}

table tag_collision_incoming_table {
    actions {
        tag_collision_incoming;
    }
    default_action: tag_collision_incoming();
}
