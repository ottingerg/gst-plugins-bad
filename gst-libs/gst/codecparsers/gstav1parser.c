/* gstav1parser.c
 *
 *  Copyright (C) 2018 Georg Ottinger
 *    Author: Georg Ottinger<g.ottinger@gmx.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/**
 * SECTION:gstav1parser
 * @title: GstAV1Parser
 * @short_description: Convenience library for parsing AV1 video bitstream.
 *
 * For more details about the structures, you can refer to the
 * AV1 Bitstream & Decoding Process Specification V1.0.0
 * [specification](https://aomediacodec.github.io/av1-spec/av1-spec.pdf)
 */

 /* TODO: Annex B */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include "gstav1parser.h"



#define GST_AV1_DEBUG() GST_DEBUG("enter")
#define GST_AV1_EVAL_RETVAL_LOGGED(ret) if (ret != GST_AV1_PARSER_OK) { GST_LOG("Parser-Error(%d) at line %d",(gint)ret, __LINE__); return ret; }

/**
 * GstAV1ParserPrivate:
 *
 * @seen_frame_header: is a variable used to mark whether the frame header for the current frame has been received.
 *                   It is initialized to zero.
 *
 */
typedef struct
{
  gboolean seen_frame_header;
  guint8 temporal_id;
  guint8 spatial_id;
  guint64 obu_start_position;
  gsize obu_size;
  GstAV1OBUType obu_type;
} GstAV1ParserPrivate;

#define GST_AV1_PARSER_GET_PRIVATE(parser)  ((GstAV1ParserPrivate *)(parser->priv))


#define gst_av1_read_bit(br)  gst_av1_read_bits(br, 1)

#ifdef __GNUC__
  /*GNU C allows using compound statements as expressions,thus returning calling function on error is possible */
#define gst_av1_read_bits(br, nbits) ({GstAV1ParserResult _br_retval; guint64 _br_bits=gst_av1_read_bits_checked(br,(nbits),&_br_retval); GST_AV1_EVAL_RETVAL_LOGGED(_br_retval); _br_bits;})
#else
  /*on other C compilers the retval is not evaluated */
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

  GST_AV1_DEBUG ();

  for (i = 0; i < 8; i++) {

    leb128_byte = gst_av1_read_bits_checked (br, 8, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    value |= (((gint) leb128_byte & 0x7f) << (i * 7));
    if (!(leb128_byte & 0x80)) {
      break;
    }
  }
  if (value < G_MAXUINT32) {    /*check for bitstream conformance see chapter4.10.5 */
    return (guint32) value;
  } else {
    *retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    return 0;
  }
}

static guint32
gst_av1_bitstreamfn_uvlc (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 leading_zero = 0;
  guint32 readv;
  guint32 value;
  gboolean done;


  GST_AV1_DEBUG ();

  while (1) {
    done = gst_av1_read_bits_checked (br, 1, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    if (done) {
      break;
    }
    leading_zero++;
  }
  if (leading_zero >= 32) {
    value = G_MAXUINT32;
    return value;
  }
  readv = gst_av1_read_bits_checked (br, leading_zero, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  value = readv + (1 << leading_zero) - 1;

  return value;
}


static guint32
gst_av1_bitstreamfn_su (GstBitReader * br, guint8 n,
    GstAV1ParserResult * retval)
{
  guint32 v, sign_mask;

  GST_AV1_DEBUG ();

  v = gst_av1_read_bits_checked (br, n, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  sign_mask = 1 << (n - 1);
  if (v & sign_mask)
    return v - 2 * sign_mask;
  else
    return v;
}

static guint8
gst_av1_bitstreamfn_ns (GstBitReader * br, guint8 n,
    GstAV1ParserResult * retval)
{
  gint w, m, v;
  gint extra_bit;

  GST_AV1_DEBUG ();


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


  GST_AV1_DEBUG ();

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

  GST_AV1_DEBUG ();

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

  return 0;
}

static GstAV1ParserResult
gst_av1_bitstreamfn_trailing_bits (GstBitReader * br, gsize nbBits)
{
  guint8 trailing_one_bit, trailing_zero_bit;

  trailing_one_bit = gst_av1_read_bit (br);
  if (trailing_one_bit != 1) {
    return GST_AV1_PARSER_BITSTREAM_ERROR;
  }

  nbBits--;
  while (nbBits > 0) {
    trailing_zero_bit = gst_av1_read_bit (br);
    if (trailing_zero_bit != 0) {
      return GST_AV1_PARSER_BITSTREAM_ERROR;
    }
    nbBits--;
  }

  return GST_AV1_PARSER_OK;
}

/*************************************
 *                                   *
 * Parser Functions                  *
 *                                   *
 *************************************/

static GstAV1ParserResult
gst_av1_skip_trailing_bits (GstAV1Parser * parser, GstBitReader * br)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);
  guint64 current_position;
  gsize payload_bits;

  if (priv->obu_size > 0
      && priv->obu_type != GST_AV1_OBU_TILE_GROUP
      && priv->obu_type != GST_AV1_OBU_FRAME) {
    current_position = gst_av1_bit_reader_get_pos (br);
    payload_bits = current_position - priv->obu_start_position;
    return gst_av1_bitstreamfn_trailing_bits (br,
        priv->obu_size * 8 - payload_bits);
  }
  return GST_AV1_PARSER_OK;
}


GstAV1ParserResult
gst_av1_parse_obu_header (GstAV1Parser * parser, GstBitReader * br,
    GstAV1OBUHeader * obu_header, gsize annexb_sz)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);
  GstAV1ParserResult retval;
  guint8 obu_forbidden_bit;

  GST_AV1_DEBUG ();

  memset (obu_header, 0, sizeof (GstAV1OBUHeader));

  obu_forbidden_bit = gst_av1_read_bit (br);

  if (obu_forbidden_bit != 0)
    return GST_AV1_PARSER_BITSTREAM_ERROR;

  obu_header->obu_type = gst_av1_read_bits (br, 4);
  obu_header->obu_extention_flag = gst_av1_read_bit (br);
  obu_header->obu_has_size_field = gst_av1_read_bit (br);
  obu_header->obu_reserved_1bit = gst_av1_read_bit (br);

  if (obu_header->obu_extention_flag) {
    obu_header->extention.obu_temporal_id = gst_av1_read_bits (br, 3);
    obu_header->extention.obu_spatial_id = gst_av1_read_bits (br, 2);
    obu_header->extention.obu_extension_header_reserved_3bits =
        gst_av1_read_bits (br, 3);

    priv->temporal_id = obu_header->extention.obu_temporal_id;
    priv->spatial_id = obu_header->extention.obu_spatial_id;
  } else {
    priv->temporal_id = 0;
    priv->spatial_id = 0;
  }

  if (obu_header->obu_has_size_field) {
    obu_header->obu_size = gst_av1_bitstreamfn_leb128 (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);
  } else {
    obu_header->obu_size = annexb_sz;
  }

  priv->obu_size = obu_header->obu_size;
  priv->obu_start_position = gst_av1_bit_reader_get_pos (br);
  priv->obu_type = obu_header->obu_type;

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_color_config (GstAV1Parser * parser, GstBitReader * br,
    GstAV1ColorConfig * color_config)
{
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  color_config->high_bitdepth = gst_av1_read_bit (br);
  if (seq_header->seq_profile == 2 && color_config->high_bitdepth) {
    color_config->twelve_bit = gst_av1_read_bit (br);
    color_config->bit_depth = color_config->twelve_bit ? 12 : 10;
  } else if (seq_header->seq_profile <= 2) {
    color_config->bit_depth = color_config->high_bitdepth ? 10 : 8;
  }

  /* duplicate variables into parser struct */
  parser->bit_depth = color_config->bit_depth;

  if (seq_header->seq_profile == 1)
    color_config->mono_chrome = 0;
  else
    color_config->mono_chrome = gst_av1_read_bit (br);

  color_config->num_planes = color_config->mono_chrome ? 1 : 3;

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
    /* duplicate variables into parser struct */
    parser->subsampling_x = color_config->subsampling_x;
    parser->subsampling_y = color_config->subsampling_y;
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
      if (color_config->bit_depth == 12) {
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

  /* duplicate variables into parser struct */
  parser->subsampling_x = color_config->subsampling_x;
  parser->subsampling_y = color_config->subsampling_y;

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_timing_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TimingInfo * timing_info)
{
  GstAV1ParserResult retval;

  GST_AV1_DEBUG ();

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
  GST_AV1_DEBUG ();

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
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

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

  GST_AV1_DEBUG ();

  memset (seq_header, 0, sizeof (GstAV1SequenceHeaderOBU));
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
        seq_header->
            operating_points[i].initial_display_delay_present_for_this_op =
            gst_av1_read_bit (br);
        if (seq_header->
            operating_points[i].initial_display_delay_present_for_this_op)
          seq_header->operating_points[i].initial_display_delay_minus_1 =
              gst_av1_read_bits (br, 4);
      }
    }
  }

  /*operatingPoint = choose_operating_point( ) */
  /*OperatingPointIdc = operating_point_idc[ operatingPoint ] */
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
    /*OrderHintBits = 0 */
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
      /*OrderHintBits = order_hint_bits_minus_1; */
    } else {
      /*OrderHintBits = 0; */
    }
  }
  seq_header->enable_superres = gst_av1_read_bit (br);
  seq_header->enable_cdef = gst_av1_read_bit (br);
  seq_header->enable_restoration = gst_av1_read_bit (br);

  retval = gst_av1_parse_color_config (parser, br, &(seq_header->color_config));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  seq_header->film_grain_params_present = gst_av1_read_bit (br);

  return gst_av1_skip_trailing_bits (parser, br);
}

