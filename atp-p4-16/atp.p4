#include "data.p4"
#include "route.p4"

Register<bit<32>, total_aggregator_idx_t>(total_aggregator_cnt, 0) appID_and_Seq;
Register<bit<32>, total_aggregator_idx_t>(total_aggregator_cnt, 0) bitmap;
Register<bit<32>, total_aggregator_idx_t>(total_aggregator_cnt, 0) agtr_time;
Register<bit<8>, total_aggregator_idx_t>(total_aggregator_cnt, 0) ecn_register;

Register<bit<32>, bit<1>>(1, 0) loss_counter;

control AtpAck(
        in p4ml_h p4ml,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_md_for_tm,
        inout p4ml_meta_h mdata,
        in p4ml_agtr_index_h p4ml_agtr_index
        ){


    RegisterAction<bit<32>, total_aggregator_idx_t, bit<1>>(appID_and_Seq) clean_app_id_and_seq = {
        void apply(inout bit<32> value, out bit<1> rv){
            if(value == p4ml.appIDandSeqNum){
                value = 0;
                rv = 1;
            }
            
        }
    };

    RegisterAction<bit<32>, total_aggregator_idx_t, bit<32>>(bitmap) clean_bitmap = {
        void apply(inout bit<32> value, out bit<32> rv){
                value = 0;
        }
    };
    RegisterAction<bit<8>, total_aggregator_idx_t, bool>(ecn_register) clean_ecn_register = {
        void apply(inout bit<8> value, out bool rv){
            value = 0;
        }
    };
    /*
    * This table has to be in the same stage with
    * agr_time_table and agtr_time_resend_table. Both are in stage 3
    */
    @pragma stage 3 
    RegisterAction<bit<8>, total_aggregator_idx_t, bit<8>>(agtr_time) clean_agtr_time = {
        void apply(inout bit<8> value, out bit<8> rv){
            value = 0;
        }
    };

	action multicast(MulticastGroupId_t mgid ){
		ig_intr_md_for_tm.mcast_grp_a = mgid;
	}
	table multicast_table {
	    key ={
	        p4ml.isACK: exact;
	    }
	    actions = {
	        multicast; 
	    }
		size=2;
	}

    apply{
        
        if(p4ml.overflow == 1 && p4ml.isResend == 0){
        
        }else{
            mdata.isMyAppIDandMyCurrentSeq = clean_app_id_and_seq.execute(p4ml_agtr_index.agtr);
            if (mdata.isMyAppIDandMyCurrentSeq != 0) {
                /* Clean */
                clean_bitmap.execute(p4ml_agtr_index.agtr);
                clean_ecn_register.execute(p4ml_agtr_index.agtr);
                clean_agtr_time.execute(p4ml_agtr_index.agtr);
            }
        }
		multicast_table.apply();
    }

}

