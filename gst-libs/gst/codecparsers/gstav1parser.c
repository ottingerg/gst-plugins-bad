/* gstav1parser.c
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
/**
 * SECTION:gstav1parser
 * @title: GstAV1Parser
 * @short_description: Convenience library for parsing AV1 video bitstream.
 *
 * For more details about the structures, you can refer to the
 * specifications: https://aomediacodec.github.io/av1-spec/av1-spec.pdf
 */

 /* TODO: Annex B */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <gst/base/gstbitreader.h>
#include "gstav1parser.h"

#define GST_AV1_DEBUG_HELPER() GST_DEBUG("entering function %s", __func__)
#define GST_AV1_EVAL_RETVAL_LOGGED(ret) if (ret != GST_AV1_PARSER_OK) { GST_LOG("Parser-Error(%d) at line %d",(gint)ret, __LINE__); return ret; }


#define gst_av1_read_bit(br)  gst_av1_read_bits(br, 1)

#ifdef __GNUC__
  //GNU C allows using compound statements as expressions,thus returning calling function on error is possible
#define gst_av1_read_bits(br, nbits) ({GstAV1ParserResult _br_retval; guint64 _br_bits=gst_av1_read_bits_checked(br,(nbits),&_br_retval); GST_AV1_EVAL_RETVAL_LOGGED(_br_retval); _br_bits;})
#else
  //on other C compilers the retval is not evaluated
#define gst_av1_read_bits(br, nbits) gst_bit_reader_get_bits_uint64_unchecked(br, nbits)
#endif



static guint64
gst_av1_read_bits_checked (GstBitReader * br, guint nbits,
    GstAV1ParserResult * retval)
{
  guint64 read_bits;
  gboolean result;

  result = gst_bit_reader_get_bits_uint64 (br, &read_bits, nbits);

  if (result == TRUE) {
    *retval = GST_AV1_PARSER_OK;
    return read_bits;
  } else {
    *retval = GST_AV1_PARSER_READBITS_ERROR;
    return read_bits;
  }
}


#define gst_av1_bit_reader_skip_bytes(br,nbytes) gst_av1_bit_reader_skip(br,(nbytes)*8)

#define gst_av1_bit_reader_skip(br, nbits) if (!gst_bit_reader_skip(br,(nbits))) { \
                                             GST_LOG("Skip Bit Error at line %d",__LINE__); \
                                             return GST_AV1_PARSER_SKIPBITS_ERROR; \
                                           }

#define gst_av1_bit_reader_skip_to_byte(br) if (!gst_bit_reader_skip_to_byte(br)) { \
                                              GST_LOG("Skip to Byte Error at line %d",__LINE__); \
                                              return GST_AV1_PARSER_SKIPBITS_ERROR; \
                                            }

#define gst_av1_bit_reader_get_pos(br) gst_bit_reader_get_pos(br)


/*************************************
 *                                   *
 * Helperfunctions                   *
 *                                   *
 *************************************/


static gint
gst_av1_helpers_FloorLog2 (guint32 x)
{
  gint s = 0;

  while (x != 0) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}

static gint
gst_av1_helper_TileLog2 (gint blkSize, gint target)
{
  gint k;
  for (k = 0; (blkSize << k) < target; k++) {
  }
  return k;
}

static gint
gst_av1_helper_InverseRecenter (gint r, gint v)
{
  if (v > 2 * r)
    return v;
  else if (v & 1)
    return r - ((v + 1) >> 1);
  else
    return r + (v >> 1);
}

static gint
gst_av1_helper_Max (gint x, gint y)
{
  if (x >= y)
    return x;
  return y;
}

static gint
gst_av1_helper_Min (gint x, gint y)
{
  if (x <= y)
    return x;
  return y;
}

static gint
gst_av1_helper_Clip3 (gint x, gint y, gint z)
{
  if (z < x)
    return x;
  else if (z > y)
    return y;

  return z;
}

/*************************************
 *                                   *
 * Bitstream Functions               *
 *                                   *
 *************************************/


