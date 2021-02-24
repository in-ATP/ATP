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
                    
                    if (mdata.isMyAppIDandMyCurrentSeq != 0) {
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
                    if (mdata.isMyAppIDandMyCurrentSeq != 0) {
                        
                        if (p4ml.isResend == 1) {
                            // Clean the bitmap also
                            apply(bitmap_resend_table);
                        } else {
                            apply(bitmap_table);
                        }

                        apply(ecn_register_table);
                        
                        apply(bitmap_aggregate_table);

                        if (p4ml.isResend == 1) {
                            // Force forward and clean
                            apply(agtr_time_resend_table);
                        } else {
                            apply(agtr_time_table);
                        }

                        // bitmap correct
                        if (mdata.isAggregate != 0) {
                            if (mdata.current_agtr_time == p4ml.agtr_time) {
                                apply(noequ0_processEntry1andWriteToPacket);
                                apply(noequ0_processEntry2andWriteToPacket);
                                apply(noequ0_processEntry3andWriteToPacket);
                                apply(noequ0_processEntry4andWriteToPacket);
                                apply(noequ0_processEntry5andWriteToPacket);
                                apply(noequ0_processEntry6andWriteToPacket);
                                apply(noequ0_processEntry7andWriteToPacket);
                                apply(noequ0_processEntry8andWriteToPacket);
                                apply(noequ0_processEntry9andWriteToPacket);
                                apply(noequ0_processEntry10andWriteToPacket);
                                apply(noequ0_processEntry11andWriteToPacket);
                                apply(noequ0_processEntry12andWriteToPacket);
                                apply(noequ0_processEntry13andWriteToPacket);
                                apply(noequ0_processEntry14andWriteToPacket);
                                apply(noequ0_processEntry15andWriteToPacket);
                                apply(noequ0_processEntry16andWriteToPacket);
                                apply(noequ0_processEntry17andWriteToPacket);
                                apply(noequ0_processEntry18andWriteToPacket);
                                apply(noequ0_processEntry19andWriteToPacket);
                                apply(noequ0_processEntry20andWriteToPacket);
                                apply(noequ0_processEntry21andWriteToPacket);
                                apply(noequ0_processEntry22andWriteToPacket);
                                apply(noequ0_processEntry23andWriteToPacket);
                                apply(noequ0_processEntry24andWriteToPacket);
                                apply(noequ0_processEntry25andWriteToPacket);
                                apply(noequ0_processEntry26andWriteToPacket);
                                apply(noequ0_processEntry27andWriteToPacket);
                                apply(noequ0_processEntry28andWriteToPacket);
                                apply(noequ0_processEntry29andWriteToPacket);
                                apply(noequ0_processEntry30andWriteToPacket);
                                apply(noequ0_processEntry31andWriteToPacket);
                                //apply(noequ0_processEntry32andWriteToPacket);
                                // set output port
                                // if(ig_intr_md.resubmit_flag == 1) {
                                apply(modify_packet_bitmap_table);
                                apply(outPort_table);
                                // } else {
                                    // apply(p4ml_resubmit);
                                // }
                            } else {
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
                                
                                if (ig_intr_md.resubmit_flag == 1) {
                                    apply(drop_table);
                                } else {
                                    apply(p4ml_resubmit);
                                }

                            }
                        } else {
                            if (mdata.current_agtr_time == p4ml.agtr_time) {
                                apply(Entry1WriteToPacket);
                                apply(Entry2WriteToPacket);
                                apply(Entry3WriteToPacket);
                                apply(Entry4WriteToPacket);
                                apply(Entry5WriteToPacket);
                                apply(Entry6WriteToPacket);
                                apply(Entry7WriteToPacket);
                                apply(Entry8WriteToPacket);
                                apply(Entry9WriteToPacket);
                                apply(Entry10WriteToPacket);
                                apply(Entry11WriteToPacket);
                                apply(Entry12WriteToPacket);
                                apply(Entry13WriteToPacket);
                                apply(Entry14WriteToPacket);
                                apply(Entry15WriteToPacket);
                                apply(Entry16WriteToPacket);
                                apply(Entry17WriteToPacket);
                                apply(Entry18WriteToPacket);
                                apply(Entry19WriteToPacket);
                                apply(Entry20WriteToPacket);
                                apply(Entry21WriteToPacket);
                                apply(Entry22WriteToPacket);
                                apply(Entry23WriteToPacket);
                                apply(Entry24WriteToPacket);
                                apply(Entry25WriteToPacket);
                                apply(Entry26WriteToPacket);
                                apply(Entry27WriteToPacket);
                                apply(Entry28WriteToPacket);
                                apply(Entry29WriteToPacket);
                                apply(Entry30WriteToPacket);
                                apply(Entry31WriteToPacket);
                                //apply(Entry32WriteToPacket);
                                // set output port
                                // if(ig_intr_md.resubmit_flag == 1) {
                                apply(modify_packet_bitmap_table);
                                apply(outPort_table);
                                // } else {
                                    // apply(p4ml_resubmit);
                                // }	
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
        // // BG traffic doesn't have data layer
        //   if (valid(p4ml_bg)){
        //      apply(bg_outPort_table);
        //   } else {
        apply(forward);
        //   }
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

