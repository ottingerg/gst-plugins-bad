/* gstvp9parser.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <gst/base/gstbitreader.h>
#include "gstav1parser.h"

#define gst_av1_read_bit(br) gst_bit_reader_get_bits_uint8_unchecked(br, 1)
#define gst_av1_read_bits(br, bits) gst_bit_reader_get_bits_uint32_unchecked(br, bits)
#define gst_av1_bit_reader_skip(br, bits) gst_bit_reader_skip(br,bits)
#define gst_bit_reader_skip_to_byte(br) gst_av1_bit_reader_skip_to_byte(br)


//TODO - guint64 Pointer is dangerous ?? (if other type is used?)
GstAV1ParserResult gst_av1_bistreamfn_leb128(GstBitReader *br, guint64 *value)
{
  char leb128_byte;

  *value = 0;

  for (int i = 0; i < 8; i++) {
      leb128_byte = gst_av1_read_bits(br,8);
      *value |= ( ((int)leb128_byte & 0x7f) << (i*7) );
      if ( !(leb128_byte & 0x80) ) {
          break;
      }
  }
  if (*value < ((1 << 32) - 1))) { //check for bitstream conformance see chapter4.10.5
    return GST_AV1_PARSER_OK;
  else
    return GST_AV1_PARSER_ERROR;
}

//TODO - guint32 Pointer is dangerous ?? (if other type is used?)
GstAV1ParserResult gst_av1_bistreamfn_uvlc(GstBitReader *br, guint32 *value)
{
  int leadingZero = 0;
  int readv;
  bool done;
  int bitsread = 0;

  while(1) {
    done = gst_av1_read_bit(br)
    bitsread ++;
    if(done) {
      break;
    }
    leadingZero ++;
  }
  if (leadingZero >= 32) {
    value = (1<<32) - 1;
    return bitsread;
  }
  readv = gst_av1_read_bits(br,leadingZero);
  bitsread += leadingZero;
  value = readv + ( 1 << leadingZero) - 1;

  return GST_AV1_PARSER_OK;
}

//TODO - gint8 Pointer is dangerous ?? (if other type is used?)

GstAV1ParserResult gst_av1_bistreamfn_su(GstBitReader *br, guint8 n , gint8 *value)
{
    int v,signMask;

    v = gst_av1_read_bits(br,n);
    signMask = 1 << (n-1);
    if( v & signMask)
     *value = v - 2*signMask;
    else
     *value = v;

     return GST_AV1_PARSER_OK;
}


int gst_av1_helpers_FloorLog2( x ) {
  int s = 0;

  while ( x != 0 ) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}


//TODO - gint8 Pointer is dangerous ?? (if other type is used?)

GstAV1ParserResult gst_av1_bitstreamfn_ns(GstBitReader *br, guint8 n , gint8 *value)
{
  int w = gst_av1_helpers_FloorLog2(n) + 1;
  int m = (1 << w) - n;
  int v = gst_av1_read_bits(br,w-1);
  if ( v < m )
    return v;
  int extra_bit = gst_av1_read_bit(br);
  return (v << 1) - m + extra_bit;
}

GstAV1ParserResult gst_av1_parse_obu_header( GstBitReader *br, GstAV1OBUHeader *obu_header)
{
  obo_header->obu_forbidden_bit = gst_av1_read_bit(br);

  if(obo_header->obu_forbidden_bit != 0)
    return GST_AV1_PARSER_ERROR;

  obu_header->obu_type = gst_av1_read_bits(br, 4);
  obu_header->obu_extention_flag = gst_av1_read_bit(br);
  obu_header->obu_has_size_field = gst_av1_read_bit(br);
  obu_header->obu_reserved_1bit = gst_av1_read_bit(br);

  if(obu_header->obu_extention_flag) {
    obu_header->temporal_id = gst_av1_read_bits(br, 3);
    obu_header->spatial_id = gst_av1_read_bits(br, 2);
    obu_header->extension_header_reserved_3bits = gst_av1_read_bits(br, 3);
  }

  if(obu_header->obu_has_size_field)
    return gst_av1_bistreamfn_leb128(br,&(obu_header->obus_size));
  else
    return GST_AV1_PARSER_OK;
}

GstAV1ParserResult gst_av1_parse_color_config( GstBitReader *br, GstAV1ColorConfig *color_config, guint8 seq_profile )
{
  color_config->high_bitdepth = gst_av1_read_bit(br);
  if( seq_profile == 2 && color_config->high_bitdepth) {
    color_config->twelve_bit = gst_av1_read_bit(br);
    color_config->BitDepth = color_config->twelve_bit ? 12 : 10
  } else if ( seq_profile <= 2 ) {
    color_config->BitDepth = color_config->high_bitdepth ? 10 : 8
  }

  if ( seq_profile == 1 )
    color_config->mono_chrome = 0;
  else
    mono_chrome = gst_av1_read_bit(br);

  color_config->NumPlanes = color_config->mono_chrome ? 1 : 3

  color_config->color_description_present_flag = gst_av1_read_bit(br);
  if ( color_config->color_description_present_flag ) {
    color_config->color_primaries = gst_av1_read_bits(br,8);
    color_config->transfer_characteristics = gst_av1_read_bits(br,8);
    color_config->matrix_coefficients = gst_av1_read_bits(br,8);
  } else {
    color_config->color_primaries = GST_AV1_CP_UNSPECIFIED;
    color_config->transfer_characteristics = GST_AV1_TC_UNSPECIFIED;
    color_config->matrix_coefficients = GST_AV1_MC_UNSPECIFIED;
  }

  if ( color_config->mono_chrome ) {
    color_config->color_range = gst_av1_read_bit(br);
    color_config->subsampling_x = 1;
    color_config->subsampling_y = 1;
    color_config->chroma_sample_position = GST_AV1_CSP_UNKNOWN;
    color_config->separate_uv_delta_q = 0;
    return GST_AV1_PARSER_OK;
  } else if ( color_config->color_primaries == GST_AV1_CP_BT_709 &&
              color_config->transfer_characteristics == GST_AV1_TC_SRGB &&
              color_config->matrix_coefficients == GST_AV1_MC_IDENTITY ) {
    color_config->color_range = 1;
    color_config->subsampling_x = 0;
    color_config->subsampling_y = 0;
  } else {
    color_config->color_range = gst_av1_read_bit(br);
    if ( seq_profile == 0 ) {
      color_config->subsampling_x = 1;
      color_config->subsampling_y = 1;
    } else if ( seq_profile == 1 ) {
      color_config->subsampling_x = 0;
      color_config->subsampling_y = 0;
    } else {
      if ( color_config->BitDepth == 12 ) {
         color_config->subsampling_x = gst_av1_read_bit(br);
         if ( color_config->subsampling_x )
           color_config->subsampling_y = gst_av1_read_bit(br);
         else
           color_config->subsampling_y = 0;
      } else {
        color_config->subsampling_x = 1;
        color_config->subsampling_y = 0;
      }
    }
    if (color_config->subsampling_x && color_config->subsampling_y)
      color_config->chroma_sample_position = gst_av1_read_bits(br,2);
  }

  color_config->separate_uv_delta_q = gst_av1_read_bit(br);

  return GST_AV1_PARSER_OK;
}

GstAV1ParserResult gst_av1_parse_timing_info( GstBitReader *br, GstAV1TimingInfo *timing_info )
{
  timing_info->num_units_in_display_tick = gst_av1_read_bits(br,32);
  timing_info->time_scale = gst_av1_read_bits(br,32);
  timing_info->equal_picture_interval = gst_av1_read_bit(br);
  if( timing_info->equal_picture_interval )
    gst_av1_bistreamfn_uvlc(br,&(timing_info->num_ticks_per_picture_minus_1));

  return GST_AV1_PARSE_OK;
}


GstAV1ParserResult gst_av1_parse_decoder_model_info( GstBitReader *br, GstAV1DecoderModelInfo *decoder_model_info )
{
  decoder_model_info->bitrate_scale = gst_av1_read_bits(br,4);
  decoder_model_info->buffer_size_scale = gst_av1_read_bits(br,4);
  decoder_model_info->buffer_delay_length_minus_1 = gst_av1_read_bits(br,5);
  decoder_model_info->num_units_in_decoding_tick = gst_av1_read_bits(br,32);
  decoder_model_info->buffer_removal_time_length_minus_1 = gst_av1_read_bits(br,5);
  decoder_model_info->frame_presentation_time_length_minus_1 = gst_av1_read_bits(br,5);

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_operating_parameters_info(GstBitReader *br,GstAV1OperatingPoint *op_point, guint8 buffer_delay_length_minus_1);
{
  gst_av1_bistreamfn_uvlc(br, &(op_point->bitrate_minus_1));
  gst_av1_bistreamfn_uvlc(br, &(op_point->buffer_size_minus_1));
  op_point->cbr_flag = gst_av1_read_bit(br);
  op_point->decoder_buffer_delay = gst_av1_read_bits(br,buffer_delay_length_minus_1+1)
  op_point->encoder_buffer_delay = gst_av1_read_bits(br,buffer_delay_length_minus_1+1)
  op_point->low_delay_mode_flag = gst_av1_read_bit(br);

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_sequence_header_obu( GstBitReader *br, GstAV1SequenceHeaderOBU *seq_header)
{
  bzero(seq_header, sizeof(GstAV1SequenceHeaderOBU));

  seq_header->seq_profile = gst_av1_read_bits(br, 3);
  seq_header->still_picture = gst_av1_read_bit(br);
  seq_header->reduced_still_picture_header = gst_av1_read_bit(br);

  if(seq_header->reduced_still_picture_header) {
    seq_header->timing_info_present_flag = 0;
    seq_header->decoder_model_info_present_flag = 0;
    seq_header->initial_display_delay_present_flag = 0;
    seq_header->operating_points_cnt_minus_1 = 0;
    seq_header->operating_points[0].idc = 0;
    seq_header->operating_points[0].seq_level_idx = gst_av1_read_bits(br, 5);
    seq_header->operating_points[0].seq_tier = 0;
    seq_header->operating_points[0].decoder_model_present_for_this_op[ 0 ] = 0;
    seq_header->operating_points[0].initial_display_delay_present_for_this_op[ 0 ] = 0;
  } else {
    seq_header->timing_info_present_flag = gst_av1_read_bit(br);

    if ( seq_header->timing_info_present_flag ) {
      gst_av1_parse_timing_info(br,&(seq_header->timing_info));
      seq_header->decoder_model_info_present_flag = gst_av1_read_bit(br);
      if ( seq_header->decoder_model_info_present_flag )
        gst_av1_parse_decoder_model_info(br,&(seq_header->decoder_model_info));
      } else {
      seq_header->decoder_model_info_present_flag = 0;
    }

    seq_header->initial_display_delay_present_flag = gst_av1_read_bit(br);
    seq_header->operating_points_cnt_minus_1 = gst_av1_read_bits(br, 5);

    for ( int i = 0; i <= seq_header->operating_points_cnt_minus_1; i++ ) {
      seq_header->operating_points[i].idc = gst_av1_read_bits(br, 12);
      seq_header->operating_points[i].seq_level_idx = gst_av1_read_bits(br, 5);
      if( seq_header->operating_points[i].seq_level_idx > 7 ) {
        seq_header->operating_points[i].seq_tier = gst_av1_read_bit(br);
      else
        seq_header->operating_points[i].seq_tier = 0;
      }
      if ( seq_header->decoder_model_info_present_flag ) {
         seq_header->operating_points[i].decoder_model_present_for_this_op = gst_av1_read_bit(br);
         if ( seq_header->operating_points[i].decoder_model_present_for_this_op )
           gst_av1_parse_operating_parameters_info( br,&(seq_header->operating_points[i]), seq_header->decoder_model_info.buffer_removal_delay_length_minus_1 );
      } else {
        seq_header->operating_points[i].decoder_model_present_for_this_op = 0
      }

      if ( seq_header->initial_display_delay_present_flag ) {
        seq_header->operating_points[i].initial_display_delay_present_for_this_op = gst_av1_read_bit(br);
        if ( seq_header->operating_points[i].initial_display_delay_present_for_this_op )
          seq_header->operating_points[i].initial_display_delay_minus_1 = gst_av1_read_bits(br,4);
      }
    }
  }

  //operatingPoint = choose_operating_point( )
  //OperatingPointIdc = operating_point_idc[ operatingPoint ]
  seq_header->frame_width_bits_minus_1 = gst_av1_read_bits(br, 4);
  seq_header->frame_height_bits_minus_1 = gst_av1_read_bits(br, 4);
  seq_header->max_frame_width_minus_1 = gst_av1_read_bits(br, frame_width_bits_minus_1+1);
  seq_header->max_frame_height_minus_1 = gst_av1_read_bits(br, frame_height_bits_minus_1+1);

  if(seq_header->reduced_still_picture_header)
    seq_header->frame_id_numbers_present_flag = 0;
  else
    seq_header->frame_id_numbers_present_flag = gst_av1_read_bit(br);

  if(seq_header->frame_id_numbers_present_flag) {
    seq_header->delta_frame_id_length_minus_2 = gst_av1_read_bits(br, 4);
    seq_header->additional_frame_id_length_minus_1 = = gst_av1_read_bits(br, 3);
  }

  seq_header->use_128x128_superblock = gst_av1_read_bit(br);
  seq_header->enable_filter_intra = gst_av1_read_bit(br);
  seq_header->enable_intra_edge_filter = gst_av1_read_bit(br);

  if(seq_header->reduced_still_picture_header) {
    seq_header->enable_interintra_compound = 0;
    seq_header->enable_masked_compound = 0;
    seq_header->enable_warped_motion = 0;
    seq_header->enable_dual_filter = 0;
    seq_header->enable_order_hint = 0;
    seq_header->enable_jnt_comp = 0;
    seq_header->enable_ref_frame_mvs = 0;
    seq_header->seq_force_screen_content_tools = GST_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
    //OrderHintBits = 0
  } else {
    seq_header->enable_interintra_compound = gst_av1_read_bit(br);
    seq_header->enable_masked_compound = gst_av1_read_bit(br);
    seq_header->enable_warped_motion = gst_av1_read_bit(br);
    seq_header->enable_dual_filter = gst_av1_read_bit(br);
    seq_header->enable_order_hint = gst_av1_read_bit(br);
    if( seq_header->enable_order_hint ) {
      seq_header->enable_jnt_comp = gst_av1_read_bit(br);
      seq_header->enable_ref_frame_mvs = gst_av1_read_bit(br);
    } else {
      seq_header->enable_jnt_comp = 0;
      seq_header->enable_ref_frame_mvs = 0;
    }
    seq_header->seq_choos_screen_content_tools = gst_av1_read_bit(br);
    if( seq_header->seq_choose_integer_mv_screen_content_tools ) {
      seq_header->seq_force_screen_content_tools = GST_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    else
      seq_header->seq_force_screen_content_tools = gst_av1_read_bit(br);

    if( seq_force_screen_content_tools > 0 ) {
      seq_header->seq_choose_integer_mv = gst_av1_read_bit(br);
      if( seq_header->seq_choose_integer_mv ) {
        seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
      else
        seq_header->seq_force_integer_mv = gst_av1_read_bit(br);
    } else {
      seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
    }
    if( seq_header->enable_order_hint ) {
      seq_header->order_hint_bits_minus_1 = gst_av1_read_bits(br,3);
      //OrderHintBits = order_hint_bits_minus_1 + 1
    } else {
      //OrderHintBits = 0;
    }
  }
  seq_header->enable_superres = gst_av1_read_bit(br);
  seq_header->enable_cdef = gst_av1_read_bit(br);
  seq_header->seq_enable_restoration = gst_av1_read_bit(br);

  gst_av1_parse_color_config(br,seq_header, &(seq_header->color_config));

  seq_header->film_grain_params_present = gst_av1_read_bit(br);

  return gst_bit_reader_skip_to_byte(br);
}

GstAV1ParserResult gst_av1_parse_metadata_itut_t35( GstBitReader *br, GstAV1MetadataITUT_T35 *itut_t35)
{
  itut_t35->itu_t_t35_country_code = gst_av1_read_bits(br,8);
  if(itut_t35->itu_t_t35_country_code) {
    itut_t35->itu_t_t35itu_t_t35_country_code_extension_byte = gst_av1_read_bits(br,8);
  }
  //ommiting itu_t_t35_payload_bytes

  return GST_AV1_PARSE_OK;
}


GstAV1ParserResult gst_av1_parse_metadata_hdr_cll( GstBitReader *br, GstAV1MetadataHdrCll *hdr_cll)
{

  hdr_cll->max_cll = gst_av1_read_bits(br,16);
  hdr_cll->max_fall = gst_av1_read_bits(br,16);


  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_metadata_hdr_mdcv( GstBitReader *br, GstAV1MetadataHdrMdcv *hdr_mdcv)
{

  for(int i = 0; i < 4; i++) {
    hdr_mdcv->primary_chromaticity_x[i] = gst_av1_read_bits(br,16);
    hdr_mdcv->primary_chromaticity_y[i] = gst_av1_read_bits(br,16);
  }

  hdr_mdcv->white_point_chromaticity_x = gst_av1_read_bits(br,16);
  hdr_mdcv->white_point_chromaticity_y = gst_av1_read_bits(br,16);

  hdr_mdcv->luminance_max = gst_av1_read_bits(br,32);
  hdr_mdcv->luminance_min = gst_av1_read_bits(br,32);

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_metadata_scalability( GstBitReader *br, GstAV1MetadataScalability *scalability)
{
  scalability->scalability_mode_idc = gst_av1_read_bits(br,8);
  scalability->spatial_layers_cnt_minus_1 = gst_av1_read_bits(br,2);
  scalability->spatial_layer_dimensions_present_flag = gst_av1_read_bit(br);
  scalability->spatial_layer_description_present_flag = gst_av1_read_bit(br);
  scalability->temporal_group_description_present_flag = gst_av1_read_bit(br);
  scalability->scalability_structure_reserved_3bits = gst_av1_read_bit(br);
  if ( scalability->spatial_layer_dimensions_present_flag ) {
    for ( i = 0; i <= scalability->spatial_layers_cnt_minus_1 ; i++ ) {
      spatial_layer_max_width[i] = gst_av1_read_bits(br,16);
      spatial_layer_max_height[i] = gst_av1_read_bits(br,16);
    }
  }

  if ( scalability->spatial_layer_description_present_flag ) {
    for ( i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++ )
      scalability->spatial_layer_ref_id[i] = gst_av1_read_bit(br);
  }

  if ( scalability->temporal_group_description_present_flag ) {
    scalabilty->temporal_group_size = gst_av1_read_bits(br,8);
    for ( i = 0; i < scalability->temporal_group_size; i++ ) {
      scalability->temporal_group_temporal_id[i] = gst_av1_read_bits(br,3);
      scalability->temporal_group_temporal_switching_up_point_flag[i] = gst_av1_read_bit(br);
      scalability->temporal_group_spatial_switching_up_point_flag[i] = gst_av1_read_bit(br);
      scalability->temporal_group_ref_cnt[i] = gst_av1_read_bits(br,3):
      for ( j = 0; j < temporal_group_ref_cnt[i]; j++ ) {
        temporal_group_ref_pic_diff[i][j] = gst_av1_read_bits(br,8);
      }
    }
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_metadata_timecode( GstBitReader *br, GstAV1MetadataTimecode *timecode)
{
  timecode->counting_type = gst_av1_read_bits(br,5);
  timecode->full_timestamp_flag = gst_av1_read_bit(br);
  timecode->discontinuity_flag = gst_av1_read_bit(br);
  timecode->cnt_dropped_flag = gst_av1_read_bit(br);
  timecode->n_frames = gst_av1_read_bits(br,9);

  if( timecode->full_timestamp_flag ) {
    timecode->seconds_value = gst_av1_read_bits(br,6);
    timecode->minutes_value = gst_av1_read_bits(br,6);
    timecode->hours_value = gst_av1_read_bits(br,5);
  } else {
    timecode->seconds_flag = gst_av1_read_bit(br);
    if( timecode->seconds_flag ) {
      timecode->seconds_value = gst_av1_read_bits(br,6);
      timecode->minutes_flag = gst_av1_read_bit(br);
      if( timecode->minutes_flag ) {
        timecode->minutes_value = gst_av1_read_bits(br,6);
        timecode->hours_flag = gst_av1_read_bit(br);
        if( timecode->hours_flag )
          timecode->hours_value = gst_av1_read_bits(br,6);
      }
    }
  }

  timecode->time_offset_length = gst_av1_read_bits(br,5);
  if( timecode->time_offset_length > 0 ) {
    timecode->time_offset_value = gst_av1_read_bits(br,timecode->time_offset_length);
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_metadata_obu(GstBitReader *br, GstAV1MetadataOBU *metadata)
{
  GstAV1ParserResult ret;

  ret = gst_av1_bistreamfn_leb128(br,&(metadata->metadata_type));

  if(ret == GST_AV1_PARSE_ERROR)
    return GST_AV1_PARSE_ERROR;

  switch(metadata->metadata_type)
  {
    case GST_AV1_METADATA_TYPE_ITUT_T35:
      ret = gst_av1_parse_metadata_itut_t35(br,&(metadata->itut_t35));
      break;
    case GST_AV1_METADATA_TYPE_HDR_CLL:
      ret = gst_av1_parse_metadata_hdr_cll(br,&(metadata->hdr_cll));
      break;
    case GST_AV1_METADATA_TYPE_HDR_MDCV:
      ret = gst_av1_parse_metadata_hdr_mdcv(br,&(metadata->hdr_mdcv));
      break;
    case GST_AV1_METADATA_TYPE_SCALABILITY:
      ret = gst_av1_parse_metadata_scalability(br,&(metadata->scalability));
      break;
    case GST_AV1_METADATA_TYPE_TIMECODE:
      ret = gst_av1_parse_metadata_timecode(br,&(metadata->timecode));
      break;
    default:
      return GST_AV1_PARSE_ERROR;
  }

  if(ret == GST_AV1_PARSE_OK)
    return gst_bit_reader_skip_to_byte;
  else
    return GST_AV1_PARSE_ERROR;
}


GstAV1ParserResult gst_av1_parse_superres_params_compute_image_size (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  if(seq_header->enable_superres)
    frame_header->use_superres = gst_av1_bit_read(br);
  else
    frame_header->use_superres = 0;

  if(frame_header->use_superres) {
    frame_header->coded_denom = gst_av1_read_bits(br, GST_AV1_SUPERRES_DENOM_BITS);
    frame_header->SuperresDenom = coded_denom + SUPERRES_DENOM_MIN;
  } else {
    frame_header->SuperresDenom = SUPERRES_NUM;
  }
  frame_header->UpscaledWidth = frame_header->FrameWidth;
  frame_header->FrameWidth = (frame_header->UpscaledWidth * SUPERRES_NUM + (frame_header->SuperresDenom / 2)) / frame_header->SuperresDenom;

  // compute_image_size:
  frame_header->MiCols = 2 * ( ( frame_header->FrameWidth + 7 ) >> 3 );
  frame_header->MiRows = 2 * ( ( frame_header->FrameHeight + 7 ) >> 3 );

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_frame_size_with_refs (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  for ( int i = 0; i < REFS_PER_FRAME; i++ ) {
    frame_header->found_ref = gst_av1_read_bit(br);
    if ( frame_header->found_ref == 1 ) {
      frame_header->UpscaledWidth = RefUpscaledWidth[frame_header->ref_frame_idx[i]];
      frame_header->FrameWidth = frame_header->UpscaledWidth;
      frame_header->FrameHeight = frames_header->RefFrameHeight[frame_header->ref_frame_idx[i]];
      frame_header->RenderWidth = frame_header->RefRenderWidth[frame_header->ref_frame_idx[i]];
      frame_header->RenderHeight = RefRenderHeight[frame_header->ref_frame_idx[i]];
      break;
    }
  }
  if ( frame_header->found_ref == 0 ) {
    gst_av1_parse_frame_size(br,frame_header,seq_header);
    gst_av1_parse_render_size(br,frame_header);
  } else {
    gst_av1_parse_superres_params_compute_image_size(br,frame_header,seq_header);
  }
}


GstAV1ParserResult gst_av1_parse_frame_size (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  if(frame_header->frame_size_override_flag ) {
    frame_header->frame_width_minus_1 = gst_av1_read_bits(br,seq_header->frame_width_bits_minus_1+1);
    frame_header->frame_height_minus_1 = gst_av1_read_bits(br,seq_header->frame_height_bits_minus_1+1);
    frame_header->FrameWidth = frame_header->frame_width_minus_1 + 1;
    frame_header->FrameHeight = frame_header->frame_height_minus_1 + 1;
  } else {
    frame_header->FrameWidth = seq_header->max_frame_width_minus_1 + 1;
    frame_header->FrameHeight= seq_header->max_frame_height_minus_1 + 1;
  }
  //TODO: superres_params()
  //TODO: compute_image_size()

  retrun GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_render_size (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header)
{
  frame_header->render_and_frame_size_different = gst_av1_read_bit(br);
  if( frame_header->render_and_frame_size_different ) {
    frame_header->render_width_minus_1 = gst_av1_read_bits(br,16);
    frame_header->render_height_minus_1 = gst_av1_read_bits(br,16);
    frame_header->RenderWidth = frame_header->render_width_minus_1 + 1;
    frame_header->RenderHeight = frame_header->render_height_minus_1 + 1;
  } else {
    frame_header->RenderWidth = frame_header->UpscaledWidth;
    frame_header->RenderHeight = frame_header->FrameHeight;
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_delta_q (GstBitReader *br, gint8 *delta_q)
{
  guint8 delta_coded = gst_av1_read_bit(br);
  if(delta_codec)
    gst_av1_bitstreamfn_su(br,7,delta_q)
  else
    delta_q = 0;

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_quantization_params (GstBitReader *br, GstAV1QuantizationParams *quant_params, GstAV1ColorConfig *color_config)
{

  gst_av1_parse_delta_q(br, &());

  quant_params->base_q_idx = gst_av1_read_bits(br,8);

  gst_av1_parse_delta_q(br, &(quant_params->DeltaQYDc));

  if ( color_config->NumPlanes > 1 ) {
    if ( color_config->separate_uv_delta_q )
      quant_params->diff_uv_delta = gst_av1_read_bit(br);
    else
      quant_params->diff_uv_delta = 0;
    gst_av1_parse_delta_q(br, &(quant_params->DeltaQUDc));
    gst_av1_parse_delta_q(br, &(quant_params->DeltaQUAc));

    if ( quant_params->diff_uv_delta ) {
      gst_av1_parse_delta_q(br, &(quant_params->DeltaQVDc));
      gst_av1_parse_delta_q(br, &(quant_params->DeltaQVAc));
    } else {
      quant_params->DeltaQVDc = quant_params->DeltaQUDc;
      quant_params->DeltaQVAc = quant_params->DeltaQUAc;
    }
  } else {
    quant_params->DeltaQUDc = 0
    quant_params->DeltaQUAc = 0
    quant_params->DeltaQVDc = 0
    quant_params->DeltaQVAc = 0
  }

  quant_params->using_qmatrix = gst_av1_read_bit(br);

  if ( quant_params->using_qmatrix ) {
    quant_params->qm_y = gst_av1_read_bits(br,4);
    quant_params->qm_u = gst_av1_read_bits(br,4);

    if ( !color_config->separate_uv_delta_q )
      quant_params->qm_v = quant_params->qm_u;
    else
      quant_params->qm_v = gst_av1_read_bits(br,4);
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_segmentation_params (GstBitReader *br, GstAV1SegmenationParams *seg_params, GstAV1FrameHeaderOBU *frame_header)
{
  const guint8 Segmentation_Feature_Bits[GST_AV1_SEG_LVL_MAX ] = { 8 , 6 , 6 , 6 , 6 , 3 , 0 , 0 };
  const guint8 Segmentation_Feature_Signed[ GST_AV1_SEG_LVL_MAX ] = { 1 , 1 , 1 , 1 , 1 , 0 , 0 , 0 };
  const guint8 Segmentation_Feature_Max[ GST_AV1_SEG_LVL_MAX ] = {255,255, GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER,GST_AV1_MAX_LOOP_FILTER,GST_AV1_MAX_LOOP_FILTER, 7 ,0 , 0 };

  seg_params->segmentation_enabled = gst_av1_read_bit(br);



  if( seg_params->segmentation_enabled ) {
    if( frame_header->primary_ref_frame == GST_AV1_PRIMARY_REF_NONE) {
      seg_params->segmentation_update_map = 1;
      seg_params->segmentation_temporal_update = 0;
      seg_params->segmentation_update_data = 1;
    } else {
      seg_params->segmentation_update_map = gst_av1_read_bit(br);
      if ( seg_params->segmentation_update_map )
        seg_params->segmentation_temporal_update = gst_av1_read_bit(br);
      segmentation_update_data = gst_av1_read_bit(br);
    }

    if ( seg_params->segmentation_update_data) {
      for ( int i = 0; i < GST_AV1_MAX_SEGMENTS; i++ ) {
        for ( int j = 0; j < GST_AV1_SEG_LVL_MAX; j++ ) {
          seg_params->FeatureEnabled[i][j] = gst_av1_read_bit(br);
          int clipped_value = 0;
          int feature_value = 0;
          if( seg_params->FeatureEnabled[i][j] ) {
            int bitsToRead = Segmentation_Feature_Bits[j];
            int limit = Segmentation_Feature_Max[j];
            if( Segmentation_Feature_Signed [j] ) {
              gst_av1_bitstreamfn_su(br,1+bitsToRead,&feature_value);
              clipped_value = gst_av1_clip3(limit * (-1), limit, feature_value);
            } else {
              feature_value = gst_av1_read_bits(br,bitsToRead);
              clipped_value = gst_av1_clip3(0,limit,feature_value);
            }
          }
          seg_params->FeatureData[i][j] = clipped_value;
        }
      }
    }
  } else {
    for ( i = 0; i < GST_AV1_MAX_SEGMENTS; i++ ) {
      for ( j = 0; j < GST_AV1_SEG_LVL_MAX; j++ ) {
        seg_params->FeatureEnabled[ i ][ j ] = 0;
        seg_params->FeatureData[ i ][ j ] = 0;
      }
    }
  }

  seg_params->SegIdPreSkip = 0
  seg_params->LastActiveSegId = 0
  for ( int i = 0; i < GST_AV1_MAX_SEGMENTS; i++ ) {
    for ( int j = 0; j < GST_AV1_SEG_LVL_MAX; j++ ) {
      if ( seg_params->FeatureEnabled[ i ][ j ] ) {
        seg_params->LastActiveSegId = i;
        if ( j >= GST_AV1_SEG_LVL_REF_FRAME ) {
          seg_params->SegIdPreSkip = 1;
        }
      }
    }
  }

  return GST_AV1_PARSE_OK;
}

int gst_av1_helper_tile_log2( int blkSize, int target ) {
  int k;
  for ( k = 0; (blkSize << k) < target; k++ ) {
  }
  return k;
}

GstAV1ParserResult gst_av1_parse_tile_info (GstBitReader *br, GstAV1TileInfo *tile_info, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{

  //TODO: Which values should be moved into GstAV1TileInfo? -> decide accordingly to spec 6.8.14
  int sbCols = seq_header->use_128x128_superblock ? ( ( frame_header->MiCols + 31 ) >> 5 ) : ( ( frame_header->MiCols + 15 ) >> 4 );
  int sbRows = seq_header->use_128x128_superblock ? ( ( frame_header->MiRows + 31 ) >> 5 ) : ( ( frame_header->MiRows + 15 ) >> 4 );
  int sbShift = seq_header->use_128x128_superblock ? 5 : 4;
  int sbSize = sbShift + 2;
  int maxTileWidthSb = GST_AV1_MAX_TILE_WIDTH >> sbSize;
  int maxTileAreaSb = GST_AV1_MAX_TILE_AREA >> ( 2 * sbSize );
  int minLog2TileCols = gst_av1_tile_log2(maxTileWidthSb, sbCols);
  int maxLog2TileCols = gst_av1_tile_log2(1, Min(sbCols, MAX_TILE_COLS));
  int maxLog2TileRows = gst_av1_tile_log2(1, Min(sbRows, MAX_TILE_ROWS));
  int minLog2Tiles = Max(minLog2TileCols,gst_av1_tile_log2(maxTileAreaSb, sbRows * sbCols));
  int increment_tile_cols_log2;

  tile_info->uniform_tile_spacing_flag = gst_av1_read_bit(br);
  if ( tile_info->uniform_tile_spacing_flag ) {
    tile_info->TileColsLog2 = minLog2TileCols;
     while ( tile_info->TileColsLog2 < maxLog2TileCols ) {
     increment_tile_cols_log2 = gst_av1_read_bit(br);
     if ( increment_tile_cols_log2 == 1 )
       tile_info->TileColsLog2++;
     else
       break;
  }
  int tileWidthSb = (sbCols + (1 << TileColsLog2) - 1) >> TileColsLog2
  i = 0
  for ( startSb = 0; startSb < sbCols; startSb += tileWidthSb ) {
  MiColStarts[ i ] = startSb << sbShift
  i += 1
  }
  MiColStarts[i] = MiCols
  TileCols = i


}

GstAV1ParserResult gst_av1_parse_loop_filter_params (GstBitReader *br, GstAV1LoopFilterParams *lf_params, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  if ( frame_header->CodedLossless || frame_header->allow_intrabc ) {
    lf_params->loop_filter_level[ 0 ] = 0;
    lf_params->loop_filter_level[ 1 ] = 0;
    lf_params->loop_filter_ref_deltas[ GST_AV1_INTRA_FRAME ] = 1;
    lf_params->loop_filter_ref_deltas[ GST_AV1_LAST_FRAME ] = 0;
    lf_params->loop_filter_ref_deltas[ GST_AV1_LAST2_FRAME ] = 0;
    lf_params->loop_filter_ref_deltas[ GST_AV1_LAST3_FRAME ] = 0;
    lf_params->loop_filter_ref_deltas[ GST_AV1_BWDREF_FRAME ] = 0;
    lf_params->loop_filter_ref_deltas[ GST_AV1_GOLDEN_FRAME ] = -1;
    lf_params->loop_filter_ref_deltas[ GST_AV1_ALTREF_FRAME ] = -1;
    lf_params->loop_filter_ref_deltas[ GST_AV1_ALTREF2_FRAME ] = -1;
    for (int i = 0; i < 2; i++ ) {
      lf_params->loop_filter_mode_deltas[i] = 0;
    }
    return GST_AV1_PARSE_OK;
  }

  lf_params->loop_filter_level[0] = gst_av1_read_bits(br,6);
  lf_params->loop_filter_level[1] = gst_av1_read_bits(br,6);
  if ( seq_header->color_config.NumPlanes > 1 ) {
    if ( lf_params->loop_filter_level[ 0 ] || lf_params->loop_filter_level[ 1 ] ) {
      lf_params->loop_filter_level[ 2 ] = gst_av1_read_bits(br,6);
      lf_params->loop_filter_level[ 3 ] = gst_av1_read_bits(br,6);
    }
  }
  lf_params->loop_filter_sharpness = gst_av1_read_bits(br,3);
  lf_params->loop_filter_delta_enabled = gst_av1_read_bit(br);

  if ( lf_params->loop_filter_delta_enabled ) {
    lf_params->loop_filter_delta_update = gst_av1_read_bit(br);
    if ( lf_params->loop_filter_delta_update) {
      for ( int i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++ ) {
        lf_params->update_ref_delta = gst_av1_read_bit(br);
        if ( lf_params->update_ref_delta)
          gst_av1_bitstreamfn_su(br,7,&(lf_params->loop_filter_ref_deltas[i]);
      }
      for ( int i = 0; i < 2; i++ ) {
        lf_params->update_mode_delta = gst_av1_read_bit(br);
        if ( lf_params->update_mode_delta )
          gst_av1_bitstreamfn_su(br,7,&(lf_params->loop_filter_mode_deltas[i]);
      }
    }
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_loop_filter_delta_params (GstBitReader *br, GstAV1LoopFilterParams *lf_params, GstAV1FrameHeaderOBU *frame_header)
{
  lf_params->delta_lf_present = 0;
  lf_params->delta_lf_res = 0;
  lf_params->delta_lf_multi = 0;

  if ( lf_params->delta_q_present ) {
    if ( !frame_header->allow_intrabc )
     lf_params->delta_lf_present = gst_av1_read_bit(br);
    if ( lf_params->delta_lf_present ) {
       lf_params->delta_lf_res = gst_av1_read_bits(br,2);
       lf_params->delta_lf_multi = gst_av1_read_bit(br);
    }
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_cdef_params (GstBitReader *br, GstAV1CDEFParams *cdef_params, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{

  if ( frame_header->CodedLossless || frame_header->allow_intrabc || !seq_header->enable_cdef) {
    cdef_params->cdef_bits = 0;
    cdef_params->cdef_y_pri_strength[0] = 0;
    cdef_params->cdef_y_sec_strength[0] = 0;
    cdef_params->cdef_uv_pri_strength[0] = 0;
    cdef_params->cdef_uv_sec_strength[0] = 0;
    cdef_params->cdef_damping_minus_3 = 0;
    //CdefDamping = 3
    return GST_AV1_PARSE_OK;
  }
  cdef_params->cdef_damping_minus_3 = gst_av1_read_bits(br,2);
  //CdefDamping = cdef_damping_minus_3 + 3
  cdef_params->cdef_bits = gst_av1_read_bits(br,2);
  for ( int i = 0; i < (1 << cdef_params->cdef_bits); i++ ) {
    cdef_params->cdef_y_pri_strength[i] = gst_av1_read_bits(br,4);
    cdef_params->cdef_y_sec_strength[i] = gst_av1_read_bits(br,2);
    if ( cdef_params->cdef_y_sec_strength[i] == 3 )
      cdef_params->cdef_y_sec_strength[i] += 1;
    if ( seq_header->color_config.NumPlanes > 1 ) {
      cdef_params->cdef_uv_pri_strength[i] = gst_av1_read_bits(br,4);
      cdef_params->cdef_uv_sec_strength[i] = gst_av1_read_bits(br,2);
      if ( cdef_params->cdef_uv_sec_strength[i] == 3 )
        cdef_params->cdef_uv_sec_strength[i] += 1;
    }
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_loop_restoration_params (GstBitReader *br, GstAV1LoopRestorationParams *lr_params, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  const GstAV1FrameRestorationType Remap_Lr_Type[4] = {
    GST_AV1_FRAME_RESTORE_NONE,
    GST_AV1_FRAME_RESTORE_SWITCHABLE,
    GST_AV1_FRAME_RESTORE_WIENER,
    GST_AV1_FRAME_RESTORE_SGRPROJ
  };

  if ( frame_header->AllLossless || frame_header->allow_intrabc || seq_header-> !enable_restoration ) {
    lr_params->FrameRestorationType[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->FrameRestorationType[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->FrameRestorationType[0] = GST_AV1_FRAME_RESTORE_NONE;
    lr_params->UsesLr = 0;
    return GST_AV1_PARSE_OK;
  }

  lr_params->UsesLr = 0
  lr_params->usesChromaLr = 0
  for (int i = 0; i < seq_header->color_config.NumPlanes; i++ ) {
    lr_params->lr_type = gst_av1_read_bits(br,2);
    lr_params->FrameRestorationType[i] = Remap_Lr_Type[lr_params->lr_type];
    if ( lr_params->FrameRestorationType[i] != GST_AV1_FRAME_RESTORE_NONE ) {
      lr_params->UsesLr = 1;
      if ( i > 0 ) {
        lr_params->usesChromaLr = 1;
      }
    }
  }

  if ( lr_params->UsesLr ) {
    if ( seq_header->use_128x128_superblock ) {
      lr_params->lr_unit_shift = gst_av1_read_bit(br);
      lr_params->lr_unit_shift++
    } else {
      lr_params->lr_unit_shift = gst_av1_read_bit(br);
      if ( lr_params->lr_unit_shift ) {
        lr_params->lr_unit_extra_shift = gst_av1_read_bit(br);
        lr_params->lr_unit_shift += lr_params->lr_unit_extra_shift;
      }
    }

    lr_params->LoopRestorationSize[ 0 ] = GST_AV1_RESTORATION_TILESIZE_MAX >> (2 - lr_params->lr_unit_shift)
    if ( seq_header->color_config.subsampling_x && seq_header->color_config.subsampling_y && lr_params->usesChromaLr ) {
      lr_params->lr_uv_shift = gst_av1_read_bit(br);
    } else {
      lr_params->lr_uv_shift = 0;
    }

    lr_params->LoopRestorationSize[ 1 ] = lr_params->LoopRestorationSize[ 0 ] >> lr_params->lr_uv_shift;
    lr_params->LoopRestorationSize[ 2 ] = lr_params->LoopRestorationSize[ 0 ] >> lr_params->lr_uv_shift;
  }

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_tx_mode (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header)
{
  if ( frame_header->CodedLossless == 1 ) {
    frame_header->TxMode =  GST_AV1_TX_MODE_ONLY_4X4;
  } else {
    frame_header->tx_mode_select = gst_av1_read_bit(br);
    if ( frame_header->tx_mode_select ) {
      frame_header->TxMode = GST_AV1_TX_MODE_SELECT;
    } else {
      frame_header->TxMode = GST_AV1_TX_MODE_LARGEST;
    }
  }
  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_loop_restoration_params (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header)
{
  int skipModeAllowed = 0;

  if( frame_header->FrameIsIntra || !frame_header->reference_select || !frame_header->enable_order_hint ) {
    frame_header->skipModeAllowed = 0;
  } else {
    int forwardIdx = -1;
    int forwardHint = 0;
    int backwardIdx = -1;
    int backwardHint = 0;
    int refHint = 0;

    for ( int i = 0; i < GST_AV1_REFS_PER_FRAME; i++ ) {
      refHint = frame_header->RefOrderHint[frame_header->ref_frame_idx[i]];
      if ( gst_av1_get_relative_dist( refHint, frame_header->OrderHint ) < 0 ) {
        if ( forwardIdx < 0 || gst_av1_get_relative_dist( refHint, forwardHint) > 0 ) {
          forwardIdx = i;
          forwardHint = refHint
        }
      } else if ( gst_av1_get_relative_dist( refHint, frame_header->OrderHint) > 0 ) {
        if ( backwardIdx < 0 || gst_av1_get_relative_dist( refHint, backwardHint) < 0 ) {
          backwardIdx = i
          backwardHint = refHint
        }
      }
    }

    if ( forwardIdx < 0 ) {
      skipModeAllowed = 0;
    } else if ( backwardIdx >= 0 ) {
      skipModeAllowed = 1;
      frame_header->SkipModeFrame[ 0 ] = GST_AV1_LAST_FRAME + Min(forwardIdx, backwardIdx);
      frame_header->SkipModeFrame[ 1 ] = GST_AV1_LAST_FRAME + Max(forwardIdx, backwardIdx);
    } else {
      int secondForwardIdx = -1;
      int secondForwardHint = 0;
      for ( int i = 0; i < GST_AV1_REFS_PER_FRAME; i++ ) {
        refHint = frame_header->RefOrderHint[ frame_header->ref_frame_idx[ i ] ];
        if ( gst_av1_get_relative_dist( refHint, forwardHint ) < 0 ) {
          if ( secondForwardIdx < 0 || get_relative_dist( refHint, secondForwardHint ) > 0 ) {
            secondForwardIdx = i;
            secondForwardHint = refHint;
          }
        }
      }

      if ( secondForwardIdx < 0 ) {
        skipModeAllowed = 0;
      } else {
        skipModeAllowed = 1;
        frame_header->SkipModeFrame[ 0 ] = GST_AV1_LAST_FRAME + Min(forwardIdx, secondForwardIdx);
        frame_header->SkipModeFrame[ 1 ] = GST_AV1_LAST_FRAME + Max(forwardIdx, secondForwardIdx);
      }
    }
  }

  if ( skipModeAllowed ) {
    frame_header->skip_mode_present = gst_av1_read_bit(br);
  } else {
    frame_header->skip_mode_present = 0;
  }

  return GST_AV1_PARSE_OK;
}

int gst_av1_inverse_recenter( int r, int v ) {
  if ( v > 2 * r )
    return v;
  else if ( v & 1 )
    return r - ((v + 1) >> 1);
  else
    return r + (v >> 1);
}

int gst_av1_decode_subexp( GstBitReader *br, int numSyms ) {
  int i = 0;
  int mk = 0;
  int k = 3;
  int subexp_final_bits = 0;
  int subexp_more_bits = 0;
  int subexp_bits = 0;

//TODO: can while(1) be dangerous??
  while ( 1 ) {
    int b2 = i ? k + i - 1 : k;
    int a = 1 << b2;
    if ( numSyms <= mk + 3 * a ) {
      gst_av1_bitstreamfn_ns(br, numSyms - mk, &subexp_final_bits);
      return subexp_final_bits + mk;
    } else {
      subexp_more_bits = gst_av1_read_bit(br);
      if ( subexp_more_bits ) {
        i++;
        mk += a;
      } else {
        subexp_bits  = gst_av1_read_bits(br,b2);
        return subexp_bits + mk;
      }
    }
  }
}


int gst_av1_decode_unsigned_subexp_with_ref( GstBitReader *br, mx, r ) {
  int v = gst_av1_decode_subexp( br, mx )
  if ( (r << 1) <= mx ) {
    return gst_av1_inverse_recenter(r, v);
  } else {
    return mx - 1 - gst_av1_inverse_recenter(mx - 1 - r, v);
  }
}

int gst_av1_decode_signed_subexp_with_ref(GstBitReader *br, int low, int high, int r) {
  return gst_av1_decode_unsigned_subexp_with_ref(br, high - low, r - low) + low;
}


//Hack-Warning: PrevGMParams[] are not implemented yet - parser should read right amount of bits, but value will be wrong.
GstAV1ParserResult gst_av1_parse_global_param (GstBitReader *br, GstAV1GlobalMotionParams *gm_params,GstAV1FrameHeaderOBU *frame_header,GstAV1WarpModelType type ,int ref, int idx) {
  int absBits = GST_AV1_GM_ABS_ALPHA_BITS;
  int precBits = GST_AV1_GM_ALPHA_PREC_BITS;

  if ( idx < 2 ) {
    if ( type == GST_AV1_WARP_MODEL_TRANSLATION ) {
      absBits = GST_AV1_GM_ABS_TRANS_ONLY_BITS - (frame_header->allow_high_precision_mv ? 0 : 1);
      precBits = GST_AV1_GM_TRANS_ONLY_PREC_BITS - (frame_header->allow_high_precision_mv ? 0 : 1);
    } else {
      absBits = GST_AV1_GM_ABS_TRANS_BITS;
      precBits = GST_AV1_GM_TRANS_PREC_BITS;
    }
  }

  int precDiff = GST_AV1_WARPEDMODEL_PREC_BITS - precBits;
  int wm_round = (idx % 3) == 2 ? (1 << GST_AV1_WARPEDMODEL_PREC_BITS) : 0;
  int sub = (idx % 3) == 2 ? (1 << precBits) : 0;
  int mx = (1 << absBits)
  //int r = (PrevGmParams[ref][idx] >> precDiff) - sub; //TODO: PrevGMParams is missing
  int r; //Hack-Warning PrevGmParams are not supported yet - bits for reading are defined with mx parameter
  gm_params->gm_params[ref][idx] = (gst_av1_decode_signed_subexp_with_ref(br,-mx, mx + 1, r )<< precDiff) + wm_round;

  return GST_AV1_PARSE_OK;
}


GstAV1ParserResult gst_av1_parse_global_motion_params (GstBitReader *br, GstAV1GlobalMotionParams *gm_params, GstAV1FrameHeaderOBU *frame_header)
{
  GstAV1WarpModelType type;

  for ( int ref = GST_AV1_LAST_FRAME; ref <= GST_AV1_ALTREF_FRAME; ref++ ) {
    gm_params->GmType[ ref ] = GST_AV1_WARP_MODEL_IDENTITY;
    for ( int i = 0; i < 6; i++ ) {
      gm_params->gm_params[ ref ][ i ] = ( ( i % 3 == 2 ) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0 )
    }

  if ( frame_header->FrameIsIntra )
    return GST_AV1_PARSE_OK;

  for ( int ref = GST_AV1_LAST_FRAME; ref <= GST_AV1_ALTREF_FRAME; ref++ ) {
    gm_params->is_global[ref] = gst_av1_read_bit(br);
    if ( gm_params->is_global[ref] ) {
      gm_params->is_rot_zoom[ref] = gst_av1_read_bit(br);
      if ( gm_params->is_rot_zoom[ref] ) {
        type = GST_AV1_WARP_MODEL_ROTZOOM;
      } else {
        gm_params->is_translation[ref] = gst_av1_read_bit(br);
        type = gm_params->is_translation[ref] ? GST_AV1_WARP_MODEL_TRANSLATION : GST_AV1_WARP_MODEL_AFFINE;
      }
    } else {
      type = GST_AV1_WARP_MODEL_IDENTITY;
    }
    gm_params->GmType[ref] = type;

    if ( type >= GST_AV1_WARP_MODEL_ROTZOOM ) {
      gst_av1_read_global_param(gm_params,type,ref,2);
      gst_av1_read_global_param(gm_params,type,ref,3);
      if ( type == GST_AV1_WARP_MODEL_AFFINE ) {
        gst_av1_read_global_param(gm_params,type,ref,4);
        gst_av1_read_global_param(gm_params,type,ref,5);
      } else {
        gm_params->gm_params[ref][4] = gm_params->gm_params[ref][3]*(-1);
        gm_params->gm_params[ref][5] = gm_params->gm_params[ref][2];
      }
    }
    if ( type >= GST_AV1_WARP_MODEL_TRANSLATION ) {
      gst_av1_read_global_param(gm_params,type,ref,0);
      gst_av1_read_global_param(gm_params,type,ref,1);
    }
  }

  return GST_AV1_PARSE_OK;
}


GstAV1ParserResult gst_av1_parse_film_grain_params (GstBitReader *br, GstAV1FilmGrainParams *fg_params, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  if(!seq_header->film_grain_params_present || !frame_header->show_frame && !frame_header->showable_frame) {
    //reset_grain_params() //TODO: implement reset_grain_params
    return GST_AV1_PARSE_OK;
  }

  fg_params->apply_grain = gst_av1_read_bit(br);

  if ( !fg_params->apply_grain ) {
    //reset_grain_params() //TODO: impl.
    return GST_AV1_PARSE_OK;
  }

  fg_params->grain_seed = gst_av1_read_bits(br);

  if ( frame_header->frame_type == GST_AV1_INTER_FRAME )
    fg_params->update_grain = gst_av1_read_bit(br);
  else
    fg_params->update_grain = 1;

  if ( !fg_params->update_grain ) {
    fg_params->film_grain_params_ref_idx = gst_av1_read_bits(br,3);

    // TODO- look at the following:
    /*tempGrainSeed = grain_seed
    load_grain_params( film_grain_params_ref_idx )
    grain_seed = tempGrainSeed
    return*/
    // END TODO
  }

  fg_params->num_y_points = gst_av1_read_bits(br,4);

  for(int i = 0; i < fg_params->num_y_points; i++) {
    fg_params->point_y_value[i] = gst_av1_read_bits(br,8);
    fg_params->point_y_scaling[i] = gst_av1_read_bits(br,8);
  }

  if(seq_header->color_config.mono_chrome) {
    fg_params->chroma_scaling_from_luma = 0;
  } else {
    fg_params->chroma_scaling_from_luma = gst_av1_read_bit(br);
  }

  if ( seq_header->color_config.mono_chrome || fg_params->chroma_scaling_from_luma ||
     ( seq_header->color_config.subsampling_x == 1 && seq_header->color_config.subsampling_y == 1 &&
     fg_params->num_y_points == 0 ) ) {
    fg_params->num_cb_points = 0;
    fg_params->num_cr_points = 0;
  } else {
    fg_params->num_cb_points = gst_av1_read_bits(br,4);
  for ( int i = 0; i < num_cb_points; i++ ) {
    fg_params->point_cb_value[i] = gst_av1_read_bits(br,8);
    fg_params->point_cb_scaling[i] = gst_av1_read_bits(br,8);
  }
  fg_params->num_cr_points = gst_av1_read_bits(br,4);
  for ( i = 0; i < num_cr_points; i++ ) {
    fg_params->point_cr_value[ i ]= gst_av1_read_bits(br,8);
    fg_params->point_cr_scaling[ i ]= gst_av1_read_bits(br,8);
  }

  fg_params->grain_scaling_minus_8 = gst_av1_read_bits(br,2);
  fg_params->ar_coeff_lag = gst_av1_read_bits(br,2);
  int numPosLuma = 2 * fg_params->ar_coeff_lag * ( fg_params->ar_coeff_lag + 1 );
  if ( fg_params->num_y_points ) {
    int numPosChroma = numPosLuma + 1;
    for ( int i = 0; i < numPosLuma; i++ )
      fg_params->ar_coeffs_y_plus_128[ i ] = gst_av1_read_bits(br,8);
  } else {
    int numPosChroma = numPosLuma;
  }

  if ( fg_params->chroma_scaling_from_luma || fg_params->num_cb_points ) {
    for ( i = 0; i < numPosChroma; i++ )
      fg_params->ar_coeffs_cb_plus_128[i] = gst_av1_read_bits(br,8);
  }

  if ( fg_params->chroma_scaling_from_luma || fg_params->num_cr_points ) {
    for ( int i = 0; i < numPosChroma; i++ )
      fg_params->ar_coeffs_cr_plus_128[i] = gst_av1_read_bits(br,8);
  }


  fg_params->ar_coeff_shift_minus_6 = gst_av1_read_bits(br,2);
  fg_params->grain_scale_shift = gst_av1_read_bits(br,2);

  if ( fg_params->num_cb_points ) {
    fg_params->cb_mult= gst_av1_read_bits(br,8);
    fg_params->cb_luma_mult= gst_av1_read_bits(br,8);
    fg_params->cb_offset= gst_av1_read_bits(br,9);
  }

  if ( fg_params->num_cr_points ) {
    fg_params->cr_mult= gst_av1_read_bits(br,8);
    fg_params->cr_luma_mult= gst_av1_read_bits(br,8);
    fg_params->cr_offset= gst_av1_read_bits(br,9);
  }

  fg_params->overlap_flag = gst_av1_read_bit(br);
  fg_params->clip_to_restricted_range = gst_av1_read_bit(br);

  return GST_AV1_PARSE_OK;
}

