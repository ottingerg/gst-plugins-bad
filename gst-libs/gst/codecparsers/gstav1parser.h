/*
 * gstav1parser.h
 *
 *  Copyright (C) 2018 Georg Ottinger
 *    Author: Georg Ottinger<g.ottinger@gmx.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef GST_AV1_PARSER_H
#define GST_AV1_PARSER_H

#ifndef GST_USE_UNSTABLE_API
#warning "The AV1 parsing library is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>
#include <gst/codecparsers/codecparsers-prelude.h>

G_BEGIN_DECLS

#define GST_AV1_MAX_OPERATING_POINTS 32
#define GST_AV1_MAX_DECODER_MODEL_OPERATING_POINTS 32

/**
 * GstAV1ColorPrimaries:
 * GST_AV1_CP_BT_709: BT.709
 * GST_AV1_CP_UNSPECIFIED: Unspecified
 * GST_AV1_CP_BT_470_M: BT.470 System M (historical)
 * GST_AV1_CP_BT_470_B_G:BT.470 System B, G (historical),
 * GST_AV1_CP_BT_601: BT.601
 * GST_AV1_CP_SMPTE_240: SMPTE 240
 * GST_AV1_CP_GENERIC_FILM: Generic film (color filters using illuminant C,
 * GST_AV1_CP_BT_2020: BT.2020, BT.2100,
 * GST_AV1_CP_XYZ: SMPTE 428 (CIE 1921 XYZ),
 * GST_AV1_CP_SMPTE_431: SMPTE RP 431-2
 * GST_AV1_CP_SMPTE_432: SMPTE EG 432-1
 * GST_AV1_CP_EBU_3213: EBU Tech. 3213-E
 *
 */

typedef enum {
  GST_AV1_CP_BT_709 = 1,
  GST_AV1_CP_UNSPECIFIED = 2,
  GST_AV1_CP_BT_470_M = 4,
  GST_AV1_CP_BT_470_B_G  = 5,
  GST_AV1_CP_BT_601 = 6,
  GST_AV1_CP_SMPTE_240 = 7,
  GST_AV1_CP_GENERIC_FILM = 8,
  GST_AV1_CP_BT_2020 = 9,
  GST_AV1_CP_XYZ = 10,
  GST_AV1_CP_SMPTE_431 = 11,
  GST_AV1_CP_SMPTE_432 = 12,
  GST_AV1_CP_EBU_3213 = 22,
} GstAV1ColorPrimaries;




/**
 * _GstAV1OperatingPoints:
 * @seq_level_idx: specifies the level that the coded video sequence conforms to.
 * @seq_tier: specifies the tier that the coded video sequence conforms to.
 * @idc: contains a bitmask that indicates which spatial and temporal layers should be decoded
 *       Bit k is equal to 1 if temporal layer k should be decoded (for k between 0 and 7).
 *       Bit j+8 is equal to 1 if spatial layer j should be decoded (for j between 0 and 3).
 *
 */

struct _GstAV1OperatingPoints {
  guint8 seq_level_idx;
  guint8 seq_tier;
  guint16 idc;
};

/**
 * _GstAV1DecoderModelOperatingPoints:
 * @idc: specifies the corresponding operating point for a set of operatingparameters.
 * @param_present_flag: specifies whether param.X is present in the bitstream
 * @param.X: are syntax elements used by the decoder model. The
 *           value does not affect the decoding process.
 *
 */

struct _GstAV1DecoderModelOperatingPoints {
  guint16 idc;
  guint8 param_present_flag;
  struct {
    guint8 initial_display_delay_minus_1;
    guint8 bitrate_minus_1;
    guint8 buffer_size_minus_1;
    guint8 cbr_flag;
    guint8 decoder_buffer_delay;
    guint8 encoder_buffer_delay;
    guint8 low_delay_mode_flag;
  } param;
};

struct _GstAV1DecoderModelInfo {
  guint8 bitrate_scale;
  guint8 buffer_size_scale;
  guint8 encoder_decoder_buffer_delay_length_minus_1;
  guint32 num_units_in_decoding_tick;
  guint8 buffer_removal_delay_length_minus_1;
  guint8 frame_presentation_delay_length_minus_1;
};

struct _GstAV1TimingInfo {
  guint32 num_units_in_display_tick;
  guint32 time_scale;
  guint8 equal_picture_interval;
  guint32 num_ticks_per_picture_minus_1;
};

