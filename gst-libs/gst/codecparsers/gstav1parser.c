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
  decoder_model_info->encoder_decoder_buffer_delay_length_minus_1 = gst_av1_read_bits(br,5);
  decoder_model_info->num_units_in_decoding_tick = gst_av1_read_bits(br,32);
  decoder_model_info->buffer_removal_delay_length_minus_1 = gst_av1_read_bits(br,5);
  decoder_model_info->frame_presentation_delay_length_minus_1 = gst_av1_read_bits(br,5);

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
