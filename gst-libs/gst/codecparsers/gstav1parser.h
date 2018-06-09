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
 * GstAV1TransferCharacteristics:
 *
 * GST_AV1_TC_RESERVED_0: For future use
 * GST_AV1_TC_BT_709: BT.709
 * GST_AV1_TC_UNSPECIFIED: Unspecified
 * GST_AV1_TC_RESERVED_3: For future use
 * GST_AV1_TC_BT_470_M: BT.470 System M (historical)
 * GST_AV1_TC_BT_470_B_G: BT.470 System B, G (historical)
 * GST_AV1_TC_BT_601: BT.601
 * GST_AV1_TC_SMPTE_240: SMPTE 240 M
 * GST_AV1_TC_LINEAR: Linear
 * GST_AV1_TC_LOG_100: Logarithmic (100 : 1 range)
 * GST_AV1_TC_LOG_100_SQRT10: Logarithmic (100 * Sqrt(10) : 1 range)
 * GST_AV1_TC_IEC_61966: IEC 61966-2-4
 * GST_AV1_TC_BT_1361: BT.1361
 * GST_AV1_TC_SRGB: sRGB or sYCC
 * GST_AV1_TC_BT_2020_10_BIT: BT.2020 10-bit systems
 * GST_AV1_TC_BT_2020_12_BIT: BT.2020 12-bit systems
 * GST_AV1_TC_SMPTE_2084: SMPTE ST 2084, ITU BT.2100 PQ
 * GST_AV1_TC_SMPTE_428: SMPTE ST 428
 * GST_AV1_TC_HLG: BT.2100 HLG, ARIB STD-B67
 */

typedef enum {
  GST_AV1_TC_RESERVED_0 = 0,
  GST_AV1_TC_BT_709 = 1,
  GST_AV1_TC_UNSPECIFIED = 2,
  GST_AV1_TC_RESERVED_3 = 3,
  GST_AV1_TC_BT_470_M = 4,
  GST_AV1_TC_BT_470_B_G = 5,
  GST_AV1_TC_BT_601 = 6,
  GST_AV1_TC_SMPTE_240 = 7,
  GST_AV1_TC_LINEAR = 8,
  GST_AV1_TC_LOG_100 = 9,
  GST_AV1_TC_LOG_100_SQRT10 = 10,
  GST_AV1_TC_IEC_61966 = 11,
  GST_AV1_TC_BT_1361 = 12,
  GST_AV1_TC_SRGB = 13,
  GST_AV1_TC_BT_2020_10_BIT = 14,
  GST_AV1_TC_BT_2020_12_BIT = 15,
  GST_AV1_TC_SMPTE_2084 = 16,
  GST_AV1_TC_SMPTE_428 = 17,
  GST_AV1_TC_HLG = 18,
} GstAV1TransferCharacteristics;


/**
 * GstAV1MatrixCoefficients:
 * GST_AV1_MC_IDENTITY: Identity matrix
 * GST_AV1_MC_BT_709: BT.709
 * GST_AV1_MC_UNSPECIFIED: Unspecified
 * GST_AV1_MC_RESERVED_3: For future use
 * GST_AV1_MC_FCC: US FCC 73.628
 * GST_AV1_MC_BT_470_B_G: BT.470 System B, G (historical)
 * GST_AV1_MC_BT_601: BT.601
 * GST_AV1_MC_SMPTE_240: SMPTE 240 M
 * GST_AV1_MC_SMPTE_YCGCO: YCgCo
 * GST_AV1_MC_BT_2020_NCL: BT.2020 non-constant luminance, BT.2100 YCbCr
 * GST_AV1_MC_BT_2020_CL: BT.2020 constant luminance
 * GST_AV1_MC_SMPTE_2085: SMPTE ST 2085 YDzDx
 * GST_AV1_MC_CHROMAT_NCL: Chromaticity-derived non-constant luminance
 * GST_AV1_MC_CHROMAT_CL: Chromaticity-derived constant luminancw
 * GST_AV1_MC_ICTCP: BT.2100 ICtCp
 */

typedef enum {
  GST_AV1_MC_IDENTITY = 0,
  GST_AV1_MC_BT_709 = 1,
  GST_AV1_MC_UNSPECIFIED = 2,
  GST_AV1_MC_RESERVED_3 = 3,
  GST_AV1_MC_FCC = 4,
  GST_AV1_MC_BT_470_B_G = 5,
  GST_AV1_MC_BT_601 = 6,
  GST_AV1_MC_SMPTE_240 = 7,
  GST_AV1_MC_SMPTE_YCGCO = 8,
  GST_AV1_MC_BT_2020_NCL = 9,
  GST_AV1_MC_BT_2020_CL = 10,
  GST_AV1_MC_SMPTE_2085 = 11,
  GST_AV1_MC_CHROMAT_NCL = 12,
  GST_AV1_MC_CHROMAT_CL = 13,
  GST_AV1_MC_ICTCP = 14,
} GstAV1MatrixCoefficients;

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