struct _GstAV1ColorConfig {
  guint8 high_bitdepth;
  guint8 twelve_bit;
  guint8 mono_chrome;
  guint8 color_description_present_flag;
  guint8 color_primaries;
  guint8 transfer_characteristics;
  guint8 matrix_coefficients;
  guint8 color_range;
  guint8 subsampling_x;
  guint8 subsampling_y;
  guint8 chroma_sample_position;
  guint8 separate_uv_delta_q;
}

/**
 * _GstAV1SequenceHeaderOBU:
 * @seq_profile: specifies the features that can be used in the coded video sequence
 * @still_picture: equal to 1 specifies that the bitstream contains only one coded frame.
 * @reduced_still_picture_header: specifies that the syntax elements not needed by a still picture are omitted
 * @operating_points_cnt_minus_1: indicates the number of operating points minus 1 present in this bitstream.
 * @frame_width_bits_minus_1: specifies the number of bits minus 1 used for transmitting the frame width syntax elements.
 * @frame_height_bits_minus_1: specifies the number of bits minus 1 used for transmitting the frame height syntax elements.
 * @max_frame_width_minus_1: specifies the maximum frame width minus 1 for the frames represented by this sequenceheader.
 * @max_frame_height_minus_1: specifies the maximum frame height minus 1 for the frames represented by this sequenceheader.
 * @frame_id_numbers_present_flag: specifies whether frame id numbers are present in the bitstream.
 * @delta_frame_id_length_minus_2: specifies the number of bits minus 2 used to encode delta_frame_id syntax elements.
.* @additional_frame_id_length_minus_1: is used to calculate the number of bits used to encode the frame_id syntax element.
.* @use_128x128_superblock: when equal to 1, indicates that superblocks contain 128x128 luma samples. When equal to 0, it
 *                          indicates that superblocks contain 64x64 luma samples. (The number of contained chroma samples
 *                          depends on subsampling_x and subsampling_y).
 * @enable_filter_intra: equal to 1 specifies that the use_filter_intra syntax element may be present.
 *                       enable_filter_intra equal to 0 specifies that the use_filter_intra syntax element will not be present.
 *
 */

struct _GstAV1SequenceHeaderOBU {
  guint8 seq_profile;
  guint8 still_picture;
  guint8 reduced_still_picture_header;


  guint8 frame_width_bits_minus_1;
  guint8 frame_height_bits_minus_1;
  guint16 max_frame_width_minus_1;
  guint16 max_frame_height_minus_1;
  guint8 frame_id_number_present_flag;
  guint8 delta_frame_id_length_minus_2;
  guint8 additional_frame_id_length_minus_1;
  guint8 use_128x128_superblock;
  guint8 enable_filter_intra;
  //----------------------
  guint8 enable_intra_edge_filter;
  guint8 enable_interintra_compound;
  guint8 enable_masked_compound;
  guint8 enable_warped_motion;
  guint8 enable_dual_filter;
  guint8 enable_order_hint;
  guint8 enable_jnt_comp;
  guint8 enable_ref_frame_mvs;
  guint8 seq_choose_screen_content_tools;
  guint8 seq_force_screen_content_tools;
  guint8 seq_choose_integer_mv;
  guint8 seq_force_integer_mv;
  guint8 order_hint_bits_minus_1;
  guint8 enable_superres;
  guint8 enable_cdef;
  guint8 enable_restoration;

  guint8 film_grain_params_present;


  guint8 operating_points_cnt_minus_1;
  GstAV1OperatingPoints operating_points[GST_AV1_MAX_OPERATING_POINTS];

  guint8 decoder_model_info_present_flag;
  GstAV1DecoderModelInfo decoder_model_info;

  guint8 operating_points_decoder_model_present;
  guint8 operating_points_decoder_model_count_minus_1;
  GstAV1DecoderModelOperatingPoints decoder_model_operating_points[GST_AV1_MAX_DECODER_MODEL_OPERATING_POINTS];

  guint8 timing_info_present_flag;
  GstAV1TimingInfo timing_info;

  GstAV1ColorConfig color_config;
};

GST_CODEC_PARSERS_API
GstAV1Parser *     gst_av1_parser_new (void);

GST_CODEC_PARSERS_API
void               gst_av1_parser_free (GstAV1Parser * parser);

G_END_DECLS

#endif /* GST_AV1_PARSER_H */
