#ifndef _PPS_H_
#define _PPS_H_

struct pps_t
{
    int pic_parameter_set_id;                    /* ue(v) */
    int seq_parameter_set_id;                    /* ue(v) */
    int entropy_coding_mode_flag;                /* u(1) */
    int pic_order_present_flag;                  /* u(1) */
    int num_slice_groups_minus1;                 /* ue(v) */
    int num_ref_idx_l0_active_minus1;            /* ue(v) */
    int num_ref_idx_l1_active_minus1;            /* ue(v) */
    int weighted_pred_flag;                      /* u(1) */
    int weighted_bipred_idc;                     /* u(1) */
    int pic_init_qp_minus26;                     /* se(v) */
    int pic_init_qs_minus26;                     /* se(v) */
    int chroma_qp_index_offset;                  /* se(v) */
    int deblocking_filter_control_present_flag;  /* u(1) */
    int constrained_intra_pred_flag;             /* u(1) */
    int redundant_pic_cnt_present_flag;          /* u(1) */
};

int
parse_pps(struct bits_t *bits, struct pps_t *pps);

#endif
