# define P4ML_ORIGIN

#include <tofino/intrinsic_metadata.p4>
#include <tofino/stateful_alu_blackbox.p4>
#include <tofino/constants.p4>
#include "includes/headers.p4"
#include "includes/parser.p4"

#include "includes/registers.p4"
#include "includes/tables.p4"
#include "includes/actions.p4"
#include "includes/common.p4"

field_list p4ml_resubmit_list{
	mdata.agtr_time;	
}

action do_resubmit(){
	resubmit(p4ml_resubmit_list);
}

table p4ml_resubmit{
	actions{
		do_resubmit;
	}
	default_action: do_resubmit();
	size: 1;
}

control ingress 
{

    if (valid(p4ml_entries)) {

            if (ipv4.ecn == 3 or p4ml.ECN == 1) {
                apply(setup_ecn_table);
            }
            // ack packet
            if (p4ml.isACK == 1) {
                
                if (p4ml.overflow == 1 and p4ml.isResend == 0) {

                } else {
                    apply(clean_appID_and_seq_table);
                    
                    if (mdata.isOccupiedandNotMyAppIDandMyCurrentSeq == 0) {
                        /* Clean */
                        apply(clean_bitmap_table);
                        apply(clean_ecn_table);
                        apply(clean_agtr_time_table);
                        // apply(cleanEntry1);
                    }
                }

                /* Multicast Back */
                if(ig_intr_md.resubmit_flag == 1) {
                    apply(multicast_table);
                } else {
                    apply(p4ml_resubmit);
                }
                
            } else {

                if (p4ml.overflow == 1) {
                    apply(outPort_table);
                } else {
                    if (p4ml.isResend == 1) {
                        apply(appID_and_seq_resend_table);
                    } else {
                        apply(appID_and_seq_table);
                    }
                    // Correct ID and Seq
                    if (mdata.isOccupiedandNotMyAppIDandMyCurrentSeq == 0) {
                        
                        if (p4ml.isResend == 1) {
                            // Clean the bitmap also
                            apply(bitmap_resend_table);
                        } else {
                            apply(bitmap_table);
                        }

                        apply(ecn_register_table);
                        
                        apply(bitmap_aggregate_table);
                        if (mdata.isAggregate != 0) {
                            apply(set_need_aggregate_table);
                        }

                        if (p4ml.isResend == 1) {
                            // Force forward and clean
                            apply(agtr_time_resend_table);
                        } else {
                            apply(agtr_time_table);
                        }

                        apply(processEntry1);
                        apply(processEntry2);
                        apply(processEntry3);
                        apply(processEntry4);
                        apply(processEntry5);
                        apply(processEntry6);
                        apply(processEntry7);
                        apply(processEntry8);
                        apply(processEntry9);
                        apply(processEntry10);
                        apply(processEntry11);
                        apply(processEntry12);
                        apply(processEntry13);
                        apply(processEntry14);
                        apply(processEntry15);
                        apply(processEntry16);
                        apply(processEntry17);
                        apply(processEntry18);
                        apply(processEntry19);
                        apply(processEntry20);
                        apply(processEntry21);
                        apply(processEntry22);
                        apply(processEntry23);
                        apply(processEntry24);
                        apply(processEntry25);
                        apply(processEntry26);
                        apply(processEntry27);
                        apply(processEntry28);
                        apply(processEntry29);
                        apply(processEntry30);
                        apply(processEntry31);
                        //apply(processEntry32);
                                
                        if (mdata.need_aggregate == 1) {
                            if (mdata.need_send_out == 1) {
                                apply(modify_packet_bitmap_table);
                                apply(outPort_table);
                            } else {
                                if (ig_intr_md.resubmit_flag == 1) {
                                    apply(drop_table);
                                } else {
                                    apply(p4ml_resubmit);
                                }
                            }
                        } else {
                            if (mdata.need_send_out == 1) {
                                apply(modify_packet_bitmap_table);
                                apply(outPort_table);
                            }
                        }

                    } else {
                        /* tag collision bit in incoming one */
                        // if not empty
                        if (p4ml.isResend == 0) {
                            apply(tag_collision_incoming_table);
                        }
                        apply(outPort_table);
                    }
                }
            }
    } else {
        apply(forward);
    }
}

control egress 
{
      apply(qdepth_table);
      if (valid(ipv4)) {
          if (mdata.qdepth != 0) {
            apply(mark_ecn_ipv4_table);
          }
      }
      if (valid(p4ml_entries)) {
        if (mdata.qdepth != 0) {
            apply(modify_ecn_table);
        }
      }
}