GstAV1ParserResult gst_av1_parse_uncompressed_frame_header (GstBitReader *br, GstAV1FrameHeaderOBU *frame_header, GstAV1SequenceHeaderOBU *seq_header)
{
  int idLen = 0;

  if(seq_header->frame_id_number_present_flag)
    idLen = seq_header->additional_frame_id_length_minus_1 + seq_header->delta_frame_id_length_minus_2 + 3;

  int allFrames = (1<<GST_AV1_NUM_REF_FRAMES) - 1;

  if(seq_header->reduced_still_picture_header) {
    frame_header->show_existing_frame = 0;
    frame_header->frame_type = GST_AV1_KEY_FRAME;
    frame_header->FrameIsIntra = 1;
    frame_header->show_frame = 1;
    frame_header->showable_frame = 0;
  } else {
    frame_header->show_existing_frame = gst_av1_read_bit(br);
    if( frame_header->show_existing_frame ) {
      frame_header->frame_to_show_map_idx = gst_av1_read_bits(br,3);
      if( seq_header->decoder_model_info_present_flag && !seq_header->timing_info.equal_picture_interval )
        frame_header->frame_presentation_time = gst_av1_read_bits(br,seq_header->decoder_model_info.frame_presentation_time_length_minus_1+1);
      frame_header->refresh_frame_flags = 0;
      if( seq_header->frame_id_numbers_present_flag ){
        frame_header->display_frame_id = gst_av1_read_bits(br,idLen);
      }

      frame_header->frame_type frame_type = RefFrameType[ frame_to_show_map_idx ] // TODO: RefFrameType not available in the scope yet!!!

      if ( frame_header->frame_type == GST_AV1_KEY_FRAME ) {
         frame_header->refresh_frame_flags = allFrames;
      }
      if ( seq_header->film_grain_params_present ) {
      //load_grain_params( frame_to_show_map_idx ) //TODO: load_grain_params
      }
      return GST_AV1_PARSE_OK;
    }

    frame_header->frame_type = gst_av1_read_bits(br,2);
    frame_header->FrameIsIntra = (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME ||
                                  frame_header->frame_type == GST_AV1_KEY_FRAME);
    frame_header->show_frame = gst_av1_read_bit(br);
    if( frame_header->show_frame && seq_header->decoder_model_info_present_flag && !seq_header->timing_info.equal_picture_interval )
      frame_header->frame_presentation_time = gst_av1_read_bits(br,seq_header->decoder_model_info.frame_presentation_time_length_minus_1+1);

    if(frame_header->show_frame )
      frame_header->showable_frame = 0;
    else
      frame_header->showable_frame = gst_av1_read_bit(br);

    if(frame_header->frame_type == GST_AV1_SWITCH_FRAME || (frame_header->frame_type == GST_AV1_KEY_FRAME && frame_header->show_frame))
      frame_header->error_resilient_mode = 1;
    else
      frame_header->error_resilient_mode = gst_av1_reat_bit(br);
  }
  /* RefValid, RefOrderHint, OrderHints not available yet
  if ( frame_header->frame_type == KEY_FRAME && frame_header->show_frame ) {
    for ( int i = 0; i < GST_AV1_NUM_REF_FRAMES; i++ ) {
      RefValid[ i ] = 0
      RefOrderHint[ i ] = 0
    }
    for ( int i = 0; i < GST_AV1_REFS_PER_FRAME; i++ ) {
      OrderHints[ LAST_FRAME + i ] = 0
    }
  }
  */
  frame_header->disable_cdf_update = gst_av1_bit_read(br);

  if ( seq_header->seq_force_screen_content_tools == GST_AV1_SELECT_SCREEN_CONTENT_TOOLS ) {
    frame_header->allow_screen_content_tools = gst_av1_bit_read(br);
  } else {
    frame_header->allow_screen_content_tools = seq_header->seq_force_screen_content_tools;
  }

  if ( frame_header->allow_screen_content_tools ) {
    if ( seq_header->seq_force_integer_mv == GST_AV1_SELECT_INTEGER_MV ) {
      force_integer_mv = gst_av1_bit_read(br)
    } else {
      frame_header->force_integer_mv = seq_header->seq_force_integer_mv;
    }
  } else {
    frame_header->force_integer_mv = 0
  }

  if ( frame_header->FrameIsIntra ) {
    frame_header->force_integer_mv = 1
  }

  if( seq_header->frame_id_numbers_present_flag ) {
    //PrevFrameID = current_frame_id
    frame_header->current_frame_id = gst_av1_read_bits(br,idLen);
    mark_ref_frames(idLen); //TODO: Mark Ref Frames
  } else {
    frame_header->current_frame_id = 0;
  }
  if( frame_header->frame_type == GST_AV1_SWITCH_FRAME) {
    frame_header->frame_size_override_flag = 1;
  } else if ( seq_header->reduced_still_picture_header ) {
    frame_header->frame_size_override_flag = 0;
  } else {
    frame_header->frame_size_override_flag = gst_av1_read_bit(br);
  frame_header->order_hint = gst_av1_read_bits(br,seq_header->order_hint_bits_minus_1+1);
  //OrderHint = order_hint

  if ( frame_header->FrameIsIntra || frame_header->error_resilient_mode ) {
    frame_header->primary_ref_frame = GST_AV1_PRIMARY_REF_NONE;
  } else {
    frame_header->primary_ref_frame = gst_av1_read_bits(br,3);
  }

  if ( seq_header->decoder_model_info_present_flag ) {
    frame_header->buffer_removal_time_present_flag = gst_av1_read_bit(br);
    if ( frame_header->buffer_removal_time_present_flag ) {
      for ( int opNum = 0; opNum <= operating_points_cnt_minus_1; opNum++ ) {
        if ( seq_header->decoder_model_info.decoder_model_present_for_this_op[ opNum ] ) {
          int opPtIdc = seq_header->operating_points[ opNum ].idc;
          int inTemporalLayer = ( opPtIdc >> temporal_id ) & 1
          int inSpatialLayer = ( opPtIdc >> ( spatial_id + 8 ) ) & 1
          if ( opPtIdc == 0 || ( inTemporalLayer && inSpatialLayer ) )
            buffer_removal_time = gst_av1_read_bits(br,seq_header->decoder_model_info.buffer_removal_time_length_minus_1+1);
        }
      }
    }
  }

  frame_header->allow_high_precision_mv = 0;
  frame_header->use_ref_frame_mvs = 0;
  frame_header->allow_intrabc = 0;
  if ( frame_header->frame_type == GST_AV1_SWITCH_FRAME ||
     ( frame_header->frame_type == GST_AV1_KEY_FRAME && frame_header->show_frame ) ) {
    frame_header->refresh_frame_flags = allFrames;
  } else {
    frame_header->refresh_frame_flags = gst_av1_read_bits(br,8);
 }

  if ( !frame_header->FrameIsIntra || frame_header->refresh_frame_flags != allFrames ) {
    if ( frame_header->error_resilient_mode && seq_header->enable_order_hint ) {
      for ( int i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
       frame_header->ref_order_hint[i] = gst_av1_read_bits(br,seq_header->order_hint_bits_minus_1+1);
      // if ( ref_order_hint[ i ] != RefOrderHint[ i ] )
      //   RefValid[ i ] = 0
      }
    }
  }

  //here comes a concious simplification of the Spec 20180614 handeling KEY_FRAME and INTRA_ONLY_FRAME the same warranty
  //TODO: Recheck this section with recent Spec - before merging
  if( frame_header->frame_type == GST_AV1_KEY_FRAME || frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME ) {
    gst_av1_parse_frame_size(br, frame_header, seq_header);
    gst_av1_parse_render_size(br, frame_header);
    if( allow_screen_content_tools && frame_header->UpscaledWidth == frame_header->FrameWidth) {
      frame_header->allow_intrabc = gst_av1_read_bit(br);
    }
  } else {
    if( !seq_header->enable_order_hint) {
      frame_header->frame_refs_short_signaling = 0;
    } else {
      frame_header->frame_refs_short_signaling = gst_av1_read_bit(br);
      if(frame_header->frame_refs_short_signaling) {
        frame_header->last_frame_idx = gst_av1_read_bits(br,3);
        frame_header->gold_frame_idx = gst_av1_read_bits(br,3);
        //set_frame_refs()
      }
    }
    for ( i = 0; i < REFS_PER_FRAME; i++ ) {
      if ( !frame_header->frame_refs_short_signaling )
        frame_header->ref_frame_idx[i] = gst_av1_read_bits(br,3);

      if ( seq_header->frame_id_numbers_present_flag ) {
        frame_header->delta_frame_id_minus_1 = gst_av1_read_bits(br,delta_frame_id_length_minus_2 + 2)
        frame_header->expectedFrameId[i] = ((current_frame_id + (1 << idLen) - (delta_frame_id_minus_1 + 1) ) % (1 << idLen));
      }
    }

    if ( frame_header->frame_size_override_flag && !frame_header->error_resilient_mode ) {
      gst_av1_parse_frame_size_with_refs(br, frame_header, seq_header);
    } else {
      gst_av1_parse_frame_size(br, frame_header, seq_header);
      gst_av1_parse_render_size(br, frame_header);
    }
    if ( frame_header->force_integer_mv ) {
      frame_header->allow_high_precision_mv = 0;
    } else {
      allow_high_precision_mv = gst_av1_read_bit(br);
    }

    //inline read_interpolation_filter
    frame_header->is_filter_switchable = gst_av1_read_bit(br);
    if (frame_header->is_filter_switchable) {
      frame_header->interpolation_filter = GST_AV1_INTERPOLATION_FILTER_SWITCHABLE;
    } else {
      frame_header->interpolation_filter = gst_av1_read_bits(br,2);
    }

    frame_header->is_motion_mode_switchable = gst_av1_read_bit(br);
    if ( frame_header->error_resilient_mode || !frame_header->enable_ref_frame_mvs ) {
      frame_header->use_ref_frame_mvs = 0;
    } else {
      frame_header->use_ref_frame_mvs = gst_av1_read_bit(br);
    }
  }

  if( !frame_header->FrameIsIntra ) {
    for(int i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      int refFrame = GST_AV1_LAST_FRAME + 1;
      int hint = frame_header->RefOrderHint[frame_header->ref_frame_idx[i]];
      frame_header->OrderHints[refFrame] = hint;
      if( !frame_header->enable_order_hint) {
        frame_header->RefFrameSignBias[refFrame] = 0;
      } else {
        frame_header->RefFrameSignBias[refFrame] = (get_relative_dist(hint,OrderHint) > 0); //TODO: OrderHint present?
      }
    }
  }
  if ( seq_header->reduced_still_picture_header || frame_header->disable_cdf_update )
    frame_header->disable_frame_end_update_cdf = 1
  else
    frame_header->disable_frame_end_update_cdf = gst_av1_read_bit(br);

  //SPECs sets upt CDFs here - for codecparser we are ommitting this section.

  tile_info() //TODO: to be VA_STATUS_ERROR_UNIMPLEMENTED

}

GstAV1ParserResult gst_av1_parse_tile_list_obu( GstBitReader *br, GstAV1TileListOBU *tile_list)
{
  tile_list->output_frame_width_in_tiles_minus_1 = gst_av1_read_bits(br,8);
  tile_list->output_frame_height_in_tiles_minus_1 = gst_av1_read_bits(br,8);
  tile_list->tile_count_minus_1 = gst_av1_read_bits(br,16);
  for ( tile = 0; tile <= tile_count_minus_1; tile++ ) {
    tile_list->entry[tile].anchor_frame_idx = gst_av1_read_bits(br,8);
    tile_list->entry[tile].anchor_tile_row = gst_av1_read_bits(br,8);
    tile_list->entry[tile].anchor_tile_col = gst_av1_read_bits(br,8);
    tile_list->entry[tile].tile_data_size_minus_1 = gst_av1_read_bits(br,16);
    //skip ofer coded_tile_data
    gst_av1_bit_reader_skip(br,8*(tile_list->entry[tile].tile_data_size_minus_1+1)));
  }

  return GST_AV1_PARSE_OK;
}
