/* Gstreamer
 * Copyright (C) 2018 Georg Ottinger
 *    Author: Georg Ottinger <g.ottinger@gmx.at>
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

#include <gst/check/gstcheck.h>
#include <gst/base/gstbitreader.h>
#include <gst/codecparsers/gstav1parser.h>

static const guint8 aom_testdata_av1_1_b8_01_size_16x16[] = {
  0x12, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x9f, 0xfb, 0xff, 0xf3,
  0x00, 0x80, 0x32, 0xa6, 0x01, 0x10, 0x00, 0x87, 0x80, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x40, 0x00, 0x9e, 0x86, 0x5b, 0xb2, 0x22, 0xb5, 0x58, 0x4d,
  0x68, 0xe6, 0x37, 0x54, 0x42, 0x7b, 0x84, 0xce, 0xdf, 0x9f, 0xec, 0xab,
  0x07, 0x4d, 0xf6, 0xe1, 0x5e, 0x9e, 0x27, 0xbf, 0x93, 0x2f, 0x47, 0x0d,
  0x7b, 0x7c, 0x45, 0x8d, 0xcf, 0x26, 0xf7, 0x6c, 0x06, 0xd7, 0x8c, 0x2e,
  0xf5, 0x2c, 0xb0, 0x8a, 0x31, 0xac, 0x69, 0xf5, 0xcd, 0xd8, 0x71, 0x5d,
  0xaf, 0xf8, 0x96, 0x43, 0x8c, 0x9c, 0x23, 0x6f, 0xab, 0xd0, 0x35, 0x43,
  0xdf, 0x81, 0x12, 0xe3, 0x7d, 0xec, 0x22, 0xb0, 0x30, 0x54, 0x32, 0x9f,
  0x90, 0xc0, 0x5d, 0x64, 0x9b, 0x0f, 0x75, 0x31, 0x84, 0x3a, 0x57, 0xd7,
  0x5f, 0x03, 0x6e, 0x7f, 0x43, 0x17, 0x6d, 0x08, 0xc3, 0x81, 0x8a, 0xae,
  0x73, 0x1c, 0xa8, 0xa7, 0xe4, 0x9c, 0xa9, 0x5b, 0x3f, 0xd1, 0xeb, 0x75,
  0x3a, 0x7f, 0x22, 0x77, 0x38, 0x64, 0x1c, 0x77, 0xdb, 0xcd, 0xef, 0xb7,
  0x08, 0x45, 0x8e, 0x7f, 0xea, 0xa3, 0xd0, 0x81, 0xc9, 0xc1, 0xbc, 0x93,
  0x9b, 0x41, 0xb1, 0xa1, 0x42, 0x17, 0x98, 0x3f, 0x1e, 0x95, 0xdf, 0x68,
  0x7c, 0xb7, 0x98, 0x12, 0x00, 0x32, 0x4b, 0x30, 0x03, 0xc3, 0x00, 0xa7,
  0x2e, 0x46, 0x8a, 0x00, 0x00, 0x03, 0x00, 0x00, 0x50, 0xc0, 0x20, 0x00,
  0xf0, 0xb1, 0x2f, 0x43, 0xf3, 0xbb, 0xe6, 0x5c, 0xbe, 0xe6, 0x53, 0xbc,
  0xaa, 0x61, 0x7c, 0x7e, 0x0a, 0x04, 0x1b, 0xa2, 0x87, 0x81, 0xe8, 0xa6,
  0x85, 0xfe, 0xc2, 0x71, 0xb9, 0xf8, 0xc0, 0x78, 0x9f, 0x52, 0x4f, 0xa7,
  0x8f, 0x55, 0x96, 0x79, 0x90, 0xaa, 0x2b, 0x6d, 0x0a, 0xa7, 0x05, 0x2a,
  0xf8, 0xfc, 0xc9, 0x7d, 0x9d, 0x4a, 0x61, 0x16, 0xb1, 0x65
};

GST_START_TEST (test_av1_parse_aom_testdata_av1_1_b8_01_size_16x16)
{
  GstAV1Parser *parser;
  GstBitReader *br;
  GstAV1OBUHeader obu_header;
  GstAV1SequenceHeaderOBU seq_header;
  GstAV1FrameOBU frame;


  bzero (&obu_header, sizeof (obu_header));
  bzero (&seq_header, sizeof (seq_header));
  bzero (&frame, sizeof (frame));

  parser = gst_av1_parser_new ();
  br = gst_bit_reader_new (aom_testdata_av1_1_b8_01_size_16x16,
      sizeof (aom_testdata_av1_1_b8_01_size_16x16));

  /* 1st OBU should be OBU_TEMPORAL_DELIMITER */
  gst_av1_parse_obu_header (parser, br, &obu_header, 0);
  assert_equals_int (obu_header.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);
  assert_equals_int (obu_header.obu_extention_flag, 0);
  assert_equals_int (obu_header.obu_has_size_field, 1);
  assert_equals_int (obu_header.obu_size, 0);

  gst_av1_parse_temporal_delimiter_obu (parser, br);


  /* 2nd OBU should be OBU_SEQUENCE_HEADER */
  gst_av1_parse_obu_header (parser, br, &obu_header, 0);
  assert_equals_int (obu_header.obu_type, GST_AV1_OBU_SEQUENCE_HEADER);
  assert_equals_int (obu_header.obu_extention_flag, 0);
  assert_equals_int (obu_header.obu_has_size_field, 1);
  assert_equals_int (obu_header.obu_size, 10);

  gst_av1_parse_sequence_header_obu (parser, br, &seq_header);
  assert_equals_int (seq_header.seq_profile, 0);
  assert_equals_int (seq_header.still_picture, 0);
  assert_equals_int (seq_header.reduced_still_picture_header, 0);
  assert_equals_int (seq_header.timing_info_present_flag, 0);
  assert_equals_int (seq_header.initial_display_delay_present_flag, 0);
  assert_equals_int (seq_header.operating_points_cnt_minus_1, 0);
  assert_equals_int (seq_header.operating_points[0].idc, 0);
  assert_equals_int (seq_header.operating_points[0].seq_level_idx, 0);
  assert_equals_int (seq_header.frame_width_bits_minus_1, 3);
  assert_equals_int (seq_header.frame_height_bits_minus_1, 3);
  assert_equals_int (seq_header.max_frame_width_minus_1, 15);
  assert_equals_int (seq_header.max_frame_height_minus_1, 15);
  assert_equals_int (seq_header.frame_id_numbers_present_flag, 0);
  assert_equals_int (seq_header.use_128x128_superblock, 1);
  assert_equals_int (seq_header.enable_filter_intra, 1);
  assert_equals_int (seq_header.enable_intra_edge_filter, 1);
  assert_equals_int (seq_header.enable_interintra_compound, 1);
  assert_equals_int (seq_header.enable_masked_compound, 1);
  assert_equals_int (seq_header.enable_warped_motion, 1);
  assert_equals_int (seq_header.enable_dual_filter, 1);
  assert_equals_int (seq_header.enable_order_hint, 1);
  assert_equals_int (seq_header.enable_jnt_comp, 1);
  assert_equals_int (seq_header.enable_ref_frame_mvs, 1);
  assert_equals_int (seq_header.seq_choose_screen_content_tools, 1);
  assert_equals_int (seq_header.seq_choose_integer_mv, 1);
  assert_equals_int (seq_header.order_hint_bits_minus_1, 6);
  assert_equals_int (seq_header.enable_superres, 0);
  assert_equals_int (seq_header.enable_cdef, 1);
  assert_equals_int (seq_header.enable_restoration, 1);
  assert_equals_int (seq_header.color_config.high_bitdepth, 0);
  assert_equals_int (seq_header.color_config.mono_chrome, 0);
  assert_equals_int (seq_header.color_config.color_description_present_flag, 0);
  assert_equals_int (seq_header.color_config.chroma_sample_position,
      GST_AV1_CSP_UNKNOWN);
  assert_equals_int (seq_header.color_config.separate_uv_delta_q, 0);
  assert_equals_int (seq_header.film_grain_params_present, 0);




  /* 3rd OBU should be GST_AV1_OBU_FRAME */
  gst_av1_parse_obu_header (parser, br, &obu_header, 0);
  assert_equals_int (obu_header.obu_type, GST_AV1_OBU_FRAME);
  assert_equals_int (obu_header.obu_extention_flag, 0);
  assert_equals_int (obu_header.obu_has_size_field, 1);
  assert_equals_int (obu_header.obu_size, 166);

  gst_av1_parse_frame_obu (parser, br, &frame);
  assert_equals_int (frame.frame_header.show_existing_frame, 0);
  assert_equals_int (frame.frame_header.frame_type, GST_AV1_KEY_FRAME);
  assert_equals_int (frame.frame_header.show_frame, 1);
  assert_equals_int (frame.frame_header.disable_cdf_update, 0);
  assert_equals_int (frame.frame_header.allow_screen_content_tools, 0);
  assert_equals_int (frame.frame_header.frame_size_override_flag, 0);
  assert_equals_int (frame.frame_header.order_hint, 0);
  assert_equals_int (frame.frame_header.render_and_frame_size_different, 0);
  assert_equals_int (frame.frame_header.disable_frame_end_update_cdf, 0);
  assert_equals_int (frame.frame_header.quantization_params.base_q_idx, 15);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_ydc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_udc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_uac, 0);
  assert_equals_int (frame.frame_header.quantization_params.using_qmatrix, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_present, 0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[0],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[1],
      0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_sharpness, 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_delta_enabled, 1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_delta_update, 1);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[0],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[1],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[2],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[3],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[4],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[5],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[6],
      0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.update_mode_deltas[0], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.update_mode_deltas[1], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_damping_minus_3, 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_bits, 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_pri_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_sec_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_pri_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_sec_strength[0], 1);
  assert_equals_int (frame.frame_header.
      loop_restoration_params.frame_restoration_type[0],
      GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.
      loop_restoration_params.frame_restoration_type[1],
      GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.
      loop_restoration_params.frame_restoration_type[2],
      GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.tx_mode_select, 0);
  assert_equals_int (frame.frame_header.reduced_tx_set, 0);


  /* 4th OBU should be OBU_TEMPORAL_DELIMITER */
  gst_av1_parse_obu_header (parser, br, &obu_header, 0);
  assert_equals_int (obu_header.obu_type, GST_AV1_OBU_TEMPORAL_DELIMITER);
  assert_equals_int (obu_header.obu_extention_flag, 0);
  assert_equals_int (obu_header.obu_has_size_field, 1);
  assert_equals_int (obu_header.obu_size, 0);

  gst_av1_parse_temporal_delimiter_obu (parser, br);


  /* 5th OBU should be GST_AV1_OBU_FRAME */
  gst_av1_parse_obu_header (parser, br, &obu_header, 0);
  assert_equals_int (obu_header.obu_type, GST_AV1_OBU_FRAME);
  assert_equals_int (obu_header.obu_extention_flag, 0);
  assert_equals_int (obu_header.obu_has_size_field, 1);
  assert_equals_int (obu_header.obu_size, 75);

  gst_av1_parse_frame_obu (parser, br, &frame);
  assert_equals_int (frame.frame_header.show_existing_frame, 0);
  assert_equals_int (frame.frame_header.frame_type, GST_AV1_INTER_FRAME);
  assert_equals_int (frame.frame_header.show_frame, 1);
  assert_equals_int (frame.frame_header.error_resilient_mode, 0);
  assert_equals_int (frame.frame_header.disable_cdf_update, 0);
  assert_equals_int (frame.frame_header.allow_screen_content_tools, 0);
  assert_equals_int (frame.frame_header.frame_size_override_flag, 0);
  assert_equals_int (frame.frame_header.order_hint, 1);
  assert_equals_int (frame.frame_header.primary_ref_frame, 7);
  assert_equals_int (frame.frame_header.refresh_frame_flags, 12);
  assert_equals_int (frame.frame_header.frame_refs_short_signaling, 0);
  assert_equals_int (frame.frame_header.ref_frame_idx[0], 0);
  assert_equals_int (frame.frame_header.ref_frame_idx[1], 1);
  assert_equals_int (frame.frame_header.ref_frame_idx[2], 2);
  assert_equals_int (frame.frame_header.ref_frame_idx[3], 3);
  assert_equals_int (frame.frame_header.ref_frame_idx[4], 4);
  assert_equals_int (frame.frame_header.ref_frame_idx[5], 5);
  assert_equals_int (frame.frame_header.ref_frame_idx[6], 6);
  assert_equals_int (frame.frame_header.allow_high_precision_mv, 1);
  assert_equals_int (frame.frame_header.is_filter_switchable, 0);
  assert_equals_int (frame.frame_header.interpolation_filter,
      GST_AV1_INTERPOLATION_FILTER_EIGHTTAP);
  assert_equals_int (frame.frame_header.is_motion_mode_switchable, 1);
  assert_equals_int (frame.frame_header.use_ref_frame_mvs, 1);
  assert_equals_int (frame.frame_header.disable_frame_end_update_cdf, 0);
  assert_equals_int (frame.frame_header.quantization_params.base_q_idx, 20);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_ydc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_udc, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_uac, 0);
  assert_equals_int (frame.frame_header.quantization_params.using_qmatrix, 0);
  assert_equals_int (frame.frame_header.
      segmentation_params.segmentation_enabled, 0);
  assert_equals_int (frame.frame_header.quantization_params.delta_q_present, 0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[0],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.loop_filter_level[1],
      0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_sharpness, 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_delta_enabled, 1);
  assert_equals_int (frame.frame_header.
      loop_filter_params.loop_filter_delta_update, 1);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[0],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[1],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[2],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[3],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[4],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[5],
      0);
  assert_equals_int (frame.frame_header.loop_filter_params.update_ref_deltas[6],
      0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.update_mode_deltas[0], 0);
  assert_equals_int (frame.frame_header.
      loop_filter_params.update_mode_deltas[1], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_damping_minus_3, 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_bits, 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_pri_strength[0], 1);
  assert_equals_int (frame.frame_header.cdef_params.cdef_y_sec_strength[0], 1);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_pri_strength[0], 0);
  assert_equals_int (frame.frame_header.cdef_params.cdef_uv_sec_strength[0], 4);
  assert_equals_int (frame.frame_header.
      loop_restoration_params.frame_restoration_type[0],
      GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.
      loop_restoration_params.frame_restoration_type[1],
      GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.
      loop_restoration_params.frame_restoration_type[2],
      GST_AV1_FRAME_RESTORE_NONE);
  assert_equals_int (frame.frame_header.tx_mode_select, 0);
  assert_equals_int (frame.frame_header.reference_select, 0);
  assert_equals_int (frame.frame_header.allow_warped_motion, 1);
  assert_equals_int (frame.frame_header.reduced_tx_set, 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[1], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[2], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[3], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[4], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[5], 0);
  assert_equals_int (frame.frame_header.global_motion_params.is_global[6], 0);


  gst_bit_reader_free (br);
  gst_av1_parser_free (parser);

}


GST_END_TEST;

static Suite *
av1parsers_suite (void)
{
  Suite *s = suite_create ("AV1 Parser library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_av1_parse_aom_testdata_av1_1_b8_01_size_16x16);

  return s;
}

GST_CHECK_MAIN (av1parsers);
