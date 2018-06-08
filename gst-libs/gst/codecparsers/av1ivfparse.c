#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

enum enum_obu_type {
  RESERVED_0 = 0,
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_FRAME = 6,
  OBU_REDUNDANT_FRAME_HEADER = 7,
  OBU_TILE_LIST = 8,
  RESERVED_9 = 9,
  RESERVED_10 = 10,
  RESERVED_11 = 11,
  RESERVED_12 = 12,
  RESERVED_13 = 13,
  RESERVED_14 = 14,
  OBU_PADDING = 15,
};

const char text_obu_type[16][50] = {
  "RESERVED_0",
  "OBU_SEQUENCE_HEADER",
  "OBU_TEMPORAL_DELIMITER",
  "OBU_FRAME_HEADER",
  "OBU_TILE_GROUP",
  "OBU_METADATA",
  "OBU_FRAME",
  "OBU_REDUNDANT_FRAME_HEADER",
  "OBU_TILE_LIST",
  "RESERVED_9",
  "RESERVED_10",
  "RESERVED_11",
  "RESERVED_12",
  "RESERVED_13",
  "RESERVED_14",
  "OBU_PADDING"
};

struct str_ivf_header {
  char signature[4];
  uint16_t version;
  uint16_t length;
  char fourcc[4];
  uint16_t width;
  uint16_t height;
  uint32_t frame_rate;
  uint32_t time_scale;
  uint32_t num_frames;
  uint32_t reserved;
} __attribute__((packed));

struct str_frame_header {
  uint32_t frame_size;
  uint64_t timestamp;
} __attribute__((packed));

struct str_obu_header {
  unsigned int  forbidden_bit : 1;
  unsigned int  type : 4;
  unsigned int  extention_flag : 1;
  unsigned int  has_size_field : 1;
  unsigned int  reserved_1bit : 1;
} __attribute__((packed));

struct str_obu_extention_header {
  unsigned int   temporal_id : 3;
  unsigned int   spatial_id : 2;
  unsigned int   extension_header_reserved_3bits : 3;
} __attribute__((packed));

#define SELECT_SCREEN_CONTENT_TOOLS 2
#define SELECT_INTEGER_MV 2

#define MAX_I 32
struct str_sequence_header_obu {
  unsigned int seq_profile : 3;
  unsigned int still_picture : 1;
  unsigned int reduced_still_picture_header : 1;
  unsigned int seq_level_idx[MAX_I]; // : 5;
  unsigned int operating_points_cnt_minus_1 : 1;
  unsigned int opterating_point_idc[MAX_I]; //: 12;
  unsigned int seq_tier[MAX_I]; // : 1;
  unsigned int frame_width_bits_minus_1 : 4;
  unsigned int frame_height_bits_minus_1 : 4;
  unsigned int max_frame_width_minus_1 : 16;
  unsigned int max_frame_height_minus_1 : 16;
  unsigned int frame_id_number_present_flag : 1;
  unsigned int delta_frame_id_length_minus_2 : 4;
  unsigned int additional_frame_id_length_minus_1 : 3;
  unsigned int use_128x128_superblock : 1;
  unsigned int enable_filter_intra : 1;
  unsigned int enable_intra_edge_filter : 1;
  unsigned int enable_interintra_compound : 1;
  unsigned int enable_masked_compound : 1;
  unsigned int enable_warped_motion : 1;
  unsigned int enable_dual_filter : 1;
  unsigned int enable_order_hint : 1;
  unsigned int enable_jnt_comp : 1;
  unsigned int enable_ref_frame_mvs : 1;
  unsigned int seq_choose_screen_content_tools : 2;
  unsigned int seq_force_screen_content_tools : 2;
  unsigned int seq_choose_integer_mv : 1;
  unsigned int seq_force_integer_mv : 1;
  unsigned int order_hint_bits_minus_1 : 3;
  unsigned int enable_superres : 1;
  unsigned int enable_cdef : 1;
  unsigned int enable_restoration : 1;
  unsigned int timing_info_present_flag : 1;
  unsigned int decoder_model_info_present_flag : 1;
  unsigned int operating_points_decoder_model_present : 1;
  unsigned int operating_points_decoder_model_count_minus_1 : 5;
  unsigned int decoder_model_operating_point_idc[MAX_I]; // : 12;
  unsigned int display_model_param_present_flag : 1;
  unsigned int initial_display_delay_minus_1 : 4;
  unsigned int decoder_model_param_present_flag : 1;
  unsigned int film_grain_params_present : 1;
};