control AppIdSeq(
        inout switch_header_t hdr,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_md_for_dprsr,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_md_for_tm,
        inout p4ml_h p4ml,
        inout entry_h p4ml_entries,
        inout entry_h p4ml_entries1,
        inout p4ml_meta_h mdata,
        in p4ml_agtr_index_h p4ml_agtr_index
        
    ){
    RegisterAction<bit<32>, total_aggregator_idx_t, bit<1>>(appID_and_Seq) normal_check_app_id_and_seq = {
        void apply(inout bit<32> value, out bit<1> rv){
            if(value == 0 || value == p4ml.appIDandSeqNum){
                value = p4ml.appIDandSeqNum;
                rv = 1;
            }else{
                rv = 0;
            }
            
        }
    };
    RegisterAction<bit<32>, total_aggregator_idx_t, bit<1>>(appID_and_Seq) resend_check_app_id_and_seq = {
        void apply(inout bit<32> value, out bit<1> rv){
            if(value == p4ml.appIDandSeqNum){
                value = 0;
                rv = 1;
            }else{
                rv = 0;
            }
        }
    };
    //yle: the order matters?!
    RegisterAction<bit<32>, total_aggregator_idx_t, bit<32>>(bitmap) resend_read_write_bitmap = {
        void apply(inout bit<32> value, out bit<32> rv){
                rv = value;
                value = 0;
        }
    };

    RegisterAction<bit<32>, total_aggregator_idx_t, bit<32>>(bitmap) normal_read_write_bitmap = {
        void apply(inout bit<32> value, out bit<32> rv){
                rv = value;
                value = value | p4ml.bitmap;
        }
    };

    RegisterAction<bit<8>, total_aggregator_idx_t, bit<1>>(ecn_register) update_ecn_bit = {
        void apply(inout bit<8> value, out bit<1> rv){
            rv = value[0:0];
            value = value | mdata.is_ecn;  
        }
    };    

    action setup_ecn_action(){
        mdata.is_ecn = 1;
    }
    action check_aggregate_and_forward() {
        // this is is for aggregation needed checking
        //bit_andcb(mdata.isAggregate, p4ml.bitmap, mdata.bitmap); 
        mdata.isAggregate = p4ml.bitmap & ~mdata.bitmap;         
        mdata.integrated_bitmap =  p4ml.bitmap | mdata.bitmap;                                                   
    }
    RegisterAction<bit<32>, total_aggregator_idx_t, bit<1>>(agtr_time) resend_check_agtr_time = {
        void apply(inout bit<32> value, out bit<1> rv){
            value = 0;
            rv = 1;
            
        }
    };

    RegisterAction<bit<32>, total_aggregator_idx_t, bit<1>>(agtr_time) normal_check_agtr_time = {
        void apply(inout bit<32> value, out bit<1> rv){
            if(mdata.isAggregate != 0){
                value = value + 1;
            }
            rv = (bit<1>)(value[7:0] == p4ml.agtr_time);
        }
    };

    action tag_collision_incoming() {
        p4ml.isSWCollision = 1; 
    }

	action drop_pkt(){
		ig_intr_md_for_dprsr.drop_ctl = 3w1;
	}

	action nop(){
	}
	action modify_packet_bitmap() {
    	p4ml.bitmap = mdata.integrated_bitmap;
	}
	
    table modify_packet_bitmap_table {
	    key = {
	        p4ml.dataIndex: exact;
	    }
	    actions = {
	        modify_packet_bitmap; nop;
	    }
	    const default_action = nop;
        size = 2;
	}

    @pragma stage 4
    ProcessData0() process_data0;
    ProcessData1() process_data1;
    ProcessData2() process_data2;
    ProcessData3() process_data3;
    ProcessData4() process_data4;
    ProcessData5() process_data5;
    ProcessData6() process_data6;
    ProcessData7() process_data7;
    ProcessData8() process_data8;
    ProcessData9() process_data9;
    ProcessData10() process_data10;
    ProcessData11() process_data11;
    ProcessData12() process_data12;
    ProcessData13() process_data13;
    ProcessData14() process_data14;
    ProcessData15() process_data15;
    ProcessData16() process_data16;
    ProcessData17() process_data17;
    ProcessData18() process_data18;
    ProcessData19() process_data19;
    ProcessData20() process_data20;
    ProcessData21() process_data21;
    ProcessData22() process_data22;
    ProcessData23() process_data23;
    ProcessData24() process_data24;
    ProcessData25() process_data25;
    ProcessData26() process_data26;
    ProcessData27() process_data27;
    ProcessData28() process_data28;
    ProcessData29() process_data29;
    ProcessData30() process_data30;

    AtpAck() atp_ack;
    Route() route;

    apply{
          if (hdr.ipv4.diffserv == 0b11 || p4ml.ECN == 1) {
            setup_ecn_action();
          }
          if(p4ml.isACK == 1){
            atp_ack.apply(p4ml, ig_intr_md_for_tm, mdata, p4ml_agtr_index);
          }else{
              if(p4ml.overflow == 1){
                  route.apply(hdr, ig_intr_md_for_dprsr, ig_intr_md_for_tm, p4ml.dataIndex);
              }else{

                  if(p4ml.isResend == 1){
                      mdata.isMyAppIDandMyCurrentSeq = resend_check_app_id_and_seq.execute(p4ml_agtr_index.agtr);
                  }else{
                      mdata.isMyAppIDandMyCurrentSeq = normal_check_app_id_and_seq.execute(p4ml_agtr_index.agtr);
                  }
                  //hit aggregator
                  if(mdata.isMyAppIDandMyCurrentSeq == 1){
                      if(p4ml.isResend == 1){
                          mdata.bitmap = resend_read_write_bitmap.execute(p4ml_agtr_index.agtr);
                      }else{
                          mdata.bitmap = normal_read_write_bitmap.execute(p4ml_agtr_index.agtr);
                      }
                      p4ml.ECN = update_ecn_bit.execute(p4ml_agtr_index.agtr);
                      check_aggregate_and_forward();
                      if(p4ml.isResend == 1){
                          mdata.need_send_out = resend_check_agtr_time.execute(p4ml_agtr_index.agtr);
                      }else{
                          mdata.need_send_out  = normal_check_agtr_time.execute(p4ml_agtr_index.agtr);
                      }
                      
                      PROCESS_ENTRY 


                      if (mdata.isAggregate != 0) {
                          if (mdata.need_send_out == 1) {
                                modify_packet_bitmap_table.apply();
                          		route.apply(hdr, ig_intr_md_for_dprsr, ig_intr_md_for_tm, p4ml.dataIndex);
                            } else {
                            	drop_pkt();
							}
                        } else {
                            if (mdata.need_send_out == 1) {
                                modify_packet_bitmap_table.apply();
                          		route.apply(hdr, ig_intr_md_for_dprsr, ig_intr_md_for_tm, p4ml.dataIndex);
                            }
                        }


                  }else{
                       /* tag collision bit in incoming one */
                      // if not empty   
                      if (p4ml.isResend == 0) {
                          tag_collision_incoming();
                      }
					  route.apply(hdr, ig_intr_md_for_dprsr, ig_intr_md_for_tm, p4ml.dataIndex);
                  }
               }
           }
    }
}
