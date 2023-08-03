#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "pps.h"

int
parse_pps(struct bits_t *bits, struct pps_t *pps)
{
    pps->pic_parameter_set_id = in_ueint(bits);
    pps->seq_parameter_set_id = in_ueint(bits);
    pps->entropy_coding_mode_flag = in_uint(bits, 1);
    pps->pic_order_present_flag = in_uint(bits, 1);
    pps->num_slice_groups_minus1 = in_ueint(bits);
    if (pps->num_slice_groups_minus1 > 0)
    {
        printf("Error unsupported\n");
        return 1;
    }
    pps->num_ref_idx_l0_active_minus1 = in_ueint(bits);
    pps->num_ref_idx_l1_active_minus1 = in_ueint(bits);
    pps->weighted_pred_flag = in_uint(bits, 1);
    pps->weighted_bipred_idc = (in_uint(bits, 1) << 1) + in_uint(bits, 1);
    pps->pic_init_qp_minus26 = in_seint(bits);
    pps->pic_init_qs_minus26 = in_seint(bits);
    pps->chroma_qp_index_offset = in_seint(bits);
    pps->deblocking_filter_control_present_flag = in_uint(bits, 1);
    pps->constrained_intra_pred_flag = in_uint(bits, 1);
    pps->redundant_pic_cnt_present_flag = in_uint(bits, 1);

    return 0;
}