/**
 * _GstAV1DecoderModelInfo:
 * @bitrate_scale, buffer_size_scale, encoder_decoder_buffer_delay_length_minus_1, num_units_in_decoding_tick,
 *  buffer_removal_delay_length_minus_1, and frame_presentation_delay_length_minus_1: These are syntax elements used
 *  by the decoder model. They do not affect the decoding process.
 */

struct _GstAV1DecoderModelInfo {
  guint8 bitrate_scale;
  guint8 buffer_size_scale;
  guint8 encoder_decoder_buffer_delay_length_minus_1;
  guint32 num_units_in_decoding_tick;
  guint8 buffer_removal_delay_length_minus_1;
  guint8 frame_presentation_delay_length_minus_1;
};

/**
 * _GstAV1TimingInfo:
 *
 * @num_units_in_display_tick: is the number of time units of a clock operating at the frequency time_scale Hz that
 *                             corresponds to one increment of a clock tick counter. A clock tick, in seconds, is
 *                             equal to num_units_in_display_tick divided by time_scale.
 *                             It is a requirement of bitstream conformance that num_units_in_display_tick is greater than 0.
 * @time_scale: is the number of time units that pass in one second.
 *              It is a requirement of bitstream conformance that time_scale is greater than 0
 * @equal_picture_interval: equal to 1 indicates that pictures should be displayed according to their output order with the
 *                          number of ticks between two consecutive pictures (without dropping frames) specified by
 *                          num_ticks_per_picture_minus_1 + 1. equal_picture_interval equal to 0 indicates that the interval
 *                          between two consecutive pictures is not specified.
 * @num_ticks_per_picture_minus_1: plus 1 specifies the number of clock ticks corresponding to output time between two
 *                                 consecutive pictures in the output order.
 *                                 It is a requirement of bitstream conformance that the value of num_ticks_per_picture_minus_1
 *                                 shall be in the range of 0 to (1 << 32) − 2, inclusive.
 *
 */

struct _GstAV1TimingInfo {
  guint32 num_units_in_display_tick;
  guint32 time_scale;
  guint8 equal_picture_interval;
  guint32 num_ticks_per_picture_minus_1;
};

/**
 * _GstAV1TimingInfo:
 *
 * @high_bitdepth and twelve_bit: are syntax elements which, together with seq_profile, determine the bit depth.
 * @mono_chrome: equal to 1 indicates that the video does not contain U and V color planes. mono_chrome equal to 0
 *               indicates that the video contains Y, U, and V color planes.
 * @color_description_present_flag: equal to 1 specifies that color_primaries, transfer_characteristics, and
 *                                  matrix_coefficients are present. color_description_present_flag equal to 0 specifies
 *                                  that color_primaries, transfer_characteristics and matrix_coefficients are not present.
 * @color_primaries: is an integer that is defined by the “Color primaries” section of ISO/IEC 23091-4/ITU-T H.273.
 * @transfer_characteristics: is an integer that is defined by the “Transfer characteristics” section of ISO/IEC 23091-4
                              /ITU-T H.273.
 * @matrix_coefficients: is an integer that is defined by the “Matrix coefficients” section of ISO/IEC 23091-4/ITU-T H.273.
 * @color_range: is a binary value that is associated with the VideoFullRangeFlag variable specified in ISO/IEC 23091-4/ITU-T
 *                 H.273. color range equal to 0 shall be referred to as the studio swing representation and color range equal
 *              to 1 shall be referred to as the full swing representation for all intents relating to this specification.
 * @subsampling_x, subsampling_y: specify the chroma subsampling format.
 *                                If matrix_coefficients is equal to GST_AV1_MC_IDENTITY, it is a requirement of bitstream
 *                                conformance that subsampling_x is equal to 0 and subsampling_y is equal to 0.
 * @chroma_sample_position specifies the sample position for subsampled streams:
 * @separate_uv_delta_q: equal to 1 indicates that the U and V planes may have separate delta quantizer values. separate_uv_delta_q
 *                      equal to 0 indicates that the U and V planes will share the same delta quantizer value.
 *
 */