GstAV1ParserResult
gst_av1_parse_temporal_delimiter_obu (GstAV1Parser * parser, GstBitReader * br)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);

  GST_AV1_DEBUG ();

  priv->seen_frame_header = 0;

  return gst_av1_skip_trailing_bits (parser, br);
}


static GstAV1ParserResult
gst_av1_parse_metadata_itut_t35 (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataITUT_T35 * itut_t35)
{
  GST_AV1_DEBUG ();

  itut_t35->itu_t_t35_country_code = gst_av1_read_bits (br, 8);
  if (itut_t35->itu_t_t35_country_code) {
    itut_t35->itu_t_t35_country_code_extention_byte = gst_av1_read_bits (br, 8);
  }
  /*TODO: Is skipping bytes necessary here? */
  /*ommiting itu_t_t35_payload_bytes */

  return GST_AV1_PARSER_OK;
}


static GstAV1ParserResult
gst_av1_parse_metadata_hdr_cll (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataHdrCll * hdr_cll)
{
  GST_AV1_DEBUG ();

  hdr_cll->max_cll = gst_av1_read_bits (br, 16);
  hdr_cll->max_fall = gst_av1_read_bits (br, 16);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_metadata_hdr_mdcv (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataHdrMdcv * hdr_mdcv)
{
  gint i;

  GST_AV1_DEBUG ();

  for (i = 0; i < 3; i++) {
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

  GST_AV1_DEBUG ();

  scalability->scalability_mode_idc = gst_av1_read_bits (br, 8);
  if (scalability->scalability_mode_idc == GST_AV1_SCALABILITY_SS) {
    scalability->spatial_layers_cnt_minus_1 = gst_av1_read_bits (br, 2);
    scalability->spatial_layer_dimensions_present_flag = gst_av1_read_bit (br);
    scalability->spatial_layer_description_present_flag = gst_av1_read_bit (br);
    scalability->temporal_group_description_present_flag =
        gst_av1_read_bit (br);
    gst_av1_read_bits (br, 3);  /*scalability_structure_reserved_3bits */
    if (scalability->spatial_layer_dimensions_present_flag) {
      for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++) {
        scalability->spatial_layer_max_width[i] = gst_av1_read_bits (br, 16);
        scalability->spatial_layer_max_height[i] = gst_av1_read_bits (br, 16);
      }
    }

    if (scalability->spatial_layer_description_present_flag) {
      for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++)
        scalability->spatial_layer_ref_id[i] = gst_av1_read_bits (br, 8);
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
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_metadata_timecode (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataTimecode * timecode)
{
  GST_AV1_DEBUG ();

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
          timecode->hours_value = gst_av1_read_bits (br, 5);
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

  GST_AV1_DEBUG ();

  memset (metadata, 0, sizeof (GstAV1MetadataOBU));

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

  return gst_av1_skip_trailing_bits (parser, br);;
}


static GstAV1ParserResult
gst_av1_parse_superres_params_compute_image_size (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  /* superres_params() */
  if (seq_header->enable_superres) {
    frame_header->use_superres = gst_av1_read_bit (br);
  } else {
    frame_header->use_superres = 0;
  }

  if (frame_header->use_superres) {
    frame_header->coded_denom =
        gst_av1_read_bits (br, GST_AV1_SUPERRES_DENOM_BITS);
    frame_header->superres_denom =
        frame_header->coded_denom + GST_AV1_SUPERRES_DENOM_MIN;
  } else {
    frame_header->superres_denom = GST_AV1_SUPERRES_NUM;
  }
  frame_header->upscaled_width = frame_header->frame_width;
  frame_header->frame_width =
      (frame_header->upscaled_width * GST_AV1_SUPERRES_NUM +
      (frame_header->superres_denom / 2)) / frame_header->superres_denom;

  /* compute_image_size() */
  frame_header->mi_cols = 2 * ((frame_header->frame_width + 7) >> 3);
  frame_header->mi_rows = 2 * ((frame_header->frame_height + 7) >> 3);

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_frame_size (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1SequenceHeaderOBU *seq_header;

  GST_AV1_DEBUG ();

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
    frame_header->frame_width = frame_header->frame_width_minus_1 + 1;
    frame_header->frame_height = frame_header->frame_height_minus_1 + 1;
  } else {
    frame_header->frame_width = seq_header->max_frame_width_minus_1 + 1;
    frame_header->frame_height = seq_header->max_frame_height_minus_1 + 1;
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
  GST_AV1_DEBUG ();

  frame_header->render_and_frame_size_different = gst_av1_read_bit (br);
  if (frame_header->render_and_frame_size_different) {
    frame_header->render_width_minus_1 = gst_av1_read_bits (br, 16);
    frame_header->render_height_minus_1 = gst_av1_read_bits (br, 16);
    frame_header->render_width = frame_header->render_width_minus_1 + 1;
    frame_header->render_height = frame_header->render_height_minus_1 + 1;
  } else {
    frame_header->render_width = frame_header->upscaled_width;
    frame_header->render_height = frame_header->frame_height;
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

  GST_AV1_DEBUG ();

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
    frame_header->found_ref = gst_av1_read_bit (br);
    if (frame_header->found_ref == 1) {
      gint ref_idx = frame_header->ref_frame_idx[i];
      frame_header->upscaled_width =
          ref_info->entry[ref_idx].ref_upscaled_width;
      frame_header->frame_width = frame_header->upscaled_width;
      frame_header->frame_height = ref_info->entry[ref_idx].ref_frame_height;
      frame_header->render_width = ref_info->entry[ref_idx].ref_render_width;
      frame_header->render_height = ref_info->entry[ref_idx].ref_render_height;
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


  GST_AV1_DEBUG ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  color_config = &(parser->seq_header->color_config);

  quant_params->base_q_idx = gst_av1_read_bits (br, 8);

  quant_params->delta_q_ydc = gst_av1_bitstreamfn_delta_q (br, &retval);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  if (color_config->num_planes > 1) {
    if (color_config->separate_uv_delta_q) {
      quant_params->diff_uv_delta = gst_av1_read_bit (br);
    } else {
      quant_params->diff_uv_delta = 0;
    }
    quant_params->delta_q_udc = gst_av1_bitstreamfn_delta_q (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);
    quant_params->delta_q_uac = gst_av1_bitstreamfn_delta_q (br, &retval);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    if (quant_params->diff_uv_delta) {
      quant_params->delta_q_vdc = gst_av1_bitstreamfn_delta_q (br, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

      quant_params->delta_q_vac = gst_av1_bitstreamfn_delta_q (br, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

    } else {
      quant_params->delta_q_vdc = quant_params->delta_q_udc;
      quant_params->delta_q_vac = quant_params->delta_q_uac;
    }
  } else {
    quant_params->delta_q_udc = 0;
    quant_params->delta_q_uac = 0;
    quant_params->delta_q_vdc = 0;
    quant_params->delta_q_vac = 0;
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
  const guint8 segmentation_feature_bits[GST_AV1_SEG_LVL_MAX] =
      { 8, 6, 6, 6, 6, 3, 0, 0 };
  const guint8 segmentation_feature_signed[GST_AV1_SEG_LVL_MAX] =
      { 1, 1, 1, 1, 1, 0, 0, 0 };
  const guint8 segmentation_feature_max[GST_AV1_SEG_LVL_MAX] =
      { 255, GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER,
    GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER, 7, 0, 0
  };
  gint clipped_value, feature_value;

  GST_AV1_DEBUG ();

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
          seg_params->feature_enabled[i][j] = gst_av1_read_bit (br);
          clipped_value = 0;
          feature_value = 0;
          if (seg_params->feature_enabled[i][j]) {
            gint bitsToRead = segmentation_feature_bits[j];
            gint limit = segmentation_feature_max[j];
            if (segmentation_feature_signed[j] == 1) {
              feature_value =
                  gst_av1_bitstreamfn_su (br, 1 + bitsToRead, &retval);
              GST_AV1_EVAL_RETVAL_LOGGED (retval);
              clipped_value = CLAMP (limit * (-1), limit, feature_value);
            } else {
              feature_value = gst_av1_read_bits (br, bitsToRead);
              clipped_value = CLAMP (0, limit, feature_value);
            }
          }
          seg_params->feature_data[i][j] = clipped_value;
        }
      }
    }
  } else {
    for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
      for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
        seg_params->feature_enabled[i][j] = 0;
        seg_params->feature_data[i][j] = 0;
      }
    }
  }

  seg_params->seg_id_preskip = 0;
  seg_params->last_active_seg_id = 0;
  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      if (seg_params->feature_enabled[i][j]) {
        seg_params->last_active_seg_id = i;
        if (j >= GST_AV1_SEG_LVL_REF_FRAME) {
          seg_params->seg_id_preskip = 1;
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
  gboolean uniform_tile_spacing_flag;

  gint i;
  gint start_sb;
  gint sb_cols;
  gint sb_rows;
  gint sb_shift;
  gint sb_size;
  gint max_tile_width_sb, max_tile_height_sb;
  gint max_tile_area_sb;
  gint min_log2_tile_cols;
  gint max_log2_tile_cols;
  gint max_log2_tile_rows;
  gint min_log2_tile_rows;
  gint min_log2_tiles;
  gint increment_tile_cols_log2;
  gint increment_tile_rows_log2;
  gint width_in_sbs_minus_1;
  gint tile_width_sb, tile_height_sb;
  gint height_in_sbs_minus_1;
  gint max_width, max_height;
  gint tile_size_bytes_minus_1;
  gint size_sb;
  gint widest_tile_sb;

  GST_AV1_DEBUG ();

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

  sb_cols =
      seq_header->use_128x128_superblock ? ((frame_header->mi_cols +
          31) >> 5) : ((frame_header->mi_cols + 15) >> 4);
  sb_rows =
      seq_header->use_128x128_superblock ? ((frame_header->mi_rows +
          31) >> 5) : ((frame_header->mi_rows + 15) >> 4);
  sb_shift = seq_header->use_128x128_superblock ? 5 : 4;
  sb_size = sb_shift + 2;
  max_tile_width_sb = GST_AV1_MAX_TILE_WIDTH >> sb_size;
  max_tile_area_sb = GST_AV1_MAX_TILE_AREA >> (2 * sb_size);
  min_log2_tile_cols = gst_av1_helper_TileLog2 (max_tile_width_sb, sb_cols);
  max_log2_tile_cols = gst_av1_helper_TileLog2 (1, MIN (sb_cols,
          GST_AV1_MAX_TILE_COLS));
  max_log2_tile_rows = gst_av1_helper_TileLog2 (1, MIN (sb_rows,
          GST_AV1_MAX_TILE_ROWS));
  min_log2_tiles = MAX (min_log2_tile_cols,
      gst_av1_helper_TileLog2 (max_tile_area_sb, sb_rows * sb_cols));

  uniform_tile_spacing_flag = gst_av1_read_bit (br);
  if (uniform_tile_spacing_flag) {
    tile_info->tile_cols_log2 = min_log2_tile_cols;
    while (tile_info->tile_cols_log2 < max_log2_tile_cols) {
      increment_tile_cols_log2 = gst_av1_read_bit (br);
      if (increment_tile_cols_log2 == 1)
        tile_info->tile_cols_log2++;
      else
        break;
    }
    tile_width_sb =
        (sb_cols + (1 << tile_info->tile_cols_log2) -
        1) >> tile_info->tile_cols_log2;
    i = 0;
    for (start_sb = 0; start_sb < sb_cols; start_sb += tile_width_sb) {
      tile_info->mi_col_starts[i] = start_sb << sb_shift;
      i += 1;
    }
    tile_info->mi_col_starts[i] = frame_header->mi_cols;
    tile_info->tile_cols = i;

    min_log2_tile_rows = MAX (min_log2_tiles - tile_info->tile_cols_log2, 0);
    max_tile_height_sb = sb_rows >> min_log2_tile_rows;
    tile_info->tile_rows_log2 = min_log2_tile_rows;
    while (tile_info->tile_rows_log2 < max_log2_tile_rows) {
      increment_tile_rows_log2 = gst_av1_read_bit (br);
      if (increment_tile_rows_log2 == 1)
        tile_info->tile_rows_log2++;
      else
        break;
    }
    tile_height_sb =
        (sb_rows + (1 << tile_info->tile_rows_log2) -
        1) >> tile_info->tile_rows_log2;
    i = 0;
    for (start_sb = 0; start_sb < sb_rows; start_sb += tile_height_sb) {
      tile_info->mi_row_starts[i] = start_sb << sb_shift;
      i += 1;
    }
    tile_info->mi_row_starts[i] = frame_header->mi_rows;
    tile_info->tile_rows = i;
  } else {
    widest_tile_sb = 0;
    start_sb = 0;
    for (i = 0; start_sb < sb_cols; i++) {
      tile_info->mi_col_starts[i] = start_sb << sb_shift;
      max_width = MIN (sb_cols - start_sb, max_tile_width_sb);
      width_in_sbs_minus_1 = gst_av1_bitstreamfn_ns (br, max_width, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
      size_sb = width_in_sbs_minus_1 + 1;
      widest_tile_sb = MAX (size_sb, widest_tile_sb);
      start_sb += size_sb;
    }
    tile_info->mi_col_starts[i] = frame_header->mi_cols;
    tile_info->tile_cols = i;
    tile_info->tile_cols_log2 =
        gst_av1_helper_TileLog2 (1, tile_info->tile_cols);

    if (min_log2_tiles > 0)
      max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
    else
      max_tile_area_sb = sb_rows * sb_cols;

    max_tile_height_sb = MAX (max_tile_area_sb / widest_tile_sb, 1);


    start_sb = 0;
    for (i = 0; start_sb < sb_rows; i++) {
      tile_info->mi_row_starts[i] = start_sb << sb_shift;
      max_height = MIN (sb_rows - start_sb, max_tile_height_sb);
      height_in_sbs_minus_1 = gst_av1_bitstreamfn_ns (br, max_height, &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
      size_sb = height_in_sbs_minus_1 + 1;
      start_sb += size_sb;
    }

    tile_info->mi_row_starts[i] = frame_header->mi_rows;
    tile_info->tile_rows = i;
    tile_info->tile_rows_log2 =
        gst_av1_helper_TileLog2 (1, tile_info->tile_rows);
  }
  if (tile_info->tile_cols_log2 > 0 || tile_info->tile_rows_log2 > 0) {
    tile_info->context_update_tile_id =
        gst_av1_read_bits (br,
        tile_info->tile_cols_log2 + tile_info->tile_rows_log2);
    tile_size_bytes_minus_1 = gst_av1_read_bits (br, 2);
    tile_info->tile_size_bytes = tile_size_bytes_minus_1 + 1;
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

  GST_AV1_DEBUG ();

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

  if (frame_header->coded_lossless || frame_header->allow_intrabc) {
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
  if (seq_header->color_config.num_planes > 1) {
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
  GST_AV1_DEBUG ();

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

  GST_AV1_DEBUG ();

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

  GST_AV1_DEBUG ();

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

  if (frame_header->coded_lossless || frame_header->allow_intrabc
      || !seq_header->enable_cdef) {
    cdef_params->cdef_bits = 0;
    cdef_params->cdef_y_pri_strength[0] = 0;
    cdef_params->cdef_y_sec_strength[0] = 0;
    cdef_params->cdef_uv_pri_strength[0] = 0;
    cdef_params->cdef_uv_sec_strength[0] = 0;
    cdef_params->cdef_damping_minus_3 = 0;
    /*CdefDamping = 3 */
    return GST_AV1_PARSER_OK;
  }
  cdef_params->cdef_damping_minus_3 = gst_av1_read_bits (br, 2);
  /*CdefDamping = cdef_damping_minus_3 + 3 */
  cdef_params->cdef_bits = gst_av1_read_bits (br, 2);
  for (i = 0; i < (1 << cdef_params->cdef_bits); i++) {
    cdef_params->cdef_y_pri_strength[i] = gst_av1_read_bits (br, 4);
    cdef_params->cdef_y_sec_strength[i] = gst_av1_read_bits (br, 2);
    if (cdef_params->cdef_y_sec_strength[i] == 3)
      cdef_params->cdef_y_sec_strength[i] += 1;
    if (seq_header->color_config.num_planes > 1) {
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
  guint8 lr_type;
  gint i;
  const GstAV1FrameRestorationType remap_lr_type[4] = {
    GST_AV1_FRAME_RESTORE_NONE,
    GST_AV1_FRAME_RESTORE_SWITCHABLE,
    GST_AV1_FRAME_RESTORE_WIENER,
    GST_AV1_FRAME_RESTORE_SGRPROJ
  };

  GST_AV1_DEBUG ();

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

  if (frame_header->all_lossless || frame_header->allow_intrabc
      || !seq_header->enable_restoration) {
    lr_params->frame_restoration_type[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->frame_restoration_type[1] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->frame_restoration_type[2] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->uses_lr = 0;
    return GST_AV1_PARSER_OK;
  }

  lr_params->uses_lr = 0;
  lr_params->uses_chroma_lr = 0;
  for (i = 0; i < seq_header->color_config.num_planes; i++) {
    lr_type = gst_av1_read_bits (br, 2);
    lr_params->frame_restoration_type[i] = remap_lr_type[lr_type];
    if (lr_params->frame_restoration_type[i] != GST_AV1_FRAME_RESTORE_NONE) {
      lr_params->uses_lr = 1;
      if (i > 0) {
        lr_params->uses_chroma_lr = 1;
      }
    }
  }

  if (lr_params->uses_lr) {
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

    lr_params->loop_restoration_size[0] =
        GST_AV1_RESTORATION_TILESIZE_MAX >> (2 - lr_params->lr_unit_shift);
    if (seq_header->color_config.subsampling_x
        && seq_header->color_config.subsampling_y
        && lr_params->uses_chroma_lr) {
      lr_params->lr_uv_shift = gst_av1_read_bit (br);
    } else {
      lr_params->lr_uv_shift = 0;
    }

    lr_params->loop_restoration_size[1] =
        lr_params->loop_restoration_size[0] >> lr_params->lr_uv_shift;
    lr_params->loop_restoration_size[2] =
        lr_params->loop_restoration_size[0] >> lr_params->lr_uv_shift;
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_parse_tx_mode (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GST_AV1_DEBUG ();

  if (frame_header->coded_lossless == 1) {
    frame_header->tx_mode = GST_AV1_TX_MODE_ONLY_4x4;
  } else {
    frame_header->tx_mode_select = gst_av1_read_bit (br);
    if (frame_header->tx_mode_select) {
      frame_header->tx_mode = GST_AV1_TX_MODE_SELECT;
    } else {
      frame_header->tx_mode = GST_AV1_TX_MODE_LARGEST;
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
  gint skip_mode_allowed;
  gint forward_hint;
  gint forward_idx;
  gint backward_idx;
  gint backward_hint;
  gint ref_hint;
  gint second_forward_idx;
  gint second_forward_hint;

  GST_AV1_DEBUG ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  ref_info = &(parser->ref_info);

  skip_mode_allowed = 0;

  if (frame_header->frame_is_intra || !frame_header->reference_select
      || !seq_header->enable_order_hint) {
    frame_header->skip_mode_allowed = 0;
  } else {
    forward_idx = -1;
    backward_idx = -1;

    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      ref_hint = ref_info->entry[frame_header->ref_frame_idx[i]].ref_order_hint;
      if (gst_av1_get_relative_dist (parser->seq_header, ref_hint,
              frame_header->order_hint) < 0) {
        if (forward_idx < 0
            || gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                forward_hint) > 0) {
          forward_idx = i;
          forward_hint = ref_hint;
        }
      } else if (gst_av1_get_relative_dist (parser->seq_header, ref_hint,
              frame_header->order_hint) > 0) {
        if (backward_idx < 0
            || gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                backward_hint) < 0) {
          backward_idx = i;
          backward_hint = ref_hint;
        }
      }
    }

    if (forward_idx < 0) {
      skip_mode_allowed = 0;
    } else if (backward_idx >= 0) {
      skip_mode_allowed = 1;
      frame_header->skip_mode_frame[0] =
          GST_AV1_REF_LAST_FRAME + MIN (forward_idx, backward_idx);
      frame_header->skip_mode_frame[1] =
          GST_AV1_REF_LAST_FRAME + MAX (forward_idx, backward_idx);
    } else {
      second_forward_idx = -1;
      for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
        ref_hint =
            ref_info->entry[frame_header->ref_frame_idx[i]].ref_order_hint;
        if (gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                forward_hint) < 0) {
          if (second_forward_idx < 0
              || gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                  second_forward_hint) > 0) {
            second_forward_idx = i;
            second_forward_hint = ref_hint;
          }
        }
      }

      if (second_forward_idx < 0) {
        skip_mode_allowed = 0;
      } else {
        skip_mode_allowed = 1;
        frame_header->skip_mode_frame[0] =
            GST_AV1_REF_LAST_FRAME + MIN (forward_idx, second_forward_idx);
        frame_header->skip_mode_frame[1] =
            GST_AV1_REF_LAST_FRAME + MAX (forward_idx, second_forward_idx);
      }
    }
  }

  if (skip_mode_allowed) {
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

  GST_AV1_DEBUG ();

/*TODO: can while(1) be dangerous??*/
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
  GST_AV1_DEBUG ();

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

  GST_AV1_DEBUG ();

  return gst_av1_decode_unsigned_subexp_with_ref (br, high - low, r - low,
      retval) + low;
}


/*Hack-Warning: PrevGMParams[] are not implemented yet - parser should read right amount of bits, but value will be wrong.*/
static GstAV1ParserResult
gst_av1_parse_global_param (GstAV1Parser * parser, GstBitReader * br,
    GstAV1GlobalMotionParams * gm_params, GstAV1WarpModelType type, gint ref,
    gint idx)
{
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  gint prec_diff, wm_round, mx, r;
  gint abs_bits;
  gint prec_bits;
  /*gint sub; */

  GST_AV1_DEBUG ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  abs_bits = GST_AV1_GM_ABS_ALPHA_BITS;
  prec_bits = GST_AV1_GM_ALPHA_PREC_BITS;
  if (idx < 2) {
    if (type == GST_AV1_WARP_MODEL_TRANSLATION) {
      abs_bits =
          GST_AV1_GM_ABS_TRANS_ONLY_BITS -
          (frame_header->allow_high_precision_mv ? 0 : 1);
      prec_bits =
          GST_AV1_GM_TRANS_ONLY_PREC_BITS -
          (frame_header->allow_high_precision_mv ? 0 : 1);
    } else {
      abs_bits = GST_AV1_GM_ABS_TRANS_BITS;
      prec_bits = GST_AV1_GM_TRANS_PREC_BITS;
    }
  }

  prec_diff = GST_AV1_WARPEDMODEL_PREC_BITS - prec_bits;
  wm_round = (idx % 3) == 2 ? (1 << GST_AV1_WARPEDMODEL_PREC_BITS) : 0;
  /*sub = (idx % 3) == 2 ? (1 << prec_bits) : 0; */
  mx = (1 << abs_bits);
  /*int r = (PrevGmParams[ref][idx] >> prec_diff) - sub; TODO: PrevGMParams is missing */
  r = 0;                        /*HACK-Warning PrevGmParams are not supported yet - bits for reading are defined with mx parameter */
  gm_params->gm_params[ref][idx] =
      (gst_av1_decode_signed_subexp_with_ref (br, -mx, mx + 1, r,
          &retval) << prec_diff) + wm_round;
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

  GST_AV1_DEBUG ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  for (ref = GST_AV1_REF_LAST_FRAME; ref <= GST_AV1_REF_ALTREF_FRAME; ref++) {
    gm_params->gm_type[ref] = GST_AV1_WARP_MODEL_IDENTITY;
    for (i = 0; i < 6; i++) {
      gm_params->gm_params[ref][i] =
          ((i % 3 == 2) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0);
    }
  }

  if (frame_header->frame_is_intra)
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
    gm_params->gm_type[ref] = type;

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
  gint num_pos_chroma, num_pos_luma;

  GST_AV1_DEBUG ();

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
    /*reset_grain_params() TODO: implement reset_grain_params */
    return GST_AV1_PARSER_OK;
  }

  fg_params->apply_grain = gst_av1_read_bit (br);

  if (!fg_params->apply_grain) {
    /*reset_grain_params() TODO: impl. */
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

    /* TODO- look at the following:
       tempGrainSeed = grain_seed
       load_grain_params( film_grain_params_ref_idx )
       grain_seed = tempGrainSeed
       return
       END TODO */
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
  num_pos_luma = 2 * fg_params->ar_coeff_lag * (fg_params->ar_coeff_lag + 1);
  if (fg_params->num_y_points) {
    num_pos_chroma = num_pos_luma + 1;
    for (i = 0; i < num_pos_luma; i++)
      fg_params->ar_coeffs_y_plus_128[i] = gst_av1_read_bits (br, 8);
  } else {
    num_pos_chroma = num_pos_luma;
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cb_points) {
    for (i = 0; i < num_pos_chroma; i++)
      fg_params->ar_coeffs_cb_plus_128[i] = gst_av1_read_bits (br, 8);
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cr_points) {
    for (i = 0; i < num_pos_chroma; i++)
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
gst_av1_mark_ref_frames (GstAV1Parser * parser, GstBitReader * br, gint id_len)
{
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i, diff_len;

  GST_AV1_DEBUG ();

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


  diff_len = seq_header->delta_frame_id_length_minus_2 + 2;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if (frame_header->current_frame_id > (1 << diff_len)) {
      if (ref_info->entry[i].ref_frame_id > frame_header->current_frame_id ||
          ref_info->entry[i].ref_frame_id <
          (frame_header->current_frame_id - (1 << diff_len)))
        ref_info->entry[i].ref_valid = 0;
    } else {
      if (ref_info->entry[i].ref_frame_id > frame_header->current_frame_id
          && ref_info->entry[i].ref_frame_id <
          ((1 << id_len) + frame_header->current_frame_id - (1 << diff_len)))
        ref_info->entry[i].ref_valid = 0;
    }
  }

  return GST_AV1_PARSER_OK;
}



static gint
gst_av1_get_qindex_ignoreDeltaQ (GstAV1QuantizationParams * quantization_params,
    gint segment_id)
{
  /* This only handles the ignorDeltaQ case */
  return quantization_params->base_q_idx;
}


static GstAV1ParserResult
gst_av1_parse_uncompressed_frame_header (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);
  GstAV1ParserResult retval;
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i, op_num, segment_id, all_frames;
  gint id_len = 0;

  GST_AV1_DEBUG ();

  if (!parser->seq_header) {
    GST_LOG ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  seq_header = parser->seq_header;

  ref_info = &(parser->ref_info);

  memset (frame_header, 0, sizeof (GstAV1FrameHeaderOBU));
  parser->frame_header = frame_header;

  if (seq_header->frame_id_numbers_present_flag)
    id_len =
        seq_header->additional_frame_id_length_minus_1 +
        seq_header->delta_frame_id_length_minus_2 + 3;

  all_frames = (1 << GST_AV1_NUM_REF_FRAMES) - 1;

  if (seq_header->reduced_still_picture_header) {
    frame_header->show_existing_frame = 0;
    frame_header->frame_type = GST_AV1_KEY_FRAME;
    frame_header->frame_is_intra = 1;
    frame_header->show_frame = 1;
    frame_header->showable_frame = 0;
  } else {
    frame_header->show_existing_frame = gst_av1_read_bit (br);
    if (frame_header->show_existing_frame) {
      frame_header->frame_to_show_map_idx = gst_av1_read_bits (br, 3);
      if (seq_header->decoder_model_info_present_flag
          && !seq_header->timing_info.equal_picture_interval)
        /*inline temporal_point_info */
        frame_header->frame_presentation_time =
            gst_av1_read_bits (br,
            seq_header->
            decoder_model_info.frame_presentation_time_length_minus_1 + 1);
      frame_header->refresh_frame_flags = 0;
      if (seq_header->frame_id_numbers_present_flag) {
        frame_header->display_frame_id = gst_av1_read_bits (br, id_len);
      }

      frame_header->frame_type =
          ref_info->entry[frame_header->frame_to_show_map_idx].ref_frame_type;

      if (frame_header->frame_type == GST_AV1_KEY_FRAME) {
        frame_header->refresh_frame_flags = all_frames;
      }
      if (seq_header->film_grain_params_present) {
        /*load_grain_params( frame_to_show_map_idx ) TODO: load_grain_params */
      }
      return GST_AV1_PARSER_OK;
    }

    frame_header->frame_type = gst_av1_read_bits (br, 2);
    frame_header->frame_is_intra =
        (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME
        || frame_header->frame_type == GST_AV1_KEY_FRAME);
    frame_header->show_frame = gst_av1_read_bit (br);
    if (frame_header->show_frame && seq_header->decoder_model_info_present_flag
        && !seq_header->timing_info.equal_picture_interval)
      /*inline temporal_point_info */
      frame_header->frame_presentation_time =
          gst_av1_read_bits (br,
          seq_header->
          decoder_model_info.frame_presentation_time_length_minus_1 + 1);

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
    /*HACK-Warning: we are deviating form Spec because GST_AV1_NUM_REF_FRAMES is out of bound added -1 */
    for (i = 0; i < GST_AV1_NUM_REF_FRAMES - 1; i++) {
      ref_info->entry[i].ref_valid = 0;
      ref_info->entry[i].ref_order_hint = 0;
    }
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      frame_header->order_hints[GST_AV1_REF_LAST_FRAME + i] = 0;
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

  if (frame_header->frame_is_intra) {
    frame_header->force_integer_mv = 1;
  }

  if (seq_header->frame_id_numbers_present_flag) {
    /*PrevFrameID = current_frame_id */
    frame_header->current_frame_id = gst_av1_read_bits (br, id_len);
    gst_av1_mark_ref_frames (parser, br, id_len);
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
  /*OrderHint = order_hint */

  if (frame_header->frame_is_intra || frame_header->error_resilient_mode) {
    frame_header->primary_ref_frame = GST_AV1_PRIMARY_REF_NONE;
  } else {
    frame_header->primary_ref_frame = gst_av1_read_bits (br, 3);
  }

  if (seq_header->decoder_model_info_present_flag) {
    frame_header->buffer_removal_time_present_flag = gst_av1_read_bit (br);
    if (frame_header->buffer_removal_time_present_flag) {
      for (op_num = 0; op_num <= seq_header->operating_points_cnt_minus_1;
          op_num++) {
        if (seq_header->
            operating_points[op_num].decoder_model_present_for_this_op) {
          gint opPtIdc = seq_header->operating_points[op_num].idc;
          gint inTemporalLayer = (opPtIdc >> priv->temporal_id) & 1;
          gint inSpatialLayer = (opPtIdc >> (priv->spatial_id + 8)) & 1;
          if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer))
            frame_header->buffer_removal_time[op_num] =
                gst_av1_read_bits (br,
                seq_header->
                decoder_model_info.buffer_removal_time_length_minus_1 + 1);
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
    frame_header->refresh_frame_flags = all_frames;
  } else {
    frame_header->refresh_frame_flags = gst_av1_read_bits (br, 8);
  }

  if (!frame_header->frame_is_intra
      || frame_header->refresh_frame_flags != all_frames) {
    if (frame_header->error_resilient_mode && seq_header->enable_order_hint) {
      for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
        frame_header->ref_order_hint[i] =
            gst_av1_read_bits (br, seq_header->order_hint_bits_minus_1 + 1);
        if (frame_header->ref_order_hint[i] !=
            ref_info->entry[i].ref_order_hint)
          ref_info->entry[i].ref_valid = 0;
      }
    }
  }
  /*here comes a concious simplification of the Spec handeling KEY_FRAME and INTRA_ONLY_FRAME the if case */
  if (frame_header->frame_type == GST_AV1_KEY_FRAME
      || frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME) {
    gst_av1_parse_frame_size (parser, br, frame_header);
    gst_av1_parse_render_size (parser, br, frame_header);
    if (frame_header->allow_screen_content_tools
        && frame_header->upscaled_width == frame_header->frame_width) {
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
        /*set_frame_refs() */
      }
    }
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      if (!frame_header->frame_refs_short_signaling)
        frame_header->ref_frame_idx[i] = gst_av1_read_bits (br, 3);

      if (seq_header->frame_id_numbers_present_flag) {
        frame_header->delta_frame_id_minus_1 =
            gst_av1_read_bits (br,
            seq_header->delta_frame_id_length_minus_2 + 2);
        frame_header->expected_frame_id[i] =
            ((frame_header->current_frame_id + (1 << id_len) -
                (frame_header->delta_frame_id_minus_1 + 1)) % (1 << id_len));
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

    /*inline read_interpolation_filter */
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

  if (!frame_header->frame_is_intra) {
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      gint refFrame = GST_AV1_REF_LAST_FRAME + 1;
      gint hint =
          ref_info->entry[frame_header->ref_frame_idx[i]].ref_order_hint;
      frame_header->order_hints[refFrame] = hint;
      if (!seq_header->enable_order_hint) {
        frame_header->ref_frame_sign_bias[refFrame] = 0;
      } else {
        frame_header->ref_frame_sign_bias[refFrame] = (gst_av1_get_relative_dist (parser->seq_header, hint, frame_header->order_hint) > 0);     /*TODO: OrderHint present? */
      }
    }
  }
  if (seq_header->reduced_still_picture_header
      || frame_header->disable_cdf_update)
    frame_header->disable_frame_end_update_cdf = 1;
  else
    frame_header->disable_frame_end_update_cdf = gst_av1_read_bit (br);

  /*SPECs sets upt CDFs here - for codecparser we are ommitting this section. */
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
  frame_header->coded_lossless = 1;
  for (segment_id = 0; segment_id < GST_AV1_MAX_SEGMENTS; segment_id++) {
    /* HACK-Warning: Spec calls get_qindex here - but we are using a shortcut handeling only the ignoreDeltaQ case, becaue current_q_index is not decoded at the moment */
    /*qindex = get_qindex( 1, segment_id ); */
    gint qindex =
        gst_av1_get_qindex_ignoreDeltaQ (&(frame_header->quantization_params),
        segment_id);

    frame_header->lossless_array[segment_id] = (qindex == 0)
        && (frame_header->quantization_params.delta_q_ydc == 0)
        && (frame_header->quantization_params.delta_q_uac == 0)
        && (frame_header->quantization_params.delta_q_udc == 0)
        && (frame_header->quantization_params.delta_q_vac == 0)
        && (frame_header->quantization_params.delta_q_vdc == 0);
    if (!frame_header->lossless_array[segment_id])
      frame_header->coded_lossless = 0;
    if (frame_header->quantization_params.using_qmatrix) {
      if (frame_header->lossless_array[segment_id]) {
        frame_header->seg_qm_level[0][segment_id] = 15;
        frame_header->seg_qm_level[1][segment_id] = 15;
        frame_header->seg_qm_level[2][segment_id] = 15;
      } else {
        frame_header->seg_qm_level[0][segment_id] =
            frame_header->quantization_params.qm_y;
        frame_header->seg_qm_level[1][segment_id] =
            frame_header->quantization_params.qm_u;
        frame_header->seg_qm_level[2][segment_id] =
            frame_header->quantization_params.qm_v;
      }
    }
  }
  frame_header->all_lossless = frame_header->coded_lossless
      && (frame_header->frame_width == frame_header->upscaled_width);

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


  /*inlined frame_reference_mode() */
  if (frame_header->frame_is_intra) {
    frame_header->reference_select = 0;
  } else {
    frame_header->reference_select = gst_av1_read_bit (br);
  }

  retval = gst_av1_parse_skip_mode_params (parser, br, frame_header);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  if (frame_header->frame_is_intra ||
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
gst_av1_load_reference_frame (GstAV1Parser * parser)
{
  /* Note: gst_av1_reference_frame_update() is only partialy implemented
   * Check: AV1 Bitstream Spec Chapter 7.20
   */
  GstAV1FrameHeaderOBU *frame_header;
  GstAV1ReferenceFrameInfo *ref_info;

  GST_AV1_DEBUG ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  ref_info = &(parser->ref_info);

  if (frame_header->frame_to_show_map_idx > GST_AV1_NUM_REF_FRAMES - 1)
    return GST_AV1_PARSER_ERROR;

  frame_header->current_frame_id =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_frame_id;
  frame_header->frame_type =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_frame_type;
  frame_header->upscaled_width =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_upscaled_width;
  frame_header->frame_width =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_frame_width;
  frame_header->frame_height =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_frame_height;
  frame_header->render_width =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_render_width;
  frame_header->render_height =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_render_height;
  frame_header->order_hint =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_order_hint;
  frame_header->mi_cols =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_mi_cols;
  frame_header->mi_rows =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_mi_rows;
  parser->subsampling_x =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_subsampling_x;
  parser->subsampling_x =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_subsampling_y;
  parser->bit_depth =
      ref_info->entry[frame_header->frame_to_show_map_idx].ref_bit_depth;

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_reference_frame_update (GstAV1Parser * parser)
{
  /* Note: gst_av1_reference_frame_update() is only partialy implemented
   * Check: AV1 Bitstream Spec Chapter 7.20
   */


  GstAV1FrameHeaderOBU *frame_header;
  GstAV1ReferenceFrameInfo *ref_info;
  gint i;

  GST_AV1_DEBUG ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  ref_info = &(parser->ref_info);


  if (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME
      && frame_header->refresh_frame_flags == 0xff)
    return GST_AV1_PARSER_BITSTREAM_ERROR;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES - 1; i++) {
    if ((frame_header->refresh_frame_flags >> i) & 1) {
      ref_info->entry[i].ref_valid = 1;
      ref_info->entry[i].ref_frame_id = frame_header->current_frame_id;
      ref_info->entry[i].ref_frame_type = frame_header->frame_type;
      ref_info->entry[i].ref_upscaled_width = frame_header->upscaled_width;
      ref_info->entry[i].ref_frame_width = frame_header->frame_width;
      ref_info->entry[i].ref_frame_height = frame_header->frame_height;
      ref_info->entry[i].ref_render_width = frame_header->render_width;
      ref_info->entry[i].ref_render_height = frame_header->render_height;
      ref_info->entry[i].ref_order_hint = frame_header->order_hint;
      ref_info->entry[i].ref_mi_cols = frame_header->mi_cols;
      ref_info->entry[i].ref_mi_rows = frame_header->mi_rows;
      ref_info->entry[i].ref_subsampling_x = parser->subsampling_x;
      ref_info->entry[i].ref_subsampling_y = parser->subsampling_y;
      ref_info->entry[i].ref_bit_depth = parser->bit_depth;
    }
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
gst_av1_decode_frame_wrapup (GstAV1Parser * parser)
{
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  /*if(frame_header->show_existing_frame = 0 ) {
     decoder would perform post processing filtering see section 7.4. Decode frame wrapup Process
     if(frame_header->segmentation_params.segmentation_enabled && !frame_header->segmentation_params.segmentation_enabled) {
     SegmentIds[ row ][ col ] is set equal to PrevSegmentIds[ row ][ col ] for row = 0..mi_rows-1, for col = 0..mi_cols-1.
     }
     } else {
   */
  if (frame_header->show_existing_frame) {
    if (frame_header->frame_type == GST_AV1_KEY_FRAME) {
      retval = gst_av1_load_reference_frame (parser);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
    }
  }

  retval = gst_av1_reference_frame_update (parser);
  GST_AV1_EVAL_RETVAL_LOGGED (retval);

  return GST_AV1_PARSER_OK;
}


GstAV1ParserResult
gst_av1_parse_tile_list_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TileListOBU * tile_list)
{
  gint tile, ftile;

  GST_AV1_DEBUG ();

  memset (tile_list, 0, sizeof (GstAV1TileListOBU));

  tile_list->output_frame_width_in_tiles_minus_1 = gst_av1_read_bits (br, 8);
  tile_list->output_frame_height_in_tiles_minus_1 = gst_av1_read_bits (br, 8);
  tile_list->tile_count_minus_1 = gst_av1_read_bits (br, 16);
  for (tile = 0; tile <= tile_list->tile_count_minus_1; tile++) {
    tile_list->entry[tile].anchor_frame_idx = gst_av1_read_bits (br, 8);
    tile_list->entry[tile].anchor_tile_row = gst_av1_read_bits (br, 8);
    tile_list->entry[tile].anchor_tile_col = gst_av1_read_bits (br, 8);
    tile_list->entry[tile].tile_data_size_minus_1 = gst_av1_read_bits (br, 16);
    tile_list->entry[tile].coded_tile_data =
        g_slice_alloc (tile_list->entry[tile].tile_data_size_minus_1 + 1);
    if (tile_list->entry[tile].coded_tile_data == NULL) {
      /* in case of allocation error - undo all previous allocations */
      for (ftile = 0; ftile < tile; ftile++)
        g_slice_free1 (tile_list->entry[ftile].tile_data_size_minus_1 + 1,
            tile_list->entry[ftile].coded_tile_data);
      return GST_AV1_ALLOCATION_ERROR;
    }
    /* TODO read coded_tile_data - for now we are skipping */
    gst_av1_bit_reader_skip_bytes (br,
        tile_list->entry[tile].tile_data_size_minus_1 + 1);
  }

  return gst_av1_skip_trailing_bits (parser, br);
}


GstAV1ParserResult
gst_av1_free_coded_tile_data_from_tile_list_obu (GstAV1TileListOBU * tile_list)
{
  gint tile;

  GST_AV1_DEBUG ();

  for (tile = 0; tile <= tile_list->tile_count_minus_1; tile++)
    g_slice_free1 (tile_list->entry[tile].tile_data_size_minus_1 + 1,
        tile_list->entry[tile].coded_tile_data);

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_tile_group_obu (GstAV1Parser * parser, GstBitReader * br,
    gsize sz, GstAV1TileGroupOBU * tile_group)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);
  GstAV1ParserResult retval;
  GstAV1FrameHeaderOBU *frame_header;
  gint tile_num, end_bit_pos, header_bytes, start_bit_pos;

  GST_AV1_DEBUG ();

  if (!parser->frame_header) {
    GST_LOG ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }
  frame_header = parser->frame_header;

  memset (tile_group, 0, sizeof (GstAV1TileGroupOBU));

  tile_group->num_tiles =
      frame_header->tile_info.tile_cols * frame_header->tile_info.tile_rows;
  start_bit_pos = gst_av1_bit_reader_get_pos (br);
  tile_group->tile_start_and_end_present_flag = 0;

  if (tile_group->num_tiles > 1)
    tile_group->tile_start_and_end_present_flag = gst_av1_read_bit (br);
  if (tile_group->num_tiles == 1
      || !tile_group->tile_start_and_end_present_flag) {
    tile_group->tg_start = 0;
    tile_group->tg_end = tile_group->num_tiles - 1;
  } else {
    gint tileBits =
        frame_header->tile_info.tile_cols_log2 +
        frame_header->tile_info.tile_rows_log2;
    tile_group->tg_start = gst_av1_read_bits (br, tileBits);
    tile_group->tg_end = gst_av1_read_bits (br, tileBits);
  }

  gst_av1_bit_reader_skip_to_byte (br);

  end_bit_pos = gst_av1_bit_reader_get_pos (br);
  header_bytes = (end_bit_pos - start_bit_pos) / 8;
  sz -= header_bytes;

  for (tile_num = tile_group->tg_start; tile_num <= tile_group->tg_end;
      tile_num++) {
    tile_group->entry[tile_num].tile_row =
        tile_num / frame_header->tile_info.tile_cols;
    tile_group->entry[tile_num].tile_col =
        tile_num % frame_header->tile_info.tile_cols;
    /*if last tile */
    if (tile_num == tile_group->tg_end) {
      tile_group->entry[tile_num].tile_size = sz;
    } else {
      gint tile_size_minus_1 =
          gst_av1_bitstreamfn_le (br, frame_header->tile_info.tile_size_bytes,
          &retval);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);
      tile_group->entry[tile_num].tile_size = tile_size_minus_1 + 1;
      sz -=
          tile_group->entry[tile_num].tile_size -
          frame_header->tile_info.tile_size_bytes;
    }

    tile_group->entry[tile_num].mi_row_start =
        frame_header->tile_info.mi_row_starts[tile_group->entry[tile_num].
        tile_row];
    tile_group->entry[tile_num].mi_row_end =
        frame_header->tile_info.mi_row_starts[tile_group->entry[tile_num].
        tile_row + 1];
    tile_group->entry[tile_num].mi_col_start =
        frame_header->tile_info.mi_col_starts[tile_group->entry[tile_num].
        tile_col];
    tile_group->entry[tile_num].mi_col_end =
        frame_header->tile_info.mi_col_starts[tile_group->entry[tile_num].
        tile_col + 1];
    tile_group->entry[tile_num].current_q_index =
        frame_header->quantization_params.base_q_idx;
    /* Skipped
       init_symbol( tile_size )
       decode_tile( )
       exit_symbol( )
     */
    gst_av1_bit_reader_skip_bytes (br, tile_group->entry[tile_num].tile_size);
  }

  if (tile_group->tg_end == tile_group->num_tiles - 1) {
    /* Skipped
       if ( !disable_frame_end_update_cdf ) {
       frame_end_update_cdf( )
       }
       decode_frame_wrapup( )
     */
    retval = gst_av1_decode_frame_wrapup (parser);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    priv->seen_frame_header = 0;
  }

  return GST_AV1_PARSER_OK;
}


GstAV1ParserResult
gst_av1_parse_frame_header_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);
  GstAV1ParserResult retval;

  GST_AV1_DEBUG ();

  if (priv->seen_frame_header == 1) {
    /*frame_header holds vaild data */
    return GST_AV1_PARSER_OK;
  } else {
    retval = gst_av1_parse_uncompressed_frame_header (parser, br, frame_header);
    GST_AV1_EVAL_RETVAL_LOGGED (retval);

    priv->seen_frame_header = 1;

    if (frame_header->show_existing_frame) {
      /*decode_frame_warpup(); */
      retval = gst_av1_decode_frame_wrapup (parser);
      GST_AV1_EVAL_RETVAL_LOGGED (retval);

      priv->seen_frame_header = 0;
    } else {
      /*tile_num = 0; */
      priv->seen_frame_header = 1;
    }
  }

  return gst_av1_skip_trailing_bits (parser, br);
}

GstAV1ParserResult
gst_av1_parse_frame_obu (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameOBU * frame)
{
  GstAV1ParserPrivate *priv = GST_AV1_PARSER_GET_PRIVATE (parser);
  GstAV1ParserResult retval;
  gint end_bit_pos, header_bytes, start_bit_pos;

  GST_AV1_DEBUG ();

  start_bit_pos = gst_av1_bit_reader_get_pos (br);
  retval = gst_av1_parse_frame_header_obu (parser, br, &(frame->frame_header));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  gst_av1_bit_reader_skip_to_byte (br);

  end_bit_pos = gst_av1_bit_reader_get_pos (br);
  header_bytes = (end_bit_pos - start_bit_pos) / 8;
  retval =
      gst_av1_parse_tile_group_obu (parser, br,
      priv->obu_size - header_bytes, &(frame->tile_group));
  GST_AV1_EVAL_RETVAL_LOGGED (retval);


  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult
gst_av1_parse_annexb_unit_size (GstAV1Parser * parser, GstBitReader * br,
    gsize * unit_size)
{
  GstAV1ParserResult retval;

  GST_AV1_DEBUG ();

  *unit_size = gst_av1_bitstreamfn_leb128 (br, &retval);
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

  /* not necessary because of g_slice_new0 (instead of g_slice_new) */
  /* memset (parser, 0, sizeof (GstAV1Parser)); */

  parser->priv = g_slice_new0 (GstAV1ParserPrivate);

  if (!parser->priv) {
    g_slice_free (GstAV1Parser, parser);
    return NULL;
  }

  /* not necessary because of g_slice_new0 (instead of g_slice_new) */
  /* memset (parser->priv, 0, sizeof (GstAV1ParserPrivate)); */

  return parser;
}

void
gst_av1_parser_free (GstAV1Parser * parser)
{
  if (parser->priv) {
    g_slice_free (GstAV1ParserPrivate, parser->priv);
  }

  if (parser) {
    g_slice_free (GstAV1Parser, parser);
  }
}