static guint32
gst_av1_bitstreamfn_leb128 (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 leb128_byte;
  guint64 value = 0;
  gint i;

  GST_AV1_DEBUG_HELPER ();

  for (i = 0; i < 8; i++) {

    leb128_byte = gst_av1_read_bits_checked (br, 8, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    value |= (((gint) leb128_byte & 0x7f) << (i * 7));
    if (!(leb128_byte & 0x80)) {
      break;
    }
  }
  if (value < GUINT32_MAX) {    //check for bitstream conformance see chapter4.10.5
    return (guint32) value;
  } else {
    *retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    return 0;
  }
}

static guint32
gst_av1_bitstreamfn_uvlc (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 leadingZero = 0;
  guint32 readv;
  guint32 value;
  gboolean done;


  GST_AV1_DEBUG_HELPER ();

  while (1) {
    done = gst_av1_read_bits_checked (br, 1, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    if (done) {
      break;
    }
    leadingZero++;
  }
  if (leadingZero >= 32) {
    value = GUINT32_MAX;
    return value;
  }
  readv = gst_av1_read_bits_checked (br, leadingZero, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  value = readv + (1 << leadingZero) - 1;

  return value;
}


static guint32
gst_av1_bitstreamfn_su (GstBitReader * br, guint8 n,
    GstAV1ParserResult * retval)
{
  guint32 v, signMask;

  GST_AV1_DEBUG_HELPER ();

  v = gst_av1_read_bits_checked (br, n, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  signMask = 1 << (n - 1);
  if (v & signMask)
    return v - 2 * signMask;
  else
    return v;
}

static guint8
gst_av1_bitstreamfn_ns (GstBitReader * br, guint8 n,
    GstAV1ParserResult * retval)
{
  gint w, m, v;
  gint extra_bit;

  GST_AV1_DEBUG_HELPER ();


  w = gst_av1_helpers_FloorLog2 (n) + 1;
  m = (1 << w) - n;
  v = gst_av1_read_bits_checked (br, w - 1, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  if (v < m)
    return v;
  extra_bit = gst_av1_read_bits_checked (br, 1, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  return (v << 1) - m + extra_bit;
}

static guint
gst_av1_bitstreamfn_le (GstBitReader * br, guint8 n,
    GstAV1ParserResult * retval)
{
  guint t = 0;
  guint8 byte;
  gint i;


  GST_AV1_DEBUG_HELPER ();

  for (i = 0; i < n; i++) {
    byte = gst_av1_read_bits_checked (br, 8, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    t += (byte << (i * 8));
  }
  return t;
}

static gint8
gst_av1_bitstreamfn_delta_q (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 delta_coded;

  GST_AV1_DEBUG_HELPER ();

  delta_coded = gst_av1_read_bits_checked (br, 1, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  if (delta_coded) {
    gint delta_q = gst_av1_bitstreamfn_su (br, 7, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;
    return delta_q;
  } else {
    return 0;
  }

  return GST_AV1_PARSER_OK;
}

/*************************************
 *                                   *
 * Parser Functions                  *
 *                                   *
 *************************************/


GstAV1ParserResult
gst_av1_parse_obu_header (GstAV1Parser * parser, GstBitReader * br,
    GstAV1OBUHeader * obu_header, GstAV1Size annexb_sz)
{
  GstAV1ParserResult retval;
  GstAV1Size skip_trailing_bits;

  GST_AV1_DEBUG_HELPER ();

  bzero (obu_header, sizeof (GstAV1OBUHeader));

  parser->state.last_position = parser->state.current_position;
  parser->state.current_position = gst_av1_bit_reader_get_pos (br);

  if (parser->state.last_obu_size) {
    skip_trailing_bits =
        (parser->state.last_obu_size * 8) -
        (GstAV1Size) (parser->state.current_position -
        parser->state.last_position -
        parser->state.last_obu_had_size_filed * 8 - 8);
    gst_av1_bit_reader_skip (br, skip_trailing_bits);
  }

  obu_header->obu_forbidden_bit = gst_av1_read_bit (br);

  if (obu_header->obu_forbidden_bit != 0)
    return GST_AV1_PARSER_BITSTREAM_ERROR;

  obu_header->obu_type = gst_av1_read_bits (br, 4);
  obu_header->obu_extention_flag = gst_av1_read_bit (br);
  obu_header->obu_has_size_field = gst_av1_read_bit (br);
  obu_header->obu_reserved_1bit = gst_av1_read_bit (br);

  if (obu_header->obu_extention_flag) {
    obu_header->obu_temporal_id = gst_av1_read_bits (br, 3);
    obu_header->obu_spatial_id = gst_av1_read_bits (br, 2);
    obu_header->obu_extension_header_reserved_3bits = gst_av1_read_bits (br, 3);

    parser->state.temporal_id = obu_header->obu_temporal_id;
    parser->state.spatial_id = obu_header->obu_spatial_id;
  } else {
    parser->state.temporal_id = 0;
    parser->state.spatial_id = 0;
  }

  if (obu_header->obu_has_size_field) {
    obu_header->obu_size = gst_av1_bitstreamfn_leb128 (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);
    parser->state.last_obu_had_size_filed = 1;
  } else {
    obu_header->obu_size = annexb_sz;
    parser->state.last_obu_had_size_filed = 0;
  }

  parser->state.last_obu_size = obu_header->obu_size;

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_color_config (GstAV1Parser * parser, GstBitReader * br,
    GstAV1ColorConfig * color_config)
{
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  color_config->high_bitdepth = gst_av1_read_bit (br);
  if (seq_header->seq_profile == 2 && color_config->high_bitdepth) {
    color_config->twelve_bit = gst_av1_read_bit (br);
    color_config->BitDepth = color_config->twelve_bit ? 12 : 10;
  } else if (seq_header->seq_profile <= 2) {
    color_config->BitDepth = color_config->high_bitdepth ? 10 : 8;
  }

  if (seq_header->seq_profile == 1)
    color_config->mono_chrome = 0;
  else
    color_config->mono_chrome = gst_av1_read_bit (br);

  color_config->NumPlanes = color_config->mono_chrome ? 1 : 3;

  color_config->color_description_present_flag = gst_av1_read_bit (br);
  if (color_config->color_description_present_flag) {
    color_config->color_primaries = gst_av1_read_bits (br, 8);
    color_config->transfer_characteristics = gst_av1_read_bits (br, 8);
    color_config->matrix_coefficients = gst_av1_read_bits (br, 8);
  } else {
    color_config->color_primaries = GST_AV1_CP_UNSPECIFIED;
    color_config->transfer_characteristics = GST_AV1_TC_UNSPECIFIED;
    color_config->matrix_coefficients = GST_AV1_MC_UNSPECIFIED;
  }

  if (color_config->mono_chrome) {
    color_config->color_range = gst_av1_read_bit (br);
    color_config->subsampling_x = 1;
    color_config->subsampling_y = 1;
    color_config->chroma_sample_position = GST_AV1_CSP_UNKNOWN;
    color_config->separate_uv_delta_q = 0;
    return GST_AV1_PARSER_OK;
  } else if (color_config->color_primaries == GST_AV1_CP_BT_709 &&
      color_config->transfer_characteristics == GST_AV1_TC_SRGB &&
      color_config->matrix_coefficients == GST_AV1_MC_IDENTITY) {
    color_config->color_range = 1;
    color_config->subsampling_x = 0;
    color_config->subsampling_y = 0;
  } else {
    color_config->color_range = gst_av1_read_bit (br);
    if (seq_header->seq_profile == 0) {
      color_config->subsampling_x = 1;
      color_config->subsampling_y = 1;
    } else if (seq_header->seq_profile == 1) {
      color_config->subsampling_x = 0;
      color_config->subsampling_y = 0;
    } else {
      if (color_config->BitDepth == 12) {
        color_config->subsampling_x = gst_av1_read_bit (br);
        if (color_config->subsampling_x)
          color_config->subsampling_y = gst_av1_read_bit (br);
        else
          color_config->subsampling_y = 0;
      } else {
        color_config->subsampling_x = 1;
        color_config->subsampling_y = 0;
      }
    }
    if (color_config->subsampling_x && color_config->subsampling_y)
      color_config->chroma_sample_position = gst_av1_read_bits (br, 2);
  }

  color_config->separate_uv_delta_q = gst_av1_read_bit (br);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_timing_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TimingInfo * timing_info)
{
  GstAV1ParserResult retval;

  GST_AV1_DEBUG_HELPER ();

  timing_info->num_units_in_display_tick = gst_av1_read_bits (br, 32);
  timing_info->time_scale = gst_av1_read_bits (br, 32);
  timing_info->equal_picture_interval = gst_av1_read_bit (br);
  if (timing_info->equal_picture_interval) {
    timing_info->num_ticks_per_picture_minus_1 =
        gst_av1_bitstreamfn_uvlc (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);
  }

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_decoder_model_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1DecoderModelInfo * decoder_model_info)
{
  GST_AV1_DEBUG_HELPER ();

  decoder_model_info->bitrate_scale = gst_av1_read_bits (br, 4);
  decoder_model_info->buffer_size_scale = gst_av1_read_bits (br, 4);
  decoder_model_info->buffer_delay_length_minus_1 = gst_av1_read_bits (br, 5);
  decoder_model_info->num_units_in_decoding_tick = gst_av1_read_bits (br, 32);
  decoder_model_info->buffer_removal_time_length_minus_1 =
      gst_av1_read_bits (br, 5);
  decoder_model_info->frame_presentation_time_length_minus_1 =
      gst_av1_read_bits (br, 5);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_operating_parameters_info (GstAV1Parser * parser,
    GstBitReader * br, GstAV1OperatingPoint * op_point)
{
  GstAV1ParserResult retval;
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;


  op_point->bitrate_minus_1 = gst_av1_bitstreamfn_uvlc (br, &retval);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);
  op_point->buffer_size_minus_1 = gst_av1_bitstreamfn_uvlc (br, &retval);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);
  op_point->cbr_flag = gst_av1_read_bit (br);
  op_point->decoder_buffer_delay =
      gst_av1_read_bits (br,
      seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1);
  op_point->encoder_buffer_delay =
      gst_av1_read_bits (br,
      seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1);
  op_point->low_delay_mode_flag = gst_av1_read_bit (br);

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_sequence_header_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1SequenceHeaderOBU * seq_header)
{
  GstAV1ParserResult retval;
  gint i;

  GST_AV1_DEBUG_HELPER ();

  bzero (seq_header, sizeof (GstAV1SequenceHeaderOBU));
  parser->seq_header = seq_header;

  seq_header->seq_profile = gst_av1_read_bits (br, 3);
  seq_header->still_picture = gst_av1_read_bit (br);
  seq_header->reduced_still_picture_header = gst_av1_read_bit (br);

  if (seq_header->reduced_still_picture_header) {
    seq_header->timing_info_present_flag = 0;
    seq_header->decoder_model_info_present_flag = 0;
    seq_header->initial_display_delay_present_flag = 0;
    seq_header->operating_points_cnt_minus_1 = 0;
    seq_header->operating_points[0].idc = 0;
    seq_header->operating_points[0].seq_level_idx = gst_av1_read_bits (br, 5);
    seq_header->operating_points[0].seq_tier = 0;
    seq_header->operating_points[0].decoder_model_present_for_this_op = 0;
    seq_header->operating_points[0].initial_display_delay_present_for_this_op =
        0;
  } else {
    seq_header->timing_info_present_flag = gst_av1_read_bit (br);

    if (seq_header->timing_info_present_flag) {
      retval =
          gst_av1_parse_timing_info (parser, br, &(seq_header->timing_info));
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

      seq_header->decoder_model_info_present_flag = gst_av1_read_bit (br);
      if (seq_header->decoder_model_info_present_flag) {
        retval =
            gst_av1_parse_decoder_model_info (parser, br,
            &(seq_header->decoder_model_info));
        GST_AV1_EVAL_RETVAL_LOGGED (retval);
      }
    } else {
      seq_header->decoder_model_info_present_flag = 0;
    }

    seq_header->initial_display_delay_present_flag = gst_av1_read_bit (br);
    seq_header->operating_points_cnt_minus_1 = gst_av1_read_bits (br, 5);

    for (i = 0; i <= seq_header->operating_points_cnt_minus_1; i++) {
      seq_header->operating_points[i].idc = gst_av1_read_bits (br, 12);
      seq_header->operating_points[i].seq_level_idx = gst_av1_read_bits (br, 5);
      if (seq_header->operating_points[i].seq_level_idx > 7) {
        seq_header->operating_points[i].seq_tier = gst_av1_read_bit (br);
      } else {
        seq_header->operating_points[i].seq_tier = 0;
      }
      if (seq_header->decoder_model_info_present_flag) {
        seq_header->operating_points[i].decoder_model_present_for_this_op =
            gst_av1_read_bit (br);
        if (seq_header->operating_points[i].decoder_model_present_for_this_op)
          retval =
              gst_av1_parse_operating_parameters_info (parser, br,
              &(seq_header->operating_points[i]));
        GST_AV1_EVAL_RETVAL_LOGGED (retval);
      } else {
        seq_header->operating_points[i].decoder_model_present_for_this_op = 0;
      }

      if (seq_header->initial_display_delay_present_flag) {
        seq_header->operating_points[i].
            initial_display_delay_present_for_this_op = gst_av1_read_bit (br);
        if (seq_header->operating_points[i].
            initial_display_delay_present_for_this_op)
          seq_header->operating_points[i].initial_display_delay_minus_1 =
              gst_av1_read_bits (br, 4);
      }
    }
  }

  //operatingPoint = choose_operating_point( )
  //OperatingPointIdc = operating_point_idc[ operatingPoint ]
  seq_header->frame_width_bits_minus_1 = gst_av1_read_bits (br, 4);
  seq_header->frame_height_bits_minus_1 = gst_av1_read_bits (br, 4);
  seq_header->max_frame_width_minus_1 =
      gst_av1_read_bits (br, seq_header->frame_width_bits_minus_1 + 1);
  seq_header->max_frame_height_minus_1 =
      gst_av1_read_bits (br, seq_header->frame_height_bits_minus_1 + 1);

  if (seq_header->reduced_still_picture_header)
    seq_header->frame_id_numbers_present_flag = 0;
  else
    seq_header->frame_id_numbers_present_flag = gst_av1_read_bit (br);

  if (seq_header->frame_id_numbers_present_flag) {
    seq_header->delta_frame_id_length_minus_2 = gst_av1_read_bits (br, 4);
    seq_header->additional_frame_id_length_minus_1 = gst_av1_read_bits (br, 3);
  }

  seq_header->use_128x128_superblock = gst_av1_read_bit (br);
  seq_header->enable_filter_intra = gst_av1_read_bit (br);
  seq_header->enable_intra_edge_filter = gst_av1_read_bit (br);

  if (seq_header->reduced_still_picture_header) {
    seq_header->enable_interintra_compound = 0;
    seq_header->enable_masked_compound = 0;
    seq_header->enable_warped_motion = 0;
    seq_header->enable_dual_filter = 0;
    seq_header->enable_order_hint = 0;
    seq_header->enable_jnt_comp = 0;
    seq_header->enable_ref_frame_mvs = 0;
    seq_header->seq_force_screen_content_tools =
        GST_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
    //OrderHintBits = 0
  } else {
    seq_header->enable_interintra_compound = gst_av1_read_bit (br);
    seq_header->enable_masked_compound = gst_av1_read_bit (br);
    seq_header->enable_warped_motion = gst_av1_read_bit (br);
    seq_header->enable_dual_filter = gst_av1_read_bit (br);
    seq_header->enable_order_hint = gst_av1_read_bit (br);
    if (seq_header->enable_order_hint) {
      seq_header->enable_jnt_comp = gst_av1_read_bit (br);
      seq_header->enable_ref_frame_mvs = gst_av1_read_bit (br);
    } else {
      seq_header->enable_jnt_comp = 0;
      seq_header->enable_ref_frame_mvs = 0;
    }
    seq_header->seq_choose_screen_content_tools = gst_av1_read_bit (br);
    if (seq_header->seq_choose_screen_content_tools)
      seq_header->seq_force_screen_content_tools =
          GST_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    else
      seq_header->seq_force_screen_content_tools = gst_av1_read_bit (br);

    if (seq_header->seq_force_screen_content_tools > 0) {
      seq_header->seq_choose_integer_mv = gst_av1_read_bit (br);
      if (seq_header->seq_choose_integer_mv)
        seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
      else
        seq_header->seq_force_integer_mv = gst_av1_read_bit (br);
    } else {
      seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
    }
    if (seq_header->enable_order_hint) {
      seq_header->order_hint_bits_minus_1 = gst_av1_read_bits (br, 3);
      //OrderHintBits = order_hint_bits_minus_1 + 1
    } else {
      //OrderHintBits = 0;
    }
  }
  seq_header->enable_superres = gst_av1_read_bit (br);
  seq_header->enable_cdef = gst_av1_read_bit (br);
  seq_header->enable_restoration = gst_av1_read_bit (br);

  retval = gst_av1_parse_color_config (parser, br, &(seq_header->color_config));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  seq_header->film_grain_params_present = gst_av1_read_bit (br);

  gst_av1_bit_reader_skip_to_byte (br);

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_temporal_delimiter_obu (GstAV1Parser * parser, GstBitReader * br)
{
  GST_AV1_DEBUG_HELPER ();

  parser->state.SeenFrameHeader = 0;

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_metadata_itut_t35 (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataITUT_T35 * itut_t35)
{
  GST_AV1_DEBUG_HELPER ();

  itut_t35->itu_t_t35_country_code = gst_av1_read_bits (br, 8);
  if (itut_t35->itu_t_t35_country_code) {
    itut_t35->itu_t_t35_country_code_extention_byte = gst_av1_read_bits (br, 8);
  }
  //TODO: Is skipping bytes necessary here?
  //ommiting itu_t_t35_payload_bytes

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_metadata_hdr_cll (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataHdrCll * hdr_cll)
{
  GST_AV1_DEBUG_HELPER ();

  hdr_cll->max_cll = gst_av1_read_bits (br, 16);
  hdr_cll->max_fall = gst_av1_read_bits (br, 16);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_metadata_hdr_mdcv (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataHdrMdcv * hdr_mdcv)
{
  gint i;

  GST_AV1_DEBUG_HELPER ();

  for (i = 0; i < 4; i++) {
    hdr_mdcv->primary_chromaticity_x[i] = gst_av1_read_bits (br, 16);
    hdr_mdcv->primary_chromaticity_y[i] = gst_av1_read_bits (br, 16);
  }

  hdr_mdcv->white_point_chromaticity_x = gst_av1_read_bits (br, 16);
  hdr_mdcv->white_point_chromaticity_y = gst_av1_read_bits (br, 16);

  hdr_mdcv->luminance_max = gst_av1_read_bits (br, 32);
  hdr_mdcv->luminance_min = gst_av1_read_bits (br, 32);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_metadata_scalability (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataScalability * scalability)
{
  gint i, j;

  GST_AV1_DEBUG_HELPER ();

  scalability->scalability_mode_idc = gst_av1_read_bits (br, 8);
  scalability->spatial_layers_cnt_minus_1 = gst_av1_read_bits (br, 2);
  scalability->spatial_layer_dimensions_present_flag = gst_av1_read_bit (br);
  scalability->spatial_layer_description_present_flag = gst_av1_read_bit (br);
  scalability->temporal_group_description_present_flag = gst_av1_read_bit (br);
  scalability->scalability_structure_reserved_3bits = gst_av1_read_bit (br);
  if (scalability->spatial_layer_dimensions_present_flag) {
    for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++) {
      scalability->spatial_layer_max_width[i] = gst_av1_read_bits (br, 16);
      scalability->spatial_layer_max_height[i] = gst_av1_read_bits (br, 16);
    }
  }

  if (scalability->spatial_layer_description_present_flag) {
    for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++)
      scalability->spatial_layer_ref_id[i] = gst_av1_read_bit (br);
  }

  if (scalability->temporal_group_description_present_flag) {
    scalability->temporal_group_size = gst_av1_read_bits (br, 8);
    for (i = 0; i < scalability->temporal_group_size; i++) {
      scalability->temporal_group_temporal_id[i] = gst_av1_read_bits (br, 3);
      scalability->temporal_group_temporal_switching_up_point_flag[i] =
          gst_av1_read_bit (br);
      scalability->temporal_group_spatial_switching_up_point_flag[i] =
          gst_av1_read_bit (br);
      scalability->temporal_group_ref_cnt[i] = gst_av1_read_bits (br, 3);
      for (j = 0; j < scalability->temporal_group_ref_cnt[i]; j++) {
        scalability->temporal_group_ref_pic_diff[i][j] =
            gst_av1_read_bits (br, 8);
      }
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_metadata_timecode (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataTimecode * timecode)
{
  GST_AV1_DEBUG_HELPER ();

  timecode->counting_type = gst_av1_read_bits (br, 5);
  timecode->full_timestamp_flag = gst_av1_read_bit (br);
  timecode->discontinuity_flag = gst_av1_read_bit (br);
  timecode->cnt_dropped_flag = gst_av1_read_bit (br);
  timecode->n_frames = gst_av1_read_bits (br, 9);

  if (timecode->full_timestamp_flag) {
    timecode->seconds_value = gst_av1_read_bits (br, 6);
    timecode->minutes_value = gst_av1_read_bits (br, 6);
    timecode->hours_value = gst_av1_read_bits (br, 5);
  } else {
    timecode->seconds_flag = gst_av1_read_bit (br);
    if (timecode->seconds_flag) {
      timecode->seconds_value = gst_av1_read_bits (br, 6);
      timecode->minutes_flag = gst_av1_read_bit (br);
      if (timecode->minutes_flag) {
        timecode->minutes_value = gst_av1_read_bits (br, 6);
        timecode->hours_flag = gst_av1_read_bit (br);
        if (timecode->hours_flag)
          timecode->hours_value = gst_av1_read_bits (br, 6);
      }
    }
  }

  timecode->time_offset_length = gst_av1_read_bits (br, 5);
  if (timecode->time_offset_length > 0) {
    timecode->time_offset_value =
        gst_av1_read_bits (br, timecode->time_offset_length);
  }

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_metadata_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataOBU * metadata)
{
  GstAV1ParserResult retval;

  GST_AV1_DEBUG_HELPER ();

  bzero (metadata, sizeof (GstAV1MetadataOBU));

  metadata->metadata_type = gst_av1_bitstreamfn_leb128 (br, &retval);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  switch (metadata->metadata_type) {
    case GST_AV1_METADATA_TYPE_ITUT_T35:
      retval =
          gst_av1_parse_metadata_itut_t35 (parser, br, &(metadata->itut_t35));
      break;
    case GST_AV1_METADATA_TYPE_HDR_CLL:
      retval =
          gst_av1_parse_metadata_hdr_cll (parser, br, &(metadata->hdr_cll));
      break;
    case GST_AV1_METADATA_TYPE_HDR_MDCV:
      retval =
          gst_av1_parse_metadata_hdr_mdcv (parser, br, &(metadata->hdr_mdcv));
      break;
    case GST_AV1_METADATA_TYPE_SCALABILITY:
      retval =
          gst_av1_parse_metadata_scalability (parser, br,
          &(metadata->scalability));
      break;
    case GST_AV1_METADATA_TYPE_TIMECODE:
      retval =
          gst_av1_parse_metadata_timecode (parser, br, &(metadata->timecode));
      break;
    default:
      return GST_AV1_PARSER_ERROR;
  }

  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  gst_av1_bit_reader_skip_to_byte (br);

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_superres_params_compute_image_size (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  if (seq_header->enable_superres) {
    frame_header->use_superres = gst_av1_read_bit (br);
  } else {
    frame_header->use_superres = 0;
  }

  if (frame_header->use_superres) {
    frame_header->coded_denom =
        gst_av1_read_bits (br, GST_AV1_SUPERRES_DENOM_BITS);
    frame_header->SuperresDenom =
        frame_header->coded_denom + GST_AV1_SUPERRES_DENOM_MIN;
  } else {
    frame_header->SuperresDenom = GST_AV1_SUPERRES_NUM;
  }
  frame_header->UpscaledWidth = frame_header->FrameWidth;
  frame_header->FrameWidth =
      (frame_header->UpscaledWidth * GST_AV1_SUPERRES_NUM +
      (frame_header->SuperresDenom / 2)) / frame_header->SuperresDenom;

  // compute_image_size:
  frame_header->MiCols = 2 * ((frame_header->FrameWidth + 7) >> 3);
  frame_header->MiRows = 2 * ((frame_header->FrameHeight + 7) >> 3);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_frame_size (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  if (frame_header->frame_size_override_flag) {
    frame_header->frame_width_minus_1 =
        gst_av1_read_bits (br, seq_header->frame_width_bits_minus_1 + 1);
    frame_header->frame_height_minus_1 =
        gst_av1_read_bits (br, seq_header->frame_height_bits_minus_1 + 1);
    frame_header->FrameWidth = frame_header->frame_width_minus_1 + 1;
    frame_header->FrameHeight = frame_header->frame_height_minus_1 + 1;
  } else {
    frame_header->FrameWidth = seq_header->max_frame_width_minus_1 + 1;
    frame_header->FrameHeight = seq_header->max_frame_height_minus_1 + 1;
  }

  retval =
      gst_av1_parse_superres_params_compute_image_size (parser, br,
      frame_header);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_render_size (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GST_AV1_DEBUG_HELPER ();

  frame_header->render_and_frame_size_different = gst_av1_read_bit (br);
  if (frame_header->render_and_frame_size_different) {
    frame_header->render_width_minus_1 = gst_av1_read_bits (br, 16);
    frame_header->render_height_minus_1 = gst_av1_read_bits (br, 16);
    frame_header->RenderWidth = frame_header->render_width_minus_1 + 1;
    frame_header->RenderHeight = frame_header->render_height_minus_1 + 1;
  } else {
    frame_header->RenderWidth = frame_header->UpscaledWidth;
    frame_header->RenderHeight = frame_header->FrameHeight;
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_frame_size_with_refs (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1ReferenceFrameInfo *ref_info;
  gint i;

  ref_info = &(parser->ref_info);

  GST_AV1_DEBUG_HELPER ();

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
    frame_header->found_ref = gst_av1_read_bit (br);
    if (frame_header->found_ref == 1) {
      gint ref_idx = frame_header->ref_frame_idx[i];
      frame_header->UpscaledWidth = ref_info->entry[ref_idx].RefUpscaledWidth;
      frame_header->FrameWidth = frame_header->UpscaledWidth;
      frame_header->FrameHeight = ref_info->entry[ref_idx].RefFrameHeight;
      frame_header->RenderWidth = ref_info->entry[ref_idx].RefRenderWidth;
      frame_header->RenderHeight = ref_info->entry[ref_idx].RefRenderHeight;
      break;
    }
  }
  if (frame_header->found_ref == 0) {
    retval = gst_av1_parse_frame_size (parser, br, frame_header);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    retval = gst_av1_parse_render_size (parser, br, frame_header);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

  } else {
    retval =
        gst_av1_parse_superres_params_compute_image_size (parser, br,
        frame_header);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);
  }

  return GST_AV1_PARSER_OK;

}


static GstAV1ParserResult
gst_av1_parse_quantization_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1QuantizationParams * quant_params)
{
  GstAV1ParserResult retval;
  GstAV1ColorConfig *color_config;


  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  color_config = &(parser->seq_header->color_config);

  quant_params->base_q_idx = gst_av1_read_bits (br, 8);

  quant_params->DeltaQYDc = gst_av1_bitstreamfn_delta_q (br, &retval);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  if (color_config->NumPlanes > 1) {
    if (color_config->separate_uv_delta_q) {
      quant_params->diff_uv_delta = gst_av1_read_bit (br);
    } else {
      quant_params->diff_uv_delta = 0;
    }
    quant_params->DeltaQUDc = gst_av1_bitstreamfn_delta_q (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);
    quant_params->DeltaQUAc = gst_av1_bitstreamfn_delta_q (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    if (quant_params->diff_uv_delta) {
      quant_params->DeltaQVDc = gst_av1_bitstreamfn_delta_q (br, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

      quant_params->DeltaQVAc = gst_av1_bitstreamfn_delta_q (br, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

    } else {
      quant_params->DeltaQVDc = quant_params->DeltaQUDc;
      quant_params->DeltaQVAc = quant_params->DeltaQUAc;
    }
  } else {
    quant_params->DeltaQUDc = 0;
    quant_params->DeltaQUAc = 0;
    quant_params->DeltaQVDc = 0;
    quant_params->DeltaQVAc = 0;
  }

  quant_params->using_qmatrix = gst_av1_read_bit (br);

  if (quant_params->using_qmatrix) {
    quant_params->qm_y = gst_av1_read_bits (br, 4);
    quant_params->qm_u = gst_av1_read_bits (br, 4);

    if (!color_config->separate_uv_delta_q) {
      quant_params->qm_v = quant_params->qm_u;
    } else {
      quant_params->qm_v = gst_av1_read_bits (br, 4);
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_segmentation_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1SegmenationParams * seg_params)
{
  gint i, j;
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  const guint8 Segmentation_Feature_Bits[GST_AV1_SEG_LVL_MAX] =
      { 8, 6, 6, 6, 6, 3, 0, 0 };
  const guint8 Segmentation_Feature_Signed[GST_AV1_SEG_LVL_MAX] =
      { 1, 1, 1, 1, 1, 0, 0, 0 };
  const guint8 Segmentation_Feature_Max[GST_AV1_SEG_LVL_MAX] =
      { 255, GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER,
    GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER, 7, 0, 0
  };
  gint clipped_value, feature_value;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  seg_params->segmentation_enabled = gst_av1_read_bit (br);



  if (seg_params->segmentation_enabled) {
    if (frame_header->primary_ref_frame == GST_AV1_PRIMARY_REF_NONE) {
      seg_params->segmentation_update_map = 1;
      seg_params->segmentation_temporal_update = 0;
      seg_params->segmentation_update_data = 1;
    } else {
      seg_params->segmentation_update_map = gst_av1_read_bit (br);
      if (seg_params->segmentation_update_map)
        seg_params->segmentation_temporal_update = gst_av1_read_bit (br);
      seg_params->segmentation_update_data = gst_av1_read_bit (br);
    }

    if (seg_params->segmentation_update_data) {
      for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
        for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
          seg_params->FeatureEnabled[i][j] = gst_av1_read_bit (br);
          clipped_value = 0;
          feature_value = 0;
          if (seg_params->FeatureEnabled[i][j]) {
            gint bitsToRead = Segmentation_Feature_Bits[j];
            gint limit = Segmentation_Feature_Max[j];
            if (Segmentation_Feature_Signed[j]) {
              feature_value = gst_av1_bitstreamfn_su (br, bitsToRead, &retval);
              GST_AV1_EVAL_RETVAL_LOGGED (retval);
              clipped_value =
                  gst_av1_helper_Clip3 (limit * (-1), limit, feature_value);
            } else {
              feature_value = gst_av1_read_bits (br, bitsToRead);
              clipped_value = gst_av1_helper_Clip3 (0, limit, feature_value);
            }
          }
          seg_params->FeatureData[i][j] = clipped_value;
        }
      }
    }
  } else {
    for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
      for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
        seg_params->FeatureEnabled[i][j] = 0;
        seg_params->FeatureData[i][j] = 0;
      }
    }
  }

  seg_params->SegIdPreSkip = 0;
  seg_params->LastActiveSegId = 0;
  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      if (seg_params->FeatureEnabled[i][j]) {
        seg_params->LastActiveSegId = i;
        if (j >= GST_AV1_SEG_LVL_REF_FRAME) {
          seg_params->SegIdPreSkip = 1;
        }
      }
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_tile_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TileInfo * tile_info)
{
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;

  gint i;
  gint startSb;
  gint sbCols;
  gint sbRows;
  gint sbShift;
  gint sbSize;
  gint maxTileWidthSb, maxTileHeightSb;
  gint maxTileAreaSb;
  gint minLog2TileCols;
  gint maxLog2TileCols;
  gint maxLog2TileRows;
  gint minLog2TileRows;
  gint minLog2Tiles;
  gint increment_tile_cols_log2;
  gint increment_tile_rows_log2;
  gint width_in_sbs_minus_1;
  gint tileWidthSb, tileHeightSb;
  gint height_in_sbs_minus_1;
  gint maxWidth, maxHeight;
  gint tile_size_bytes_minus_1;
  gint sizeSb;
  gint widestTileSb;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;


  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  sbCols =
      seq_header->use_128x128_superblock ? ((frame_header->MiCols +
          31) >> 5) : ((frame_header->MiCols + 15) >> 4);
  sbRows =
      seq_header->use_128x128_superblock ? ((frame_header->MiRows +
          31) >> 5) : ((frame_header->MiRows + 15) >> 4);
  sbShift = seq_header->use_128x128_superblock ? 5 : 4;
  sbSize = sbShift + 2;
  maxTileWidthSb = GST_AV1_MAX_TILE_WIDTH >> sbSize;
  maxTileAreaSb = GST_AV1_MAX_TILE_AREA >> (2 * sbSize);
  minLog2TileCols = gst_av1_helper_TileLog2 (maxTileWidthSb, sbCols);
  maxLog2TileCols = gst_av1_helper_TileLog2 (1, gst_av1_helper_Min (sbCols,
          GST_AV1_MAX_TILE_COLS));
  maxLog2TileRows = gst_av1_helper_TileLog2 (1, gst_av1_helper_Min (sbRows,
          GST_AV1_MAX_TILE_ROWS));
  minLog2Tiles = gst_av1_helper_Max (minLog2TileCols,
      gst_av1_helper_TileLog2 (maxTileAreaSb, sbRows * sbCols));

  tile_info->uniform_tile_spacing_flag = gst_av1_read_bit (br);
  if (tile_info->uniform_tile_spacing_flag) {
    tile_info->TileColsLog2 = minLog2TileCols;
    while (tile_info->TileColsLog2 < maxLog2TileCols) {
      increment_tile_cols_log2 = gst_av1_read_bit (br);
      if (increment_tile_cols_log2 == 1)
        tile_info->TileColsLog2++;
      else
        break;
    }
    tileWidthSb =
        (sbCols + (1 << tile_info->TileColsLog2) -
        1) >> tile_info->TileColsLog2;
    i = 0;
    for (startSb = 0; startSb < sbCols; startSb += tileWidthSb) {
      tile_info->MiColStarts[i] = startSb << sbShift;
      i += 1;
    }
    tile_info->MiColStarts[i] = frame_header->MiCols;
    tile_info->TileCols = i;

    minLog2TileRows =
        gst_av1_helper_Max (minLog2Tiles - tile_info->TileColsLog2, 0);
    maxTileHeightSb = sbRows >> minLog2TileRows;
    tile_info->TileRowsLog2 = minLog2TileRows;
    while (tile_info->TileRowsLog2 < maxLog2TileRows) {
      increment_tile_rows_log2 = gst_av1_read_bit (br);
      if (increment_tile_rows_log2 == 1)
        tile_info->TileRowsLog2++;
      else
        break;
    }
    tileHeightSb =
        (sbRows + (1 << tile_info->TileRowsLog2) -
        1) >> tile_info->TileRowsLog2;
    i = 0;
    for (startSb = 0; startSb < sbRows; startSb += tileHeightSb) {
      tile_info->MiRowStarts[i] = startSb << sbShift;
      i += 1;
    }
    tile_info->MiRowStarts[i] = frame_header->MiRows;
    tile_info->TileRows = i;
  } else {
    widestTileSb = 0;
    startSb = 0;
    for (i = 0; startSb < sbCols; i++) {
      tile_info->MiColStarts[i] = startSb << sbShift;
      maxWidth = gst_av1_helper_Min (sbCols - startSb, maxTileWidthSb);
      width_in_sbs_minus_1 = gst_av1_bitstreamfn_ns (br, maxWidth, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
      sizeSb = width_in_sbs_minus_1 + 1;
      widestTileSb = gst_av1_helper_Max (sizeSb, widestTileSb);
      startSb += sizeSb;
    }
    tile_info->MiColStarts[i] = frame_header->MiCols;
    tile_info->TileCols = i;
    tile_info->TileColsLog2 = gst_av1_helper_TileLog2 (1, tile_info->TileCols);

    if (minLog2Tiles > 0)
      maxTileAreaSb = (sbRows * sbCols) >> (minLog2Tiles + 1);
    else
      maxTileAreaSb = sbRows * sbCols;

    maxTileHeightSb = gst_av1_helper_Max (maxTileAreaSb / widestTileSb, 1);


    startSb = 0;
    for (i = 0; startSb < sbRows; i++) {
      tile_info->MiRowStarts[i] = startSb << sbShift;
      maxHeight = gst_av1_helper_Min (sbRows - startSb, maxTileHeightSb);
      height_in_sbs_minus_1 = gst_av1_bitstreamfn_ns (br, maxHeight, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
      sizeSb = height_in_sbs_minus_1 + 1;
      startSb += sizeSb;
    }

    tile_info->MiRowStarts[i] = frame_header->MiRows;
    tile_info->TileRows = i;
    tile_info->TileRowsLog2 = gst_av1_helper_TileLog2 (1, tile_info->TileRows);
  }
  if (tile_info->TileColsLog2 > 0 || tile_info->TileRowsLog2 > 0) {
    tile_info->context_update_tile_id =
        gst_av1_read_bits (br,
        tile_info->TileColsLog2 + tile_info->TileRowsLog2);
    tile_size_bytes_minus_1 = gst_av1_read_bit (br);
    tile_info->TileSizeBytes = tile_size_bytes_minus_1 + 1;
  } else {
    tile_info->context_update_tile_id = 0;
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_loop_filter_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1LoopFilterParams * lf_params)
{
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;


  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  if (frame_header->CodedLossless || frame_header->allow_intrabc) {
    lf_params->loop_filter_level[0] = 0;
    lf_params->loop_filter_level[1] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_INTRA_FRAME] = 1;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST2_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST3_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_BWDREF_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_GOLDEN_FRAME] = -1;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_ALTREF_FRAME] = -1;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_ALTREF2_FRAME] = -1;
    for (i = 0; i < 2; i++) {
      lf_params->loop_filter_mode_deltas[i] = 0;
    }
    return GST_AV1_PARSER_OK;
  }

  lf_params->loop_filter_level[0] = gst_av1_read_bits (br, 6);
  lf_params->loop_filter_level[1] = gst_av1_read_bits (br, 6);
  if (seq_header->color_config.NumPlanes > 1) {
    if (lf_params->loop_filter_level[0] || lf_params->loop_filter_level[1]) {
      lf_params->loop_filter_level[2] = gst_av1_read_bits (br, 6);
      lf_params->loop_filter_level[3] = gst_av1_read_bits (br, 6);
    }
  }
  lf_params->loop_filter_sharpness = gst_av1_read_bits (br, 3);
  lf_params->loop_filter_delta_enabled = gst_av1_read_bit (br);

  if (lf_params->loop_filter_delta_enabled) {
    lf_params->loop_filter_delta_update = gst_av1_read_bit (br);
    if (lf_params->loop_filter_delta_update) {
      for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++) {
        lf_params->update_ref_deltas[i] = gst_av1_read_bit (br);
        if (lf_params->update_ref_deltas[i]) {
          lf_params->loop_filter_ref_deltas[i] =
              gst_av1_bitstreamfn_su (br, 7, &retval);
          GST_AV1_EVAL_RETVAL_LOGGED (retval);
        }
      }
      for (i = 0; i < 2; i++) {
        lf_params->update_mode_deltas[i] = gst_av1_read_bit (br);
        if (lf_params->update_mode_deltas[i]) {
          lf_params->loop_filter_mode_deltas[i] =
              gst_av1_bitstreamfn_su (br, 7, &retval);
          GST_AV1_EVAL_RETVAL_LOGGED (retval);
        }
      }
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_quantizer_index_delta_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1QuantizationParams * quant_params)
{
  GST_AV1_DEBUG_HELPER ();

  quant_params->delta_q_res = 0;
  quant_params->delta_q_present = 0;
  if (quant_params->base_q_idx > 0) {
    quant_params->delta_q_present = gst_av1_read_bit (br);
  }
  if (quant_params->delta_q_present) {
    quant_params->delta_q_res = gst_av1_read_bits (br, 2);
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_loop_filter_delta_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1LoopFilterParams * lf_params)
{
  GstAV1FrameHeaderOBU *frame_header;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  lf_params->delta_lf_present = 0;
  lf_params->delta_lf_res = 0;
  lf_params->delta_lf_multi = 0;

  if (lf_params->delta_lf_present) {
    if (!frame_header->allow_intrabc)
      lf_params->delta_lf_present = gst_av1_read_bit (br);
    if (lf_params->delta_lf_present) {
      lf_params->delta_lf_res = gst_av1_read_bits (br, 2);
      lf_params->delta_lf_multi = gst_av1_read_bit (br);
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_cdef_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1CDEFParams * cdef_params)
{
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  if (frame_header->CodedLossless || frame_header->allow_intrabc
      || !seq_header->enable_cdef) {
    cdef_params->cdef_bits = 0;
    cdef_params->cdef_y_pri_strength[0] = 0;
    cdef_params->cdef_y_sec_strength[0] = 0;
    cdef_params->cdef_uv_pri_strength[0] = 0;
    cdef_params->cdef_uv_sec_strength[0] = 0;
    cdef_params->cdef_damping_minus_3 = 0;
    //CdefDamping = 3
    return GST_AV1_PARSER_OK;
  }
  cdef_params->cdef_damping_minus_3 = gst_av1_read_bits (br, 2);
  //CdefDamping = cdef_damping_minus_3 + 3
  cdef_params->cdef_bits = gst_av1_read_bits (br, 2);
  for (i = 0; i < (1 << cdef_params->cdef_bits); i++) {
    cdef_params->cdef_y_pri_strength[i] = gst_av1_read_bits (br, 4);
    cdef_params->cdef_y_sec_strength[i] = gst_av1_read_bits (br, 2);
    if (cdef_params->cdef_y_sec_strength[i] == 3)
      cdef_params->cdef_y_sec_strength[i] += 1;
    if (seq_header->color_config.NumPlanes > 1) {
      cdef_params->cdef_uv_pri_strength[i] = gst_av1_read_bits (br, 4);
      cdef_params->cdef_uv_sec_strength[i] = gst_av1_read_bits (br, 2);
      if (cdef_params->cdef_uv_sec_strength[i] == 3)
        cdef_params->cdef_uv_sec_strength[i] += 1;
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_loop_restoration_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1LoopRestorationParams * lr_params)
{
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;
  const GstAV1FrameRestorationType Remap_Lr_Type[4] = {
    GST_AV1_FRAME_RESTORE_NONE,
    GST_AV1_FRAME_RESTORE_SWITCHABLE,
    GST_AV1_FRAME_RESTORE_WIENER,
    GST_AV1_FRAME_RESTORE_SGRPROJ
  };

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  if (frame_header->AllLossless || frame_header->allow_intrabc
      || !seq_header->enable_restoration) {
    lr_params->FrameRestorationType[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->FrameRestorationType[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->FrameRestorationType[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->UsesLr = 0;
    return GST_AV1_PARSER_OK;
  }

  lr_params->UsesLr = 0;
  lr_params->usesChromaLr = 0;
  for (i = 0; i < seq_header->color_config.NumPlanes; i++) {
    lr_params->lr_type = gst_av1_read_bits (br, 2);
    lr_params->FrameRestorationType[i] = Remap_Lr_Type[lr_params->lr_type];
    if (lr_params->FrameRestorationType[i] != GST_AV1_FRAME_RESTORE_NONE) {
      lr_params->UsesLr = 1;
      if (i > 0) {
        lr_params->usesChromaLr = 1;
      }
    }
  }

  if (lr_params->UsesLr) {
    if (seq_header->use_128x128_superblock) {
      lr_params->lr_unit_shift = gst_av1_read_bit (br);
      lr_params->lr_unit_shift++;
    } else {
      lr_params->lr_unit_shift = gst_av1_read_bit (br);
      if (lr_params->lr_unit_shift) {
        lr_params->lr_unit_extra_shift = gst_av1_read_bit (br);
        lr_params->lr_unit_shift += lr_params->lr_unit_extra_shift;
      }
    }

    lr_params->LoopRestorationSize[0] =
        GST_AV1_RESTORATION_TILESIZE_MAX >> (2 - lr_params->lr_unit_shift);
    if (seq_header->color_config.subsampling_x
        && seq_header->color_config.subsampling_y && lr_params->usesChromaLr) {
      lr_params->lr_uv_shift = gst_av1_read_bit (br);
    } else {
      lr_params->lr_uv_shift = 0;
    }

    lr_params->LoopRestorationSize[1] =
        lr_params->LoopRestorationSize[0] >> lr_params->lr_uv_shift;
    lr_params->LoopRestorationSize[2] =
        lr_params->LoopRestorationSize[0] >> lr_params->lr_uv_shift;
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_tx_mode (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GST_AV1_DEBUG_HELPER ();

  if (frame_header->CodedLossless == 1) {
    frame_header->TxMode = GST_AV1_TX_MODE_ONLY_4x4;
  } else {
    frame_header->tx_mode_select = gst_av1_read_bit (br);
    if (frame_header->tx_mode_select) {
      frame_header->TxMode = GST_AV1_TX_MODE_SELECT;
    } else {
      frame_header->TxMode = GST_AV1_TX_MODE_LARGEST;
    }
  }
  return GST_AV1_PARSER_OK;
}

static gint
gst_av1_get_relative_dist (GstAV1SequenceHeaderOBU * seq_header, gint a, gint b)
{
  gint m, diff;

  if (!seq_header->enable_order_hint)
    return 0;

  diff = a - b;
  m = 1 << seq_header->order_hint_bits_minus_1;
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

static GstAV1ParserResult
gst_av1_parse_skip_mode_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;
  gint skipModeAllowed;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  ref_info = &(parser->ref_info);

  skipModeAllowed = 0;

  if (frame_header->FrameIsIntra || !frame_header->reference_select
      || !seq_header->enable_order_hint) {
    frame_header->skipModeAllowed = 0;
  } else {
    gint forwardIdx = -1;
    gint forwardHint = 0;
    gint backwardIdx = -1;
    gint backwardHint = 0;
    gint refHint = 0;

    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      refHint = ref_info->entry[frame_header->ref_frame_idx[i]].RefOrderHint;
      if (gst_av1_get_relative_dist (parser->seq_header, refHint,
              frame_header->order_hint) < 0) {
        if (forwardIdx < 0
            || gst_av1_get_relative_dist (parser->seq_header, refHint,
                forwardHint) > 0) {
          forwardIdx = i;
          forwardHint = refHint;
        }
      } else if (gst_av1_get_relative_dist (parser->seq_header, refHint,
              frame_header->order_hint) > 0) {
        if (backwardIdx < 0
            || gst_av1_get_relative_dist (parser->seq_header, refHint,
                backwardHint) < 0) {
          backwardIdx = i;
          backwardHint = refHint;
        }
      }
    }

    if (forwardIdx < 0) {
      skipModeAllowed = 0;
    } else if (backwardIdx >= 0) {
      skipModeAllowed = 1;
      frame_header->SkipModeFrame[0] =
          GST_AV1_REF_LAST_FRAME + gst_av1_helper_Min (forwardIdx, backwardIdx);
      frame_header->SkipModeFrame[1] =
          GST_AV1_REF_LAST_FRAME + gst_av1_helper_Max (forwardIdx, backwardIdx);
    } else {
      gint secondForwardIdx = -1;
      gint secondForwardHint = 0;
      for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
        refHint = ref_info->entry[frame_header->ref_frame_idx[i]].RefOrderHint;
        if (gst_av1_get_relative_dist (parser->seq_header, refHint,
                forwardHint) < 0) {
          if (secondForwardIdx < 0
              || gst_av1_get_relative_dist (parser->seq_header, refHint,
                  secondForwardHint) > 0) {
            secondForwardIdx = i;
            secondForwardHint = refHint;
          }
        }
      }

      if (secondForwardIdx < 0) {
        skipModeAllowed = 0;
      } else {
        skipModeAllowed = 1;
        frame_header->SkipModeFrame[0] =
            GST_AV1_REF_LAST_FRAME + gst_av1_helper_Min (forwardIdx,
            secondForwardIdx);
        frame_header->SkipModeFrame[1] =
            GST_AV1_REF_LAST_FRAME + gst_av1_helper_Max (forwardIdx,
            secondForwardIdx);
      }
    }
  }

  if (skipModeAllowed) {
    frame_header->skip_mode_present = gst_av1_read_bit (br);
  } else {
    frame_header->skip_mode_present = 0;
  }

  return GST_AV1_PARSER_OK;
}


static gint
gst_av1_decode_subexp (GstBitReader * br, gint numSyms,
    GstAV1ParserResult * retval)
{
  gint i = 0;
  gint mk = 0;
  gint k = 3;
  gint subexp_final_bits = 0;
  gint subexp_more_bits = 0;
  gint subexp_bits = 0;

  GST_AV1_DEBUG_HELPER ();

//TODO: can while(1) be dangerous??
  while (1) {
    gint b2 = i ? k + i - 1 : k;
    gint a = 1 << b2;
    if (numSyms <= mk + 3 * a) {
      subexp_final_bits = gst_av1_bitstreamfn_ns (br, numSyms - mk, retval);
      if (*retval != GST_AV1_PARSER_OK)
        return 0;
      return subexp_final_bits + mk;
    } else {
      subexp_more_bits = gst_av1_read_bits_checked (br, 1, retval);
      if (*retval != GST_AV1_PARSER_OK)
        return 0;
      if (subexp_more_bits) {
        i++;
        mk += a;
      } else {
        subexp_bits = gst_av1_read_bits_checked (br, b2, retval);
        if (*retval != GST_AV1_PARSER_OK)
          return 0;
        return subexp_bits + mk;
      }
    }
  }
}


static gint
gst_av1_decode_unsigned_subexp_with_ref (GstBitReader * br, gint mx, gint r,
    GstAV1ParserResult * retval)
{
  gint v;
  GST_AV1_DEBUG_HELPER ();

  v = gst_av1_decode_subexp (br, mx, retval);
  if ((r << 1) <= mx) {
    return gst_av1_helper_InverseRecenter (r, v);
  } else {
    return mx - 1 - gst_av1_helper_InverseRecenter (mx - 1 - r, v);
  }
}

static gint
gst_av1_decode_signed_subexp_with_ref (GstBitReader * br, gint low, gint high,
    gint r, GstAV1ParserResult * retval)
{

  GST_AV1_DEBUG_HELPER ();

  return gst_av1_decode_unsigned_subexp_with_ref (br, high - low, r - low,
      retval) + low;
}


//Hack-Warning: PrevGMParams[] are not implemented yet - parser should read right amount of bits, but value will be wrong.
static GstAV1ParserResult
gst_av1_parse_global_param (GstAV1Parser * parser, GstBitReader * br,
    GstAV1GlobalMotionParams * gm_params, GstAV1WarpModelType type, gint ref,
    gint idx)
{
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  gint precDiff, wm_round, mx, r;
  gint absBits = GST_AV1_GM_ABS_ALPHA_BITS;
  gint precBits = GST_AV1_GM_ALPHA_PREC_BITS;
  //gint sub;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  if (idx < 2) {
    if (type == GST_AV1_WARP_MODEL_TRANSLATION) {
      absBits =
          GST_AV1_GM_ABS_TRANS_ONLY_BITS -
          (frame_header->allow_high_precision_mv ? 0 : 1);
      precBits =
          GST_AV1_GM_TRANS_ONLY_PREC_BITS -
          (frame_header->allow_high_precision_mv ? 0 : 1);
    } else {
      absBits = GST_AV1_GM_ABS_TRANS_BITS;
      precBits = GST_AV1_GM_TRANS_PREC_BITS;
    }
  }

  precDiff = GST_AV1_WARPEDMODEL_PREC_BITS - precBits;
  wm_round = (idx % 3) == 2 ? (1 << GST_AV1_WARPEDMODEL_PREC_BITS) : 0;
  //sub = (idx % 3) == 2 ? (1 << precBits) : 0;
  mx = (1 << absBits);
  //int r = (PrevGmParams[ref][idx] >> precDiff) - sub; //TODO: PrevGMParams is missing
  r = 0;                        //Hack-Warning PrevGmParams are not supported yet - bits for reading are defined with mx parameter
  gm_params->gm_params[ref][idx] =
      (gst_av1_decode_signed_subexp_with_ref (br, -mx, mx + 1, r,
          &retval) << precDiff) + wm_round;
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_global_motion_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1GlobalMotionParams * gm_params)
{
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1WarpModelType type;
  gint i, ref;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  for (ref = GST_AV1_REF_LAST_FRAME; ref <= GST_AV1_REF_ALTREF_FRAME; ref++) {
    gm_params->GmType[ref] = GST_AV1_WARP_MODEL_IDENTITY;
    for (i = 0; i < 6; i++) {
      gm_params->gm_params[ref][i] =
          ((i % 3 == 2) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0);
    }
  }

  if (frame_header->FrameIsIntra)
    return GST_AV1_PARSER_OK;

  for (ref = GST_AV1_REF_LAST_FRAME; ref <= GST_AV1_REF_ALTREF_FRAME; ref++) {
    gm_params->is_global[ref] = gst_av1_read_bit (br);
    if (gm_params->is_global[ref]) {
      gm_params->is_rot_zoom[ref] = gst_av1_read_bit (br);
      if (gm_params->is_rot_zoom[ref]) {
        type = GST_AV1_WARP_MODEL_ROTZOOM;
      } else {
        gm_params->is_translation[ref] = gst_av1_read_bit (br);
        type =
            gm_params->is_translation[ref] ? GST_AV1_WARP_MODEL_TRANSLATION :
            GST_AV1_WARP_MODEL_AFFINE;
      }
    } else {
      type = GST_AV1_WARP_MODEL_IDENTITY;
    }
    gm_params->GmType[ref] = type;

    if (type >= GST_AV1_WARP_MODEL_ROTZOOM) {
      gst_av1_parse_global_param (parser, br, gm_params, type, ref, 2);
      gst_av1_parse_global_param (parser, br, gm_params, type, ref, 3);
      if (type == GST_AV1_WARP_MODEL_AFFINE) {
        gst_av1_parse_global_param (parser, br, gm_params, type, ref, 4);
        gst_av1_parse_global_param (parser, br, gm_params, type, ref, 5);
      } else {
        gm_params->gm_params[ref][4] = gm_params->gm_params[ref][3] * (-1);
        gm_params->gm_params[ref][5] = gm_params->gm_params[ref][2];
      }
    }
    if (type >= GST_AV1_WARP_MODEL_TRANSLATION) {
      gst_av1_parse_global_param (parser, br, gm_params, type, ref, 0);
      gst_av1_parse_global_param (parser, br, gm_params, type, ref, 1);
    }
  }

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_film_grain_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FilmGrainParams * fg_params)
{
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;
  gint numPosChroma, numPosLuma;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  if (!seq_header->film_grain_params_present || (!frame_header->show_frame
          && !frame_header->showable_frame)) {
    //reset_grain_params() //TODO: implement reset_grain_params
    return GST_AV1_PARSER_OK;
  }

  fg_params->apply_grain = gst_av1_read_bit (br);

  if (!fg_params->apply_grain) {
    //reset_grain_params() //TODO: impl.
    return GST_AV1_PARSER_OK;
  }

  fg_params->grain_seed = gst_av1_read_bits (br, 16);

  if (frame_header->frame_type == GST_AV1_INTER_FRAME) {
    fg_params->update_grain = gst_av1_read_bit (br);
  } else {
    fg_params->update_grain = 1;
  }

  if (!fg_params->update_grain) {
    fg_params->film_grain_params_ref_idx = gst_av1_read_bits (br, 3);

    // TODO- look at the following:
    /*tempGrainSeed = grain_seed
       load_grain_params( film_grain_params_ref_idx )
       grain_seed = tempGrainSeed
       return */
    // END TODO
  }

  fg_params->num_y_points = gst_av1_read_bits (br, 4);

  for (i = 0; i < fg_params->num_y_points; i++) {
    fg_params->point_y_value[i] = gst_av1_read_bits (br, 8);
    fg_params->point_y_scaling[i] = gst_av1_read_bits (br, 8);
  }

  if (seq_header->color_config.mono_chrome) {
    fg_params->chroma_scaling_from_luma = 0;
  } else {
    fg_params->chroma_scaling_from_luma = gst_av1_read_bit (br);
  }

  if (seq_header->color_config.mono_chrome
      || fg_params->chroma_scaling_from_luma
      || (seq_header->color_config.subsampling_x == 1
          && seq_header->color_config.subsampling_y == 1
          && fg_params->num_y_points == 0)) {
    fg_params->num_cb_points = 0;
    fg_params->num_cr_points = 0;
  } else {
    fg_params->num_cb_points = gst_av1_read_bits (br, 4);
    for (i = 0; i < fg_params->num_cb_points; i++) {
      fg_params->point_cb_value[i] = gst_av1_read_bits (br, 8);
      fg_params->point_cb_scaling[i] = gst_av1_read_bits (br, 8);
    }
    fg_params->num_cr_points = gst_av1_read_bits (br, 4);
    for (i = 0; i < fg_params->num_cr_points; i++) {
      fg_params->point_cr_value[i] = gst_av1_read_bits (br, 8);
      fg_params->point_cr_scaling[i] = gst_av1_read_bits (br, 8);
    }
  }

  fg_params->grain_scaling_minus_8 = gst_av1_read_bits (br, 2);
  fg_params->ar_coeff_lag = gst_av1_read_bits (br, 2);
  numPosLuma = 2 * fg_params->ar_coeff_lag * (fg_params->ar_coeff_lag + 1);
  if (fg_params->num_y_points) {
    numPosChroma = numPosLuma + 1;
    for (i = 0; i < numPosLuma; i++)
      fg_params->ar_coeffs_y_plus_128[i] = gst_av1_read_bits (br, 8);
  } else {
    numPosChroma = numPosLuma;
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cb_points) {
    for (i = 0; i < numPosChroma; i++)
      fg_params->ar_coeffs_cb_plus_128[i] = gst_av1_read_bits (br, 8);
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cr_points) {
    for (i = 0; i < numPosChroma; i++)
      fg_params->ar_coeffs_cr_plus_128[i] = gst_av1_read_bits (br, 8);
  }


  fg_params->ar_coeff_shift_minus_6 = gst_av1_read_bits (br, 2);
  fg_params->grain_scale_shift = gst_av1_read_bits (br, 2);

  if (fg_params->num_cb_points) {
    fg_params->cb_mult = gst_av1_read_bits (br, 8);
    fg_params->cb_luma_mult = gst_av1_read_bits (br, 8);
    fg_params->cb_offset = gst_av1_read_bits (br, 9);
  }

  if (fg_params->num_cr_points) {
    fg_params->cr_mult = gst_av1_read_bits (br, 8);
    fg_params->cr_luma_mult = gst_av1_read_bits (br, 8);
    fg_params->cr_offset = gst_av1_read_bits (br, 9);
  }

  fg_params->overlap_flag = gst_av1_read_bit (br);
  fg_params->clip_to_restricted_range = gst_av1_read_bit (br);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_mark_ref_frames (GstAV1Parser * parser, GstBitReader * br, gint idLen)
{
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i, diffLen;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  ref_info = &(parser->ref_info);


  diffLen = seq_header->delta_frame_id_length_minus_2 + 2;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if (frame_header->current_frame_id > (1 << diffLen)) {
      if (ref_info->entry[i].RefFrameId > frame_header->current_frame_id ||
          ref_info->entry[i].RefFrameId <
          (frame_header->current_frame_id - (1 << diffLen)))
        ref_info->entry[i].RefValid = 0;
    } else {
      if (ref_info->entry[i].RefFrameId > frame_header->current_frame_id
          && ref_info->entry[i].RefFrameId <
          ((1 << idLen) + frame_header->current_frame_id - (1 << diffLen)))
        ref_info->entry[i].RefValid = 0;
    }
  }

  return GST_AV1_PARSER_OK;
}



static gint
gst_av1_get_qindex_ignoreDeltaQ (GstAV1QuantizationParams * quantization_params,
    gint segmentId)
{
  // This only handles the ignorDeltaQ case
  return quantization_params->base_q_idx;
}


static GstAV1ParserResult
gst_av1_parse_uncompressed_frame_header (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i, opNum, segmentId, allFrames;
  gint idLen = 0;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  ref_info = &(parser->ref_info);

  bzero (frame_header, sizeof (GstAV1FrameHeaderOBU));
  parser->frame_header = frame_header;

  if (seq_header->frame_id_numbers_present_flag)
    idLen =
        seq_header->additional_frame_id_length_minus_1 +
        seq_header->delta_frame_id_length_minus_2 + 3;

  allFrames = (1 << GST_AV1_NUM_REF_FRAMES) - 1;

  if (seq_header->reduced_still_picture_header) {
    frame_header->show_existing_frame = 0;
    frame_header->frame_type = GST_AV1_KEY_FRAME;
    frame_header->FrameIsIntra = 1;
    frame_header->show_frame = 1;
    frame_header->showable_frame = 0;
  } else {
    frame_header->show_existing_frame = gst_av1_read_bit (br);
    if (frame_header->show_existing_frame) {
      frame_header->frame_to_show_map_idx = gst_av1_read_bits (br, 3);
      if (seq_header->decoder_model_info_present_flag
          && !seq_header->timing_info.equal_picture_interval)
        frame_header->frame_presentation_time =
            gst_av1_read_bits (br,
            seq_header->decoder_model_info.
            frame_presentation_time_length_minus_1 + 1);
      frame_header->refresh_frame_flags = 0;
      if (seq_header->frame_id_numbers_present_flag) {
        frame_header->display_frame_id = gst_av1_read_bits (br, idLen);
      }

      frame_header->frame_type =
          ref_info->entry[frame_header->frame_to_show_map_idx].RefFrameType;

      if (frame_header->frame_type == GST_AV1_KEY_FRAME) {
        frame_header->refresh_frame_flags = allFrames;
      }
      if (seq_header->film_grain_params_present) {
        //load_grain_params( frame_to_show_map_idx ) //TODO: load_grain_params
      }
      return GST_AV1_PARSER_OK;
    }

    frame_header->frame_type = gst_av1_read_bits (br, 2);
    frame_header->FrameIsIntra =
        (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME
        || frame_header->frame_type == GST_AV1_KEY_FRAME);
    frame_header->show_frame = gst_av1_read_bit (br);
    if (frame_header->show_frame && seq_header->decoder_model_info_present_flag
        && !seq_header->timing_info.equal_picture_interval)
      frame_header->frame_presentation_time =
          gst_av1_read_bits (br,
          seq_header->decoder_model_info.
          frame_presentation_time_length_minus_1 + 1);

    if (frame_header->show_frame)
      frame_header->showable_frame = 0;
    else
      frame_header->showable_frame = gst_av1_read_bit (br);

    if (frame_header->frame_type == GST_AV1_SWITCH_FRAME
        || (frame_header->frame_type == GST_AV1_KEY_FRAME
            && frame_header->show_frame))
      frame_header->error_resilient_mode = 1;
    else
      frame_header->error_resilient_mode = gst_av1_read_bit (br);
  }

  if (frame_header->frame_type == GST_AV1_KEY_FRAME && frame_header->show_frame) {
    for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
      ref_info->entry[i].RefValid = 0;
      ref_info->entry[i].RefOrderHint = 0;
    }
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      frame_header->OrderHints[GST_AV1_REF_LAST_FRAME + i] = 0;
    }
  }

  frame_header->disable_cdf_update = gst_av1_read_bit (br);

  if (seq_header->seq_force_screen_content_tools ==
      GST_AV1_SELECT_SCREEN_CONTENT_TOOLS) {
    frame_header->allow_screen_content_tools = gst_av1_read_bit (br);
  } else {
    frame_header->allow_screen_content_tools =
        seq_header->seq_force_screen_content_tools;
  }

  if (frame_header->allow_screen_content_tools) {
    if (seq_header->seq_force_integer_mv == GST_AV1_SELECT_INTEGER_MV) {
      frame_header->force_integer_mv = gst_av1_read_bit (br);
    } else {
      frame_header->force_integer_mv = seq_header->seq_force_integer_mv;
    }
  } else {
    frame_header->force_integer_mv = 0;
  }

  if (frame_header->FrameIsIntra) {
    frame_header->force_integer_mv = 1;
  }

  if (seq_header->frame_id_numbers_present_flag) {
    //PrevFrameID = current_frame_id
    frame_header->current_frame_id = gst_av1_read_bits (br, idLen);
    gst_av1_mark_ref_frames (parser, br, idLen);
  } else {
    frame_header->current_frame_id = 0;
  }
  if (frame_header->frame_type == GST_AV1_SWITCH_FRAME) {
    frame_header->frame_size_override_flag = 1;
  } else if (seq_header->reduced_still_picture_header) {
    frame_header->frame_size_override_flag = 0;
  } else {
    frame_header->frame_size_override_flag = gst_av1_read_bit (br);
  }

  frame_header->order_hint =
      gst_av1_read_bits (br, seq_header->order_hint_bits_minus_1 + 1);
  //OrderHint = order_hint

  if (frame_header->FrameIsIntra || frame_header->error_resilient_mode) {
    frame_header->primary_ref_frame = GST_AV1_PRIMARY_REF_NONE;
  } else {
    frame_header->primary_ref_frame = gst_av1_read_bits (br, 3);
  }

  if (seq_header->decoder_model_info_present_flag) {
    frame_header->buffer_removal_time_present_flag = gst_av1_read_bit (br);
    if (frame_header->buffer_removal_time_present_flag) {
      for (opNum = 0; opNum <= seq_header->operating_points_cnt_minus_1;
          opNum++) {
        if (seq_header->operating_points[opNum].
            decoder_model_present_for_this_op) {
          gint opPtIdc = seq_header->operating_points[opNum].idc;
          gint inTemporalLayer = (opPtIdc >> parser->state.temporal_id) & 1;
          gint inSpatialLayer = (opPtIdc >> (parser->state.spatial_id + 8)) & 1;
          if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer))
            frame_header->buffer_removal_time[opNum] =
                gst_av1_read_bits (br,
                seq_header->decoder_model_info.
                buffer_removal_time_length_minus_1 + 1);
        }
      }
    }
  }

  frame_header->allow_high_precision_mv = 0;
  frame_header->use_ref_frame_mvs = 0;
  frame_header->allow_intrabc = 0;
  if (frame_header->frame_type == GST_AV1_SWITCH_FRAME ||
      (frame_header->frame_type == GST_AV1_KEY_FRAME
          && frame_header->show_frame)) {
    frame_header->refresh_frame_flags = allFrames;
  } else {
    frame_header->refresh_frame_flags = gst_av1_read_bits (br, 8);
  }

  if (!frame_header->FrameIsIntra
      || frame_header->refresh_frame_flags != allFrames) {
    if (frame_header->error_resilient_mode && seq_header->enable_order_hint) {
      for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
        frame_header->ref_order_hint[i] =
            gst_av1_read_bits (br, seq_header->order_hint_bits_minus_1 + 1);
        if (frame_header->ref_order_hint[i] != ref_info->entry[i].RefOrderHint)
          ref_info->entry[i].RefValid = 0;
      }
    }
  }
  //here comes a concious simplification of the Spec 20180614 handeling KEY_FRAME and INTRA_ONLY_FRAME the same warranty
  //TODO: Recheck this section with recent Spec - before merging
  if (frame_header->frame_type == GST_AV1_KEY_FRAME
      || frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME) {
    gst_av1_parse_frame_size (parser, br, frame_header);
    gst_av1_parse_render_size (parser, br, frame_header);
    if (frame_header->allow_screen_content_tools
        && frame_header->UpscaledWidth == frame_header->FrameWidth) {
      frame_header->allow_intrabc = gst_av1_read_bit (br);
    }
  } else {
    if (!seq_header->enable_order_hint) {
      frame_header->frame_refs_short_signaling = 0;
    } else {
      frame_header->frame_refs_short_signaling = gst_av1_read_bit (br);
      if (frame_header->frame_refs_short_signaling) {
        frame_header->last_frame_idx = gst_av1_read_bits (br, 3);
        frame_header->gold_frame_idx = gst_av1_read_bits (br, 3);
        //set_frame_refs()
      }
    }
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      if (!frame_header->frame_refs_short_signaling)
        frame_header->ref_frame_idx[i] = gst_av1_read_bits (br, 3);

      if (seq_header->frame_id_numbers_present_flag) {
        frame_header->delta_frame_id_minus_1 =
            gst_av1_read_bits (br,
            seq_header->delta_frame_id_length_minus_2 + 2);
        frame_header->expectedFrameId[i] =
            ((frame_header->current_frame_id + (1 << idLen) -
                (frame_header->delta_frame_id_minus_1 + 1)) % (1 << idLen));
      }
    }

    if (frame_header->frame_size_override_flag
        && !frame_header->error_resilient_mode) {
      gst_av1_parse_frame_size_with_refs (parser, br, frame_header);
    } else {
      gst_av1_parse_frame_size (parser, br, frame_header);
      gst_av1_parse_render_size (parser, br, frame_header);
    }
    if (frame_header->force_integer_mv) {
      frame_header->allow_high_precision_mv = 0;
    } else {
      frame_header->allow_high_precision_mv = gst_av1_read_bit (br);
    }

    //inline read_interpolation_filter
    frame_header->is_filter_switchable = gst_av1_read_bit (br);
    if (frame_header->is_filter_switchable) {
      frame_header->interpolation_filter =
          GST_AV1_INTERPOLATION_FILTER_SWITCHABLE;
    } else {
      frame_header->interpolation_filter = gst_av1_read_bits (br, 2);
    }

    frame_header->is_motion_mode_switchable = gst_av1_read_bit (br);
    if (frame_header->error_resilient_mode || !seq_header->enable_ref_frame_mvs) {
      frame_header->use_ref_frame_mvs = 0;
    } else {
      frame_header->use_ref_frame_mvs = gst_av1_read_bit (br);
    }
  }

  if (!frame_header->FrameIsIntra) {
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      gint refFrame = GST_AV1_REF_LAST_FRAME + 1;
      gint hint = ref_info->entry[frame_header->ref_frame_idx[i]].RefOrderHint;
      frame_header->OrderHints[refFrame] = hint;
      if (!seq_header->enable_order_hint) {
        frame_header->RefFrameSignBias[refFrame] = 0;
      } else {
        frame_header->RefFrameSignBias[refFrame] = (gst_av1_get_relative_dist (parser->seq_header, hint, frame_header->order_hint) > 0);        //TODO: OrderHint present?
      }
    }
  }
  if (seq_header->reduced_still_picture_header
      || frame_header->disable_cdf_update)
    frame_header->disable_frame_end_update_cdf = 1;
  else
    frame_header->disable_frame_end_update_cdf = gst_av1_read_bit (br);

  //SPECs sets upt CDFs here - for codecparser we are ommitting this section.
  /* SKIP
     if ( primary_ref_frame == PRIMARY_REF_NONE ) {
     init_non_coeff_cdfs( )
     setup_past_independence( )
     } else {
     if ( use_ref_frame_mvs == 1 )
     motion_field_estimation( )
   */
  retval = gst_av1_parse_tile_info (parser, br, &(frame_header->tile_info));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  retval =
      gst_av1_parse_quantization_params (parser, br,
      &(frame_header->quantization_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval =
      gst_av1_parse_segmentation_params (parser, br,
      &(frame_header->segmentation_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval =
      gst_av1_parse_quantizer_index_delta_params (parser, br,
      &(frame_header->quantization_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval =
      gst_av1_parse_loop_filter_delta_params (parser, br,
      &(frame_header->loop_filter_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


/*
  if ( primary_ref_frame == PRIMARY_REF_NONE ) {
    init_coeff_cdfs( )
  } else {
    load_previous_segment_ids( )
  }
*/
  frame_header->CodedLossless = 1;
  for (segmentId = 0; segmentId < GST_AV1_MAX_SEGMENTS; segmentId++) {
    // HACK-Warning: Spec calls get_qindex here - but we are using a shortcut handeling only the ignoreDeltaQ case, becaue CurrentQIndex is not decoded at the moment
    //qindex = get_qindex( 1, segmentId );
    gint qindex =
        gst_av1_get_qindex_ignoreDeltaQ (&(frame_header->quantization_params),
        segmentId);

    frame_header->LosslessArray[segmentId] = (qindex == 0)
        && (frame_header->quantization_params.DeltaQYDc == 0)
        && (frame_header->quantization_params.DeltaQUAc == 0)
        && (frame_header->quantization_params.DeltaQUDc == 0)
        && (frame_header->quantization_params.DeltaQVAc == 0)
        && (frame_header->quantization_params.DeltaQVDc == 0);
    if (!frame_header->LosslessArray[segmentId])
      frame_header->CodedLossless = 0;
    if (frame_header->quantization_params.using_qmatrix) {
      if (frame_header->LosslessArray[segmentId]) {
        frame_header->SegQMLevel[0][segmentId] = 15;
        frame_header->SegQMLevel[1][segmentId] = 15;
        frame_header->SegQMLevel[2][segmentId] = 15;
      } else {
        frame_header->SegQMLevel[0][segmentId] =
            frame_header->quantization_params.qm_y;
        frame_header->SegQMLevel[1][segmentId] =
            frame_header->quantization_params.qm_u;
        frame_header->SegQMLevel[2][segmentId] =
            frame_header->quantization_params.qm_v;
      }
    }
  }
  frame_header->AllLossless = frame_header->CodedLossless
      && (frame_header->FrameWidth == frame_header->UpscaledWidth);

  retval =
      gst_av1_parse_loop_filter_params (parser, br,
      &(frame_header->loop_filter_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval = gst_av1_parse_cdef_params (parser, br, &(frame_header->cdef_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval =
      gst_av1_parse_loop_restoration_params (parser, br,
      &(frame_header->loop_restoration_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval = gst_av1_parse_tx_mode (parser, br, frame_header);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  //inlined frame_reference_mode()
  if (frame_header->FrameIsIntra) {
    frame_header->reference_select = 0;
  } else {
    frame_header->reference_select = gst_av1_read_bit (br);
  }

  retval = gst_av1_parse_skip_mode_params (parser, br, frame_header);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  if (frame_header->FrameIsIntra ||
      frame_header->error_resilient_mode || !seq_header->enable_warped_motion)
    frame_header->allow_warped_motion = 0;
  else
    frame_header->allow_warped_motion = gst_av1_read_bit (br);

  frame_header->reduced_tx_set = gst_av1_read_bit (br);

  retval =
      gst_av1_parse_global_motion_params (parser, br,
      &(frame_header->global_motion_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  retval =
      gst_av1_parse_film_grain_params (parser, br,
      &(frame_header->film_grain_params));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  return GST_AV1_PARSER_OK;

}

static GstAV1ParserResult
gst_av1_reference_frame_update (GstAV1Parser * parser)
{
  gint i;
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1ReferenceFrameInfo *ref_info;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  ref_info = &(parser->ref_info);


  if (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME
      && frame_header->refresh_frame_flags == 0xff)
    return GST_AV1_PARSER_BITSTREAM_ERROR;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if ((frame_header->refresh_frame_flags >> i) & 1) {
      ref_info->entry[i].RefValid = 1;
      ref_info->entry[i].RefFrameId = frame_header->current_frame_id;
      ref_info->entry[i].RefFrameType = frame_header->frame_type;
      ref_info->entry[i].RefUpscaledWidth = frame_header->UpscaledWidth;
      ref_info->entry[i].RefFrameWidth = frame_header->FrameWidth;
      ref_info->entry[i].RefFrameHeight = frame_header->FrameHeight;
      ref_info->entry[i].RefRenderWidth = frame_header->RenderWidth;
      ref_info->entry[i].RefRenderHeight = frame_header->RenderHeight;
    }
  }

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_tile_list_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TileListOBU * tile_list)
{
  gint tile;

  GST_AV1_DEBUG_HELPER ();

  bzero (tile_list, sizeof (GstAV1TileListOBU));

  tile_list->output_frame_width_in_tiles_minus_1 = gst_av1_read_bits (br, 8);
  tile_list->output_frame_height_in_tiles_minus_1 = gst_av1_read_bits (br, 8);
  tile_list->tile_count_minus_1 = gst_av1_read_bits (br, 16);
  for (tile = 0; tile <= tile_list->tile_count_minus_1; tile++) {
    tile_list->entry[tile].anchor_frame_idx = gst_av1_read_bits (br, 8);
    tile_list->entry[tile].anchor_tile_row = gst_av1_read_bits (br, 8);
    tile_list->entry[tile].anchor_tile_col = gst_av1_read_bits (br, 8);
    tile_list->entry[tile].tile_data_size_minus_1 = gst_av1_read_bits (br, 16);
    //skip ofer coded_tile_data
    gst_av1_bit_reader_skip_bytes (br,
        tile_list->entry[tile].tile_data_size_minus_1 + 1);
  }

  return GST_AV1_PARSER_OK;
}


GstAV1ParserResult
gst_av1_parse_tile_group_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1Size sz, GstAV1TileGroupOBU * tile_group)
{
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  gint TileNum, endBitPos, headerBytes, startBitPos;

  GST_AV1_DEBUG_HELPER ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  bzero (tile_group, sizeof (GstAV1TileGroupOBU));

  tile_group->NumTiles =
      frame_header->tile_info.TileCols * frame_header->tile_info.TileRows;
  startBitPos = gst_av1_bit_reader_get_pos (br);
  tile_group->tile_start_and_end_present_flag = 0;

  if (tile_group->NumTiles > 1)
    tile_group->tile_start_and_end_present_flag = gst_av1_read_bit (br);
  if (tile_group->NumTiles == 1 || !tile_group->tile_start_and_end_present_flag) {
    tile_group->tg_start = 0;
    tile_group->tg_end = tile_group->NumTiles - 1;
  } else {
    gint tileBits =
        frame_header->tile_info.TileColsLog2 +
        frame_header->tile_info.TileRowsLog2;
    tile_group->tg_start = gst_av1_read_bits (br, tileBits);
    tile_group->tg_end = gst_av1_read_bits (br, tileBits);
  }

  gst_av1_bit_reader_skip_to_byte (br);

  endBitPos = gst_av1_bit_reader_get_pos (br);
  headerBytes = (endBitPos - startBitPos) / 8;
  sz -= headerBytes;

  for (TileNum = tile_group->tg_start; TileNum <= tile_group->tg_end; TileNum++) {
    tile_group->entry[TileNum].tileRow =
        TileNum / frame_header->tile_info.TileCols;
    tile_group->entry[TileNum].tileCol =
        TileNum % frame_header->tile_info.TileCols;
    //if last tile
    if (TileNum == tile_group->tg_end) {
      tile_group->entry[TileNum].tileSize = sz;
    } else {
      gint tile_size_minus_1 =
          gst_av1_bitstreamfn_le (br, frame_header->tile_info.TileSizeBytes,
          &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
      tile_group->entry[TileNum].tileSize = tile_size_minus_1 + 1;
      sz -=
          tile_group->entry[TileNum].tileSize -
          frame_header->tile_info.TileSizeBytes;
    }

    tile_group->entry[TileNum].MiRowStart =
        frame_header->tile_info.MiRowStarts[tile_group->entry[TileNum].tileRow];
    tile_group->entry[TileNum].MiRowEnd =
        frame_header->tile_info.MiRowStarts[tile_group->entry[TileNum].tileRow +
        1];
    tile_group->entry[TileNum].MiColStart =
        frame_header->tile_info.MiColStarts[tile_group->entry[TileNum].tileCol];
    tile_group->entry[TileNum].MiColEnd =
        frame_header->tile_info.MiColStarts[tile_group->entry[TileNum].tileCol +
        1];
    tile_group->entry[TileNum].CurrentQIndex =
        frame_header->quantization_params.base_q_idx;
    /* Skipped
       init_symbol( tileSize )
       decode_tile( )
       exit_symbol( )
     */
    gst_av1_bit_reader_skip_bytes (br, tile_group->entry[TileNum].tileSize);
  }

  if (tile_group->tg_end == tile_group->NumTiles - 1) {
    /* Skipped
       if ( !disable_frame_end_update_cdf ) {
       frame_end_update_cdf( )
       }
       decode_frame_wrapup( )
     */
    retval = gst_av1_reference_frame_update (parser);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    parser->state.SeenFrameHeader = 0;
  }

  return GST_AV1_PARSER_OK;
}


GstAV1ParserResult
gst_av1_parse_frame_header_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;

  GST_AV1_DEBUG_HELPER ();

  if (parser->state.SeenFrameHeader == 1) {
    //frame_header holds vaild data
    return GST_AV1_PARSER_OK;
  } else {
    retval = gst_av1_parse_uncompressed_frame_header (parser, br, frame_header);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    parser->state.SeenFrameHeader = 1;

    if (frame_header->show_existing_frame) {
      //decode_frame_warpup();
      retval = gst_av1_reference_frame_update (parser);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

      parser->state.SeenFrameHeader = 0;
    } else {
      //TileNum = 0;
      parser->state.SeenFrameHeader = 1;
    }
  }

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_frame_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameOBU * frame)
{
  GstAV1ParserResult retval;
  gint endBitPos, headerBytes, startBitPos;

  GST_AV1_DEBUG_HELPER ();

  startBitPos = gst_av1_bit_reader_get_pos (br);
  retval = gst_av1_parse_frame_header_obu (parser, br, &(frame->frame_header));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  gst_av1_bit_reader_skip_to_byte (br);

  endBitPos = gst_av1_bit_reader_get_pos (br);
  headerBytes = (endBitPos - startBitPos) / 8;
  frame->sz -= headerBytes;
  retval =
      gst_av1_parse_tile_group_obu (parser, br, frame->sz,
      &(frame->tile_group));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  return GST_AV1_PARSER_OK;
}

GstAV1Parser *
gst_av1_parser_new (void)
{
  GstAV1Parser *parser;

  GST_DEBUG ("Create AV1 Parser");

  parser = g_slice_new0 (GstAV1Parser);
  if (!parser)
    return NULL;

  bzero (parser, sizeof (GstAV1Parser));

  return parser;
}

void
gst_av1_parser_free (GstAV1Parser * parser)
{
  if (parser) {
    g_slice_free (GstAV1Parser, parser);
  }
}