struct _GstAV1ColorConfig {
  guint8 high_bitdepth;
  guint8 twelve_bit;
  guint8 mono_chrome;
  guint8 color_description_present_flag;
  GstAV1ColorPrimaries color_primaries;
  GstAV1TransferCharacteristics transfer_characteristics;
  GstAV1MatrixCoefficients matrix_coefficients;
  guint8 color_range;
  guint8 subsampling_x;
  guint8 subsampling_y;
  GstAV1ChromaSamplePosition chroma_sample_position;
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
 * @enable_intra_edge_filter: specifies whether the intra edge filtering process should be enabled.
 * @enable_interintra_compound: equal to 1 specifies that the mode info for inter blocks may contain the syntax element
 *                              interintra. enable_interintra_compound equal to 0 specifies that the syntax element interintra
 *                              will not be present.
 * @enable_masked_compound: equal to 1 specifies that the mode info for inter blocks may contain the syntax element
 *                          compound_type. enable_masked_compound equal to 0 specifies that the syntax element compound_type
 *                          will not be present.
 * @enable_warped_motion: equal to 1 indicates that the allow_warped_motion syntax element may be present. enable_warped_motion
 *                        equal to 0 indicates that the allow_warped_motion syntax element will not be present.
 * @enable_order_hint: equal to 1 indicates that tools based on the values of order hints may be used. enable_order_hint
 *                     equal to 0 indicates that tools based on order hints are disabled.
 * @enable_dual_filter: equal to 1 indicates that the inter prediction filter type may be specified independently in the horizontal
 *                      and vertical directions. If the flag is equal to 0, only one filter type may be specified, which is then
 *                      used in both directions.
 * @enable_jnt_comp: equal to 1 indicates that the distance weights process may be used for inter prediction.
 * @enable_ref_frame_mvs: equal to 1 indicates that the use_ref_frame_mvs syntax element may be present. enable_ref_frame_mvs
 *                        equal to 0 indicates that the use_ref_frame_mvs syntax element will not be present.
 * @seq_choose_screen_content_tools: equal to 0 indicates that the seq_force_screen_content_tools syntax element will be
 *                                   present. seq_choose_screen_content_tools equal to 1 indicates that seq_force_screen_content_tools
 *                                   should be set equal to SELECT_SCREEN_CONTENT_TOOLS.
 * @seq_force_screen_content_tools: equal to SELECT_SCREEN_CONTENT_TOOLS indicates that the allow_screen_content_tools syntax element
 *                                  will be present in the frame header. Otherwise, seq_force_screen_content_tools contains the value
 *                                  for allow_screen_content_tools.
 * @seq_choose_integer_mv: equal to 0 indicates that the seq_force_integer_mv syntax element will be present.seq_choose_integer_mv
 *                         equal to 1 indicates that seq_force_integer_mv should be set equal to SELECT_INTEGER_MV.
 * @seq_force_integer_mv: equal to SELECT_INTEGER_MV indicates that the force_integer_mv syntax element will be present in the frame
 *                        header (providing allow_screen_content_tools is equal to 1). Otherwise, seq_force_integer_mv contains the
 *                        value for force_integer_mv.
 * @order_hint_bits_minus_1: is used to compute OrderHintBits.
 * @enable_superres: equal to 1 specifies that the use_superres syntax element will be present in the uncompressed header.
 *                   enable_superres equal to 0 specifies that the use_superres syntax element will not be present (instead
 *                   use_superres will be set to 0 in the uncompressed header without being read).
 * @enable_cdef: equal to 1 specifies that cdef filtering may be enabled. enable_cdef equal to 0 specifies that cdef filtering is
 *               disabled.
 * @enable_restoration: equal to 1 specifies that loop restoration filtering may be enabled. enable_restoration equal to 0 specifies
 *                      that loop restoration filtering is disabled.
 * @film_grain_params_present: specifies whether film grain parameters are present in the bitstream.
 * @operating_points_cnt_minus_1: indicates the number of operating points minus 1 present in this bitstream.
 * @operating_points[]: specifies the corresponding operating point for a set of operating parameters.
 * @decoder_model_info_present_flag: specifies whether the decoder model info is present in the bitstream.
 * @decoder_model_info: holds information about the decoder model.
 * @operating_points_decoder_model_present: specifies whether decoder operating points are present in the bitstream.
 * @operating_points_decoder_model_count_minus_1: plus 1 specifies the number of decoder model operating points
 *                                                present in the bitstream.
 * @decoder_model_operating_points: holds the decoder model operating points.
 * @timing_info_present_flag specifies whether timing info is present in the bitstream.
 * @timing_info: holds the timing information.
 * @color_config: hold the color configuration.
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
