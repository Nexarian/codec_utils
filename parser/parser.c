/*
slice_type  name of slice
0           P
1           B
2           I
3           SP
4           SI
5           P
6           B
7           I
8           SP
9           SI
*/

/* https://chromium.googlesource.com/external/webrtc/+/HEAD/common_video/h264/sps_parser.cc */
/* https://en.wikipedia.org/wiki/Exponential-Golomb_coding */
/* https://www.cardinalpeak.com/blog/the-h-264-sequence-parameter-set */
/* https://github.com/alb423/sps_pps_parser */
/* https://blog.birost.com/a?ID=01450-72ff90f9-a465-464a-91c7-d4671f46cece */
/* https://base64.guru/converter/encode/hex */
/* https://mradionov.github.io/h264-bitstream-viewer/ */
/* https://github.com/cplussharp/graph-studio-next/blob/master/src/H264StructReader.cpp#L32 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "bits.h"
#include "sps.h"
#include "utils.h"

static int
get_next_frame(int fd, char* data, int* bytes, int* width, int* height)
{
    struct _header
    {
        char text[4];
        int width;
        int height;
        int bytes_follow;
    } header;

    if (read(fd, &header, sizeof(header)) != sizeof(header))
    {
        return 1;
    }
    if (strncmp(header.text, "BEEF", 4) != 0)
    {
        printf("not BEEF file\n");
        return 2;
    }
    if (header.bytes_follow > *bytes)
    {
        return 3;
    }
    if (read(fd, data, header.bytes_follow) != header.bytes_follow)
    {
        return 4;
    }
    printf("new frame width %d height %d bytes_follow %d\n", header.width, header.height, header.bytes_follow);
    *bytes = header.bytes_follow; 
    *width = header.width;
    *height = header.height;
    return 0;
}


uint32_t sps_parser_offset;

uint8_t sps_parser_base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t sps_parser_base64_decode(char *buffer) {

	uint8_t dtable[256], block[4], tmp, pad = 0;
	size_t i, count = 0, pos = 0, len = strlen(buffer);

	memset(dtable, 0x80, 256);
	for (i = 0; i < sizeof(sps_parser_base64_table) - 1; i++) {
		dtable[sps_parser_base64_table[i]] = (unsigned char) i;
	}

	dtable['='] = 0;

	for (i = 0; i < len; i++) {
		if (dtable[buffer[i]] != 0x80) {
			count++;
		}
	}

	if (count == 0 || count % 4) return 0;


	count = 0;
	for (i = 0; i < len; i++) {
		tmp = dtable[buffer[i]];
		if (tmp == 0x80) continue;

		if (buffer[i] == '=') pad++;
		block[count] = tmp;
		count++;
		if (count == 4) {

			buffer[pos++] = (block[0] << 2) | (block[1] >> 4);
			buffer[pos++] = (block[1] << 4) | (block[2] >> 2);
			buffer[pos++] = (block[2] << 6) | block[3];

			count = 0;
			if (pad) {

				if (pad == 1) pos--;
				else if (pad == 2) pos -= 2;
				else return 0;

				break;
			}
		}
	}

	return pos;
}

uint32_t sps_parser_read_bits(char *buffer, uint32_t count) {
	uint32_t result = 0;
	uint8_t index = (sps_parser_offset / 8);
	uint8_t bitNumber = (sps_parser_offset - (index * 8));
	uint8_t outBitNumber = count - 1;
	for (uint8_t c = 0; c < count; c++) {
		if (buffer[index] << bitNumber & 0x80) {
			result |= (1 << outBitNumber);
		}
		if (++bitNumber > 7) { bitNumber = 0; index++; }
		outBitNumber--;
	}
	sps_parser_offset += count;
	return result;
}

uint32_t sps_parser_read_ueg(char* buffer) {
    uint32_t bitcount = 0;

    for (;;) {
        if (sps_parser_read_bits(buffer, 1) == 0)
        {
	        bitcount++;
        }
        else
        {
            // bitOffset--;
            break;
        }
    }

    // bitOffset --;
    uint32_t result = 0;
    if (bitcount)
    {
        uint32_t val = sps_parser_read_bits(buffer, bitcount);
        result = (uint32_t) ((1 << bitcount) - 1 + val);
    }

    return result;
}

int32_t sps_parser_read_eg(char* buffer) {
	uint32_t value = sps_parser_read_ueg(buffer);
	if (value & 0x01) {
		return (value + 1) / 2;
	} else {
		return -(value / 2);
	}
}

void sps_parser_skipScalingList(char* buffer, uint8_t count) {
	uint32_t deltaScale, lastScale = 8, nextScale = 8;
	for (uint8_t j = 0; j < count; j++) {
		if (nextScale != 0) {
			deltaScale = sps_parser_read_eg(buffer);
			nextScale = (lastScale + deltaScale + 256) % 256;
		}
		lastScale = (nextScale == 0 ? lastScale : nextScale);
	}
}

uint32_t sps_parser(char *buffer, struct sps_t* sps)
{
	sps_parser_offset = 0;
	sps_parser_base64_decode(buffer);

    sps->forbidden_zero_bit = sps_parser_read_bits(buffer, 1);
    sps->nal_ref_idc   = sps_parser_read_bits(buffer, 2);
    sps->nal_unit_type = sps_parser_read_bits(buffer, 5);

	sps->profile_idc = sps_parser_read_bits(buffer, 8);

    sps->constraint_set0_flag  = sps_parser_read_bits(buffer, 1);
    sps->constraint_set1_flag  = sps_parser_read_bits(buffer, 1);
    sps->constraint_set2_flag  = sps_parser_read_bits(buffer, 1);
    sps->constraint_set3_flag  = sps_parser_read_bits(buffer, 1);
    sps->reserved_zero_4bits   = sps_parser_read_bits(buffer, 4);
    sps->level_idc             = sps_parser_read_bits(buffer, 8);

	sps->seq_parameter_set_id = sps_parser_read_ueg(buffer);

	if (sps->profile_idc == 100 || sps->profile_idc == 110 || sps->profile_idc == 122 ||
		sps->profile_idc == 244 || sps->profile_idc == 44 || sps->profile_idc == 83 ||
		sps->profile_idc == 86 || sps->profile_idc == 118 || sps->profile_idc == 128)
    {
        sps->chroma_format_idc = sps_parser_read_ueg(buffer);
		if (sps->chroma_format_idc == 3)
        {
            sps->separate_color_plane_flag = sps_parser_read_bits(buffer, 1);
        }
		sps->bit_depth_luma_minus8 = sps_parser_read_ueg(buffer);
		sps->bit_depth_chroma_minus8 = sps_parser_read_ueg(buffer);
		sps->qpprime_y_zero_transform_bypass_flag = sps_parser_read_bits(buffer, 1);
        sps->seq_scaling_matrix_present_flag = sps_parser_read_bits(buffer, 1);
		if (sps->seq_scaling_matrix_present_flag)
        {
			for (uint8_t i = 0; i < ((sps->chroma_format_idc != 3) ? 8 : 12); i++)
            {
				if (sps_parser_read_bits(buffer, 1))
                {
					if (i < 6)
                    {
						sps_parser_skipScalingList(buffer, 16);
					}
                    else
                    {
						sps_parser_skipScalingList(buffer, 64);
					}
				}
			}
		}
	}

	sps->log2_max_frame_num_minus4 = sps_parser_read_ueg(buffer);
	sps->pic_order_cnt_type = sps_parser_read_ueg(buffer);

	if (sps->pic_order_cnt_type == 0)
    {
		sps->log2_max_pic_order_cnt_lsb_minus4 = sps_parser_read_ueg(buffer);
	}
    else if (sps->pic_order_cnt_type == 1)
    {
		sps->delta_pic_order_always_zero_flag = sps_parser_read_bits(buffer, 1);
		sps->offset_for_non_ref_pic = sps_parser_read_eg(buffer);
		sps->offset_for_top_to_bottom_field = sps_parser_read_eg(buffer);
        sps->num_ref_frames_in_pic_order_cnt_cycle = sps_parser_read_ueg(buffer);
		for (uint32_t i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; ++i)
        {
			sps_parser_read_eg(buffer);
		}
	}

	sps->num_ref_frames = sps_parser_read_ueg(buffer);
	sps->gaps_in_frame_num_value_allowed_flag = sps_parser_read_bits(buffer, 1);
	sps->pic_width_in_mbs_minus_1 = sps_parser_read_ueg(buffer);
	sps->pic_height_in_map_units_minus_1 = sps_parser_read_ueg(buffer);
	sps->frame_mbs_only_flag = sps_parser_read_bits(buffer, 1);
	if (!sps->frame_mbs_only_flag)
    {
        sps->mb_adaptive_frame_field_flag = sps_parser_read_bits(buffer, 1);
    }
	sps->direct_8x8_inference_flag = sps_parser_read_bits(buffer, 1);
    sps->frame_cropping_flag = sps_parser_read_bits(buffer, 1);
	if (sps->frame_cropping_flag)
    {
		sps->frame_crop_left_offset = sps_parser_read_ueg(buffer);
		sps->frame_crop_right_offset = sps_parser_read_ueg(buffer);
		sps->frame_crop_top_offset = sps_parser_read_ueg(buffer);
		sps->frame_crop_bottom_offset = sps_parser_read_ueg(buffer);
	}
    sps->vui_prameters_present_flag = sps_parser_read_bits(buffer, 1);

    sps->width = (((sps->pic_width_in_mbs_minus_1 + 1) * 16) - sps->frame_crop_left_offset * 2 - sps->frame_crop_right_offset * 2);
    sps->height = ((2 - sps->frame_mbs_only_flag) * (sps->pic_height_in_map_units_minus_1 + 1) * 16) - ((sps->frame_mbs_only_flag ? 2 : 4) * (sps->frame_crop_top_offset + sps->frame_crop_bottom_offset));

    return 0;
}

static int
process_sps(char* data, int bytes)
{
    struct sps_t sps;
    struct bits_t bits;

    bits_init(&bits, data, bytes);
    memset(&sps, 0, sizeof(sps));
    //parse_sps(&bits, &sps);
    sps_parser(data, &sps);

    printf("    bits.error                              %d\n", bits.error);
    printf("    bytes left                              %d\n", (int)(bits.data_bytes - bits.offset));
    printf("    forbidden_zero_bit                      %d\n", sps.forbidden_zero_bit);
    printf("    nal_ref_idc                             %d\n", sps.nal_ref_idc);
    printf("    nal_unit_type                           %d\n", sps.nal_unit_type);
    printf("    profile_idc                             %d\n", sps.profile_idc);
    printf("    constraint_set0_flag                    %d\n", sps.constraint_set0_flag);
    printf("    constraint_set1_flag                    %d\n", sps.constraint_set1_flag);
    printf("    constraint_set2_flag                    %d\n", sps.constraint_set2_flag);
    printf("    constraint_set3_flag                    %d\n", sps.constraint_set3_flag);
    printf("    reserved_zero_4bits                     %d\n", sps.reserved_zero_4bits);
    printf("    level_idc                               %d\n", sps.level_idc);
    printf("    seq_parameter_set_id                    %d\n", sps.seq_parameter_set_id);
    printf("    log2_max_frame_num_minus4               %d\n", sps.log2_max_frame_num_minus4);
    printf("    pic_order_cnt_type                      %d\n", sps.pic_order_cnt_type);
    printf("    log2_max_pic_order_cnt_lsb_minus4       %d\n", sps.log2_max_pic_order_cnt_lsb_minus4);
    printf("    delta_pic_order_always_zero_flag        %d\n", sps.delta_pic_order_always_zero_flag);
    printf("    offset_for_non_ref_pic                  %d\n", sps.offset_for_non_ref_pic);
    printf("    offset_for_top_to_bottom_field          %d\n", sps.offset_for_top_to_bottom_field);
    printf("    num_ref_frames_in_pic_order_cnt_cycle   %d\n", sps.num_ref_frames_in_pic_order_cnt_cycle);
    printf("    num_ref_frames                          %d\n", sps.num_ref_frames);
    printf("    gaps_in_frame_num_value_allowed_flag    %d\n", sps.gaps_in_frame_num_value_allowed_flag);
    printf("    pic_width_in_mbs_minus_1                %d\n", sps.pic_width_in_mbs_minus_1);
    printf("    pic_height_in_map_units_minus_1         %d\n", sps.pic_height_in_map_units_minus_1);
    printf("    frame_mbs_only_flag                     %d\n", sps.frame_mbs_only_flag);
    printf("    mb_adaptive_frame_field_flag            %d\n", sps.mb_adaptive_frame_field_flag);
    printf("    direct_8x8_inference_flag               %d\n", sps.direct_8x8_inference_flag);
    printf("    frame_cropping_flag                     %d\n", sps.frame_cropping_flag);
    printf("    frame_crop_left_offset                  %d\n", sps.frame_crop_left_offset);
    printf("    frame_crop_right_offset                 %d\n", sps.frame_crop_right_offset);
    printf("    frame_crop_top_offset                   %d\n", sps.frame_crop_top_offset);
    printf("    frame_crop_bottom_offset                %d\n", sps.frame_crop_bottom_offset);
    printf("    vui_prameters_present_flag              %d\n", sps.vui_prameters_present_flag);
    printf("    aspect_ratio_info_present_flag          %d\n", sps.vui.aspect_ratio_info_present_flag);
    printf("    aspect_ratio_idc                        %d\n", sps.vui.aspect_ratio_idc);
    printf("    sar_width                               %d\n", sps.vui.sar_width);
    printf("    sar_height                              %d\n", sps.vui.sar_height);
    printf("    overscan_info_present_flag              %d\n", sps.vui.overscan_info_present_flag);
    printf("    overscan_appropriate_flag               %d\n", sps.vui.overscan_appropriate_flag);
    printf("    video_signal_type_present_flag          %d\n", sps.vui.video_signal_type_present_flag);
    printf("    video_format                            %d\n", sps.vui.video_format);
    printf("    video_full_range_flag                   %d\n", sps.vui.video_full_range_flag);
    printf("    colour_description_present_flag         %d\n", sps.vui.colour_description_present_flag);
    printf("    colour_primaries                        %d\n", sps.vui.colour_primaries);
    printf("    transfer_characteristics                %d\n", sps.vui.transfer_characteristics);
    printf("    matrix_coefficients                     %d\n", sps.vui.matrix_coefficients);
    printf("    chroma_loc_info_present_flag            %d\n", sps.vui.chroma_loc_info_present_flag);
    printf("    chroma_sample_loc_type_top_field        %d\n", sps.vui.chroma_sample_loc_type_top_field);
    printf("    chroma_sample_loc_type_bottom_field     %d\n", sps.vui.chroma_sample_loc_type_bottom_field);
    printf("    timing_info_present_flag                %d\n", sps.vui.timing_info_present_flag);
    printf("    num_units_in_tick                       %d\n", sps.vui.num_units_in_tick);
    printf("    time_scale                              %d\n", sps.vui.time_scale);
    printf("    fixed_frame_rate_flag                   %d\n", sps.vui.fixed_frame_rate_flag);
    printf("    nal_hrd_parameters_present_flag         %d\n", sps.vui.nal_hrd_parameters_present_flag);
    printf("    vcl_hrd_parameters_present_flag         %d\n", sps.vui.vcl_hrd_parameters_present_flag);
    printf("    low_delay_hrd_flag                      %d\n", sps.vui.low_delay_hrd_flag);
    printf("    pic_struct_present_flag                 %d\n", sps.vui.pic_struct_present_flag);
    printf("    bitstream_restriction_flag              %d\n", sps.vui.bitstream_restriction_flag);
    printf("    motion_vectors_over_pic_boundaries_flag %d\n", sps.vui.motion_vectors_over_pic_boundaries_flag);
    printf("    max_bytes_per_pic_denom                 %d\n", sps.vui.max_bytes_per_pic_denom);
    printf("    max_bits_per_mb_denom                   %d\n", sps.vui.max_bits_per_mb_denom);
    printf("    log2_max_mv_length_horizontal           %d\n", sps.vui.log2_max_mv_length_horizontal);
    printf("    log2_max_mv_length_vertical             %d\n", sps.vui.log2_max_mv_length_vertical);
    printf("    num_reorder_frames                      %d\n", sps.vui.num_reorder_frames);
    printf("    max_dec_frame_buffering                 %d\n", sps.vui.max_dec_frame_buffering);
    printf("    width                                   %d\n", sps.width);
    printf("    height                                  %d\n", sps.height);
    return 0;
}

static int
process_pps(const char* data, int bytes)
{
    return 0;
}

char test[] =
{
    0x67, 0x42, 0xc0, 0x20, 0xda, 0x01, 0x0c, 0x1d,
    0xf9, 0x78, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40,
    0x00, 0x00, 0x0c, 0x23, 0xc6, 0x0c, 0xa8 };

int
main(int argc, char** argv)
{
    int fd;
    char* alloc_data;
    char* data;
    char* end_data;
    int data_bytes;
    int width;
    int height;
    int nal_unit_type;
    int start_code_bytes;
    int nal_bytes;
    char* rbsp;
    int lnal_bytes;
    int rbsp_bytes;

    if (argc < 2)
    {
        printf("error\n");
        return 1;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        printf("error\n");
        return 1;
    }
    data_bytes = 1024 * 1024;
    alloc_data = (char*)malloc(data_bytes);
    while (get_next_frame(fd, alloc_data, &data_bytes, &width, &height) == 0)
    {
        data = alloc_data;
        end_data = data + data_bytes;
        while (data < end_data)
        {
            start_code_bytes = parse_start_code(data, end_data);
            if (start_code_bytes == 0)
            {
                printf("bad start code\n");
                break;
            }
            data += start_code_bytes;
            nal_bytes = get_nal_bytes(data, end_data);

            lnal_bytes = nal_bytes;
            rbsp_bytes = lnal_bytes + 16;
            rbsp = (char*)malloc(rbsp_bytes);
            if (rbsp != NULL)
            {
                if (nal_to_rbsp(data, &lnal_bytes, rbsp, &rbsp_bytes) != -1)
                {
                    printf("  start_code_bytes %d rbsp_bytes %d\n", start_code_bytes, rbsp_bytes);
                    nal_unit_type = rbsp[0] & 0x1F;
                    printf("  nal_unit_type 0x%2.2x\n", nal_unit_type);
                    switch (nal_unit_type)
                    {
                        case 1: /* Coded slice of a non-IDR picture */
                            hexdump(data, 32);
                            break;
                        case 5: /* Coded slice of an IDR picture */
                            hexdump(data, 32);
                            break;
                        case 7: /* Sequence parameter set */
                            hexdump(rbsp, rbsp_bytes);
                            //printf("width = %d\nheight = %d\n", dimensions >> 16, dimensions & 0xFFFF);
                            process_sps(rbsp, rbsp_bytes);
                            break;
                        case 8: /* Picture parameter set */
                            hexdump(rbsp, rbsp_bytes);
                            process_pps(rbsp, rbsp_bytes);
                            break;
                        case 9: /* Access unit delimiter */
                            hexdump(rbsp, rbsp_bytes);
                            break;
                        default:
                            break;
                    }
                }
                free(rbsp);
            }
            data += nal_bytes;
        }
        data_bytes = 1024 * 1024;
    }
    free(alloc_data);
    close(fd);

    printf("end test\n");
#if 0
    lnal_bytes = sizeof(test);
    rbsp_bytes = lnal_bytes + 16;
    rbsp = (char*)malloc(rbsp_bytes);
    if (rbsp != NULL)
    {
        if (nal_to_rbsp(test, &lnal_bytes, rbsp, &rbsp_bytes) != -1)
        {
            hexdump(test, sizeof(test));
            hexdump(rbsp, sizeof(test));
            process_sps(rbsp, rbsp_bytes);
        }
        free(rbsp);
    }
#endif
    return 0;
}