struct str_seq_timing_info {
  uint32_t num_units_in_display_tick;
  uint32_t time_scale;
  bool equal_picture_interval;
  int num_ticks_per_picture_minus_1;
}

struct str_seq_decoder_model_info {
  int bitrate_scale;
  int buffer_size_scale;
  int encoder_decoder_buffer_delay_length_minus_1;
  int num_units_in_decoding_tick;
  int buffer_removal_delay_length_minus_1;
  int frame_presentation_delay_length_minus_1;
};

struct str_seq_operating_parameters_info {
  int bitrate_minus_1;
  int buffer_size_minus_1;
  bool cbr_flag;
  int decoder_buffer_delay;
  int encoder_buffer_delay;
  bool low_delay_mode_flag;
}

struct str_seq_color_config {
  bool high_bitdepth;
  bool twelve_bit;
  bool mono_chrome;
  bool color_description_present_flag;
  int color_primaries;
  int transfer_characteristics;
  int matrix_coefficients;
  bool color_range;
  bool subsampling_x;
  bool subsampling_y;
  int chroma_sample_position;
  bool separate_uv_delta_q;

}
bool read_bool(FILE *fp, bool pos_reset)
{
  static unsigned char cur_byte;
  static int bits_left = 0;
  bool cur_bit;

  if(!bits_left || pos_reset)
  {
    fread(&cur_byte, 1,1,fp);
    printf("--> byte fetched\n");
    bits_left = 8;
  }

  if(cur_byte & 0x80)
    cur_bit = 1;
  else
    cur_bit = 0;
  bits_left --;
  cur_byte <<= 1;

  return cur_bit;
}

int read_nliteral(FILE *fp, int nbits, bool pos_reset)
{
  int lit = 0;

  for(int i = 0; i < nbits; i++)
  {
    lit <<= 1;
    lit |= read_bool(fp,pos_reset);
    pos_reset = 0;
  }

  return lit;
}

int read_leb128(FILE *fp, int *value) {
 char leb128_byte;
 int byte_read = 0;

 *value = 0;

 for (int i = 0; i < 8; i++) {
     byte_read += fread(&leb128_byte,1,1,fp);
     *value |= ( ((int)leb128_byte & 0x7f) << (i*7) );
     if ( !(leb128_byte & 0x80) ) {
         break;
     }
 }
 return byte_read;
}

int read_uvlc(FILE *fp, int *value)
{
  int leadingZero = 0;
  int readv;
  bool done;
  int bitsread = 0;

  while(1) {
    done = read_bool(fp,0);
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
  readv = read_nliteral(fp,leadingZero,0);
  bitsread += leadingZero;
  value = readv + ( 1 << leadingZero) - 1;

  return bitsread;
}

#define CP_UNSPECIFIED 2
#define TC_UNSPECIFIED 2
#define MC_UNSPECIFIED 2

int read_color_config_obu(FILE *fp, struct str_seq_color_config *color_config, struct str_sequence_header_obu *seq_header)
{
  color_config->high_bitdepth = read_bool(fp,0);
  if (seq_header->seq_profile == 2 && color_config->high_bitdepth) {
    color_config->twelve_bit = read_bool(fp,0);
    //BitDepth = twelve_bit ? 12 : 10;
  }  else if (seq_header->seq_profile <= 2) {
    //BitDepth = high_bitdepth ? 10 : 8;
  }
  if (seq_header->seq_profile == 1) {
    color_config->mono_chrome = 0;
  } else {
    color_config->mono_chrome = read_bool(fp,0);
  }
  //NumPlanes = mono_chrome ? 1 : 3;
  color_config->color_description_present_flag = read_bool(fp,0);
  if(color_config->color_description_present_flag) {
    color_config->color_primaries = read_nliteral(fp,8,0);
    color_config->transfer_characteristics = read_nliteral(fp,8,0);
    color_config->matrix_coefficients = read_nliteral(fp,8,0);
  } else {
    color_config->color_primaries = CP_UNSPECIFIED;
    color_config->transfer_characteristics = TC_UNSPECIFIED;
    color_config->matrix_coefficients = MC_UNSPECIFIED;
  }
  if (color_config->mono_chrome) {
    color_config->color_range = read_bool(fp,0);
    color_config->subsampling_x = 1;
    color_config->subsampling_y = 1;
    color_config->chroma_sample_position = CSP_UNKNOWN;
    color_config->separate_uv_delta_q = 0;
    return;
  } else {
    color_config->color_range = read_bool(fp,0);
    if(seq_header->seq_profile == 0) {
      color_config->subsampling_x = 1;
      color_config->subsampling_y = 1;
    } else if (seq_header->seq_profile == 1) {
      color_config->subsampling_x = 1;
      color_config->subsampling_y = 1;
    } else {
      if(BitDepth == 12) {
        color_config->subsampling_x = read_bool(fp,0);
        if(color_config->subsampling_x)
          color_config->subsampling_y = read_bool(fp,0);
        else
          color_config->subsampling_y = 0;
      } else {
        color_config->subsampling_x = 1;
        color_config->subsampling_y = 0;

      }
      if(color_config->subsampling_x && color_config->subsampling_y) {
        color_config->chroma_sample_position = read_nliteral(fp,2,0);
      }
    }
  }
  color_config->separate_uv_delta_q = read_bool(fp,0);

  return;
}

int read_decoder_model_info_obu(FILE *fp, struct str_seq_decoder_model_info *dec_model_info)
{
  dec_model_info->bitrate_scale = read_nliteral(fp,4,0);
  dec_model_info->buffer_size_scale = read_nliteral(fp,4,0);
  dec_model_info->encoder_decoder_buffer_delay_length_minus_1 = read_nliteral(fp,5,0);
  dec_model_info->num_units_in_decoding_tick = read_nliteral(fp,32,0);
  dec_model_info->buffer_removal_delay_length_minus_1 = read_nliteral(fp,5,0);
  dec_model_info->frame_presentation_delay_length_minus_1 = read_nliteral(fp,5,0);
}

int read_timing_info_obu(FILE *fp, struct str_seq_timing_info *timing_info)
{
  timing_info->num_units_in_display_tick = read_nliteral(fp,32,0);
  timing_info->time_scale = read_nliteral(fp,32,0);
  timing_info->equal_picture_interval = read_bool(fp,0);
  if(timing_info->equal_picture_interval)
    read_uvlc(fp,&(timing_info->num_ticks_per_picture_minus_1));
}

int read_operating_parameter_info(FILE *fp, struct str_seq_operating_parameters_info *op_param_info,struct str_seq_decoder_model_info *dec_model_info)
{
  uvlc(FILE *fp,&(op_param_info->bitrate_minus_1));
  uvlc(FILE *fp,&(op_param_info->buffer_size_minus_1));
  op_param_info->cbr_flag = read_bool(fp,0):
  op_param_info->decoder_buffer_delay = read_nliteral(fp,op_param_info->encoder_decoder_buffer_delay_length_minus_1+1);
  op_param_info->encoder_buffer_delay = read_nliteral(fp,op_param_info->encoder_decoder_buffer_delay_length_minus_1+1);
  op_param_info->low_delay_mode_flag = read_boold(fp,0);
}

int read_sequence_header_obu(FILE *fp, struct str_sequence_header_obu *seq_header)
{
  int byte_read = 0;

  seq_header->seq_profile = read_nliteral(fp,3,0);
  seq_header->still_picture = read_bool(fp,0);
  seq_header->reduced_still_picture_header = read_bool(fp,0);

  if(seq_header->reduced_still_picture_header) {
    seq_header->operating_points_cnt_minus_1 = 0;
    seq_header->opterating_point_idc[0] = 0;
    seq_header->seq_level_idx[0] = read_nliteral(fp,5,0);
    seq_header->seq_tier[0] = 0;
  } else {
    seq_header->operating_points_cnt_minus_1 = read_nliteral(fp,5,0);
    for( int i = 0; i <= seq_header->operating_points_cnt_minus_1; i++) {
      seq_header->opterating_point_idc[i] = read_nliteral(fp,12,0);
      seq_header->seq_level_idx[i] = read_nliteral(fp,5,0);
      if (seq_header->seq_level_idx[i] > 7 ) {
        seq_header->seq_tier[i] = read_bool(fp,0);
      } else  {
        seq_header->seq_tier[i] = 0;
      }

    }
  }
  //operatingPoint = choose_operating_point( )
  //OperatingPointIdc = operating_point_idc[ operatingPoint ]

  seq_header->frame_width_bits_minus_1 = read_nliteral(fp,4,0);
  seq_header->frame_height_bits_minus_1 = read_nliteral(fp,4,0);
  seq_header->max_frame_width_minus_1 = read_nliteral(fp,seq_header->frame_width_bits_minus_1+1,0);
  seq_header->max_frame_height_minus_1 = read_nliteral(fp,seq_header->frame_height_bits_minus_1+1,0);

  if(seq_header->reduced_still_picture_header) {
    seq_header->frame_id_number_present_flag = 0;
  } else {
    seq_header->frame_id_number_present_flag = 1;
  }

  if(seq_header->frame_id_number_present_flag)
  {
    seq_header->delta_frame_id_length_minus_2 = read_nliteral(fp,4,0);
    seq_header->additional_frame_id_length_minus_1 = read_nliteral(fp,3,0);
  }

  seq_header->use_128x128_superblock = read_bool(fp,0);
  seq_header->enable_filter_intra = read_bool(fp,0);
  seq_header->enable_intra_edge_filter = read_bool(fp,0);

  if (seq_header->reduced_still_picture_header) {
    seq_header->enable_interintra_compound = 0;
    seq_header->enable_masked_compound = 0;
    seq_header->enable_warped_motion = 0;
    seq_header->enable_dual_filter = 0;
    seq_header->enable_order_hint = 0;
    seq_header->enable_jnt_comp = 0;
    seq_header->enable_ref_frame_mvs = 0;
    seq_header->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
    //OrderHintBits = 0
  } else {
    seq_header->enable_interintra_compound = read_bool(fp,0);
    seq_header->enable_masked_compound = read_bool(fp,0);
    seq_header->enable_warped_motion = read_bool(fp,0);
    seq_header->enable_dual_filter = read_bool(fp,0);
    seq_header->enable_order_hint = read_bool(fp,0);
    if(seq_header->enable_order_hint) {
      seq_header->enable_jnt_comp = read_bool(fp,0);
      seq_header->enable_ref_frame_mvs = read_bool(fp,0);
    } else {
      seq_header->enable_jnt_comp = 0;
      seq_header->enable_ref_frame_mvs = 0;
    }
    seq_header->seq_choose_screen_content_tools = read_bool(fp,0);
    if (seq_header->seq_choose_screen_content_tools) {
      seq_header->seq_force_screen_content_tools = SELECT_SCREEN_CONTENT_TOOLS;
    } else {
      seq_header->seq_force_screen_content_tools = read_bool(fp,0);
    }
    if ( seq_header->seq_force_screen_content_tools > 0 ) {
      seq_header->seq_choose_integer_mv = read_bool(fp,0);
      if (seq_header->seq_choose_integer_mv) {
        seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
      } else {
        seq_header->seq_force_integer_mv = read_bool(fp,0);
      }
    } else {
      seq_header->seq_force_integer_mv = SELECT_INTEGER_MV;
    }
    if ( seq_header->enable_order_hint ) {
      seq_header->order_hint_bits_minus_1 = read_nliteral(fp,3,0);
      //OrderHintBits = order_hint_bits_minus_1 + 1
    } else {
      //OrderHintBits = 0
    }
  }

  seq_header->enable_superres = read_bool(fp,0);
  seq_header->enable_cdef = read_bool(fp,0);
  seq_header->enable_restoration = read_bool(fp,0);
  //color_config()

  if(seq_header->reduced_still_picture_header) {
    seq_header->timing_info_present_flag = 0;
  } else {
    seq_header->timing_info_present_flag = read_bool(fp,0);
  }
  if(seq_header->timing_info_present_flag) {
    read_timing_info_obu(fp);
    seq_header->decoder_model_info_present_flag = read_bool(fp,0);e
    if(seq_header->decoder_model_info_present_flag) {
      //deocder_model_info()
    }
  } else {
    seq_header->decoder_model_info_present_flag = 0;
  }

  seq_header->operating_points_decoder_model_present = read_bool(fp,0);
  if(seq_header->operating_points_decoder_model_present) {
    seq_header->operating_points_decoder_model_count_minus_1 = read_nliteral(fp,5,0);
    for(int opNum = 0; opNum <= seq_header->operating_points_decoder_model_count_minus_1; opNum++) {
      seq_header->decoder_model_operating_point_idc[opNum] = read_nliteral(fp,12,0);
      seq_header->display_model_param_present_flag = read_bool(fp,0); //strange because named and overwritten for each iteration
      if(seq_header->display_model_param_present_flag) {
        seq_header->initial_display_delay_minus_1 = read_nliteral(fp,4,0);
      }
      if(seq_header->decoder_model_info_present_flag) {
        seq_header->decoder_model_param_present_flag = read_bool(fp,0);
        if (seq_header->decoder_model_param_present_flag) {
          //operating_parameters_info() // strange should this be an array??
        }
      }
    }
  } else {
    seq_header->operating_points_decoder_model_count_minus_1 = -1;
  }
  seq_header->film_grain_params_present = read_bool(fp,0);

}

int read_open_bitstream_unit(FILE *fp) {
  struct str_obu_header obu_header;
  struct str_obu_extention_header obu_extention_header;
  struct str_sequence_header_obu seq_header;

  int byte_read = 0;
  int obu_size;
  unsigned char byte;

  obu_header.forbidden_bit = read_bool(fp,1);
  obu_header.type = read_nliteral(fp,4,0);
  obu_header.extention_flag = read_bool(fp,0);
  obu_header.has_size_field = read_bool(fp,0);
  obu_header.reserved_1bit = read_bool(fp,0);
  byte_read++;

  printf("-------------------- OBU --------------------\n");
  printf("ForbiddenBit:\t%d\n",obu_header.forbidden_bit);
  printf("Type:\t\t%s\n",text_obu_type[obu_header.type]);
  printf("ExtentionFlag:\t%d\n",obu_header.extention_flag);
  printf("HasSizeField:\t%d\n",obu_header.has_size_field);

  if(obu_header.extention_flag)
  {
    obu_extention_header.temporal_id = read_nliteral(fp,3,1);
    obu_extention_header.temporal_id = read_nliteral(fp,2,0);
    byte_read++;

    printf("TemporalID:\t%d\n",obu_extention_header.temporal_id);
    printf("SpatialID:\t%d\n",obu_extention_header.spatial_id);
  }

  if(obu_header.has_size_field)
  {
    byte_read += read_leb128(fp,&obu_size);
    printf("Size:\t\t%d\n",obu_size);
    if(obu_size) {
      byte_read += obu_size;
    }
  }

  switch(obu_header.type) {
    case OBU_SEQUENCE_HEADER:
      read_sequence_header_obu(fp,&seq_header);
      break;
    default:
      if(obu_size)
        fseek(stdin,obu_size, SEEK_CUR);
      break;
  }


  return byte_read;
}


void print_ivf_header(struct str_ivf_header ivf_header)
{

  printf("---------------- IVF HEADER ----------------\n");
  printf("Signature: \t%4s\n",ivf_header.signature);
  printf("Version: \t%d\n",ivf_header.version);
  printf("Length: \t%d\n",ivf_header.length);
  printf("FourCC: \t%2X%2X%2X%2X\n",ivf_header.fourcc[0],ivf_header.fourcc[1],ivf_header.fourcc[2],ivf_header.fourcc[3]);
  printf("Width: \t\t%d\n",ivf_header.width);
  printf("Height: \t%d\n",ivf_header.height);
  printf("FrameRate: \t%d\n",ivf_header.frame_rate);
  printf("TimeScale: \t%d\n",ivf_header.time_scale);
  printf("NumFrames: \t%d\n",ivf_header.num_frames);
}

void print_ivf_frame_header(struct str_frame_header frame_header)
{
  printf("------------- IVF FRAME HEADER --------------\n");
  printf("FrameSize: \t%d\n",frame_header.frame_size);
  printf("TimeStamp: \t%ld\n",frame_header.timestamp);
}

void main()
{
  struct str_ivf_header ivf_header;
  struct str_frame_header frame_header;
  int obus_size;

  fread(&ivf_header, sizeof(struct str_ivf_header),1,stdin);
  print_ivf_header(ivf_header);

  while(fread(&frame_header,sizeof(struct str_frame_header),1,stdin) == 1)
  {
    print_ivf_frame_header(frame_header);
    obus_size = 0;
    while(frame_header.frame_size > obus_size)
    {
      obus_size += read_open_bitstream_unit(stdin);
      printf("obus_size: %d\n",obus_size);
    }
    //fseek(stdin,frame_header.frame_size-obus_size, SEEK_CUR);
  }
}
