/*
 * gstav1parser.h
 *
 *  Copyright (C) 2018 Georg Ottinger
 *    Author: Georg Ottinger<g.ottinger@gmx.at>
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * Description texts for syntax elements are taken from
 * the AV1 Bitstream & Decoding Process Specification
 */
 /**
  * SECTION:gstav1parser
  * @title: GstAV1Parser
  * @short_description: Convenience library for parsing AV1 video bitstream.
  *
  * For more details about the structures, you can refer to the
  * AV1 Bitstream & Decoding Process Specification V1.0.0
  * [specification](https://aomediacodec.github.io/av1-spec/av1-spec.pdf)
  *
  */


#ifndef GST_AV1_PARSER_H
#define GST_AV1_PARSER_H

#ifndef GST_USE_UNSTABLE_API
#warning "The AV1 parsing library is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <stdint.h>
#include <gst/gst.h>
#include <gst/codecparsers/codecparsers-prelude.h>

G_BEGIN_DECLS

#define GUINT32_MAX UINT32_MAX


/*TODO: Import all Defines from SPEC */

#define GST_AV1_MAX_OPERATING_POINTS 32
#define GST_AV1_MAX_TILE_COUNT 512
#define GST_AV1_MAX_TILE_WIDTH 4096
#define GST_AV1_MAX_TILE_AREA (4096*2304)
#define GST_AV1_MAX_TILE_ROWS 64
#define GST_AV1_MAX_TILE_COLS 64
#define GST_AV1_MAX_SPATIAL_LAYERS 2 /*is this correct?? */
#define GST_AV1_MAX_TEMPORAL_GROUP_SIZE 8 /*is this correct?? */
#define GST_AV1_MAX_TEMPORAL_GROUP_REFERENCES 8 /*is this correct??*/
#define GST_AV1_TOTAL_REFS_PER_FRAME 8
#define GST_AV1_MAX_SEGMENTS 8
#define GST_AV1_SEG_LVL_MAX 8
#define GST_AV1_MAX_TILE_COLS 64
#define GST_AV1_MAX_TILE_ROWS 64
#define GST_AV1_CDEF_MAX (1<<3)
#define GST_AV1_MAX_NUM_Y_POINTS 16
#define GST_AV1_MAX_NUM_CB_POINTS 16
#define GST_AV1_MAX_NUM_CR_POINTS 16
#define GST_AV1_MAX_NUM_POS_LUMA 25
#define GST_AV1_NUM_REF_FRAMES 8
#define GST_AV1_REFS_PER_FRAME 7
#define GST_AV1_PRIMARY_REF_NONE 7
#define GST_AV1_SUPERRES_NUM 8
#define GST_AV1_SUPERRES_DENOM_MIN 9
#define GST_AV1_SUPERRES_DENOM_BITS 3
#define GST_AV1_MAX_LOOP_FILTER 63
#define GST_AV1_MAX_NUM_PLANES 3
#define GST_AV1_GM_ABS_ALPHA_BITS 12
#define GST_AV1_GM_ABS_TRANS_BITS 12
#define GST_AV1_GM_ABS_TRANS_ONLY_BITS 9
#define GST_AV1_GM_ABS_ALPHA_BITS 12
#define GST_AV1_GM_ALPHA_PREC_BITS 15
#define GST_AV1_GM_TRANS_PREC_BITS 6
#define GST_AV1_GM_TRANS_ONLY_PREC_BITS 3
#define GST_AV1_WARPEDMODEL_PREC_BITS 16
#define GST_AV1_SELECT_SCREEN_CONTENT_TOOLS 2
#define GST_AV1_SELECT_INTEGER_MV 2
#define GST_AV1_RESTORATION_TILESIZE_MAX 256
#define GST_AV1_SEG_LVL_REF_FRAME 5

typedef struct _GstAV1Parser GstAV1Parser;

typedef struct _GstAV1OBUHeader GstAV1OBUHeader;

typedef struct _GstAV1SequenceHeaderOBU GstAV1SequenceHeaderOBU;
typedef struct _GstAV1MetadataOBU GstAV1MetadataOBU;
typedef struct _GstAV1FrameHeaderOBU GstAV1FrameHeaderOBU;
typedef struct _GstAV1TileListOBU GstAV1TileListOBU;
typedef struct _GstAV1TileGroupOBU GstAV1TileGroupOBU;
typedef struct _GstAV1FrameOBU GstAV1FrameOBU;

typedef struct _GstAV1OperatingPoint GstAV1OperatingPoint;
typedef struct _GstAV1DecoderModelInfo GstAV1DecoderModelInfo;
typedef struct _GstAV1TimingInfo GstAV1TimingInfo;
typedef struct _GstAV1ColorConfig GstAV1ColorConfig;
typedef struct _GstAV1MetadataITUT_T35 GstAV1MetadataITUT_T35;
typedef struct _GstAV1MetadataHdrCll GstAV1MetadataHdrCll;
typedef struct _GstAV1MetadataHdrMdcv GstAV1MetadataHdrMdcv;
typedef struct _GstAV1MetadataScalability GstAV1MetadataScalability;
typedef struct _GstAV1MetadataTimecode GstAV1MetadataTimecode;
typedef struct _GstAV1LoopFilterParams GstAV1LoopFilterParams;
typedef struct _GstAV1QuantizationParams GstAV1QuantizationParams;
typedef struct _GstAV1SegmenationParams GstAV1SegmenationParams;
typedef struct _GstAV1TileInfo GstAV1TileInfo;
typedef struct _GstAV1CDEFParams GstAV1CDEFParams;
typedef struct _GstAV1LoopRestorationParams GstAV1LoopRestorationParams;
typedef struct _GstAV1GlobalMotionParams GstAV1GlobalMotionParams;
typedef struct _GstAV1FilmGrainParams GstAV1FilmGrainParams;

typedef struct _GstAV1ReferenceFrameInfo GstAV1ReferenceFrameInfo;

/**
 * GstAV1ParserResult:
 *
 */

typedef enum {
  GST_AV1_PARSER_OK = 0,
  GST_AV1_PARSER_ERROR = 1,
  GST_AV1_PARSER_READBITS_ERROR = 2,
  GST_AV1_PARSER_SKIPBITS_ERROR = 3,
  GST_AV1_PARSER_BITSTREAM_ERROR = 4,
  GST_AV1_PARSER_MISSING_OBU_REFERENCE = 5,
} GstAV1ParserResult;

/**
 * GstAV1OBUType:
 *
 */

typedef enum {
  GST_AV1_OBU_RESERVED_0 = 0,
  GST_AV1_OBU_SEQUENCE_HEADER = 1,
  GST_AV1_OBU_TEMPORAL_DELIMITER = 2,
  GST_AV1_OBU_FRAME_HEADER = 3,
  GST_AV1_OBU_TILE_GROUP = 4,
  GST_AV1_OBU_METADATA = 5,
  GST_AV1_OBU_FRAME = 6,
  GST_AV1_OBU_REDUNDANT_FRAME_HEADER = 7,
  GST_AV1_OBU_TILE_LIST = 8,
  GST_AV1_OBU_RESERVED_9 = 9,
  GST_AV1_OBU_RESERVED_10 = 10,
  GST_AV1_OBU_RESERVED_11 = 11,
  GST_AV1_OBU_RESERVED_12 = 12,
  GST_AV1_OBU_RESERVED_13 = 13,
  GST_AV1_OBU_RESERVED_14 = 14,
  GST_AV1_OBU_PADDING = 15,
} GstAV1OBUType;

/**
 * GstAV1MetadataType:
 *
 */

typedef enum {
  GST_AV1_METADATA_TYPE_RESERVED_0 = 0,
  GST_AV1_METADATA_TYPE_HDR_CLL = 1,
  GST_AV1_METADATA_TYPE_HDR_MDCV = 2,
  GST_AV1_METADATA_TYPE_SCALABILITY = 3,
  GST_AV1_METADATA_TYPE_ITUT_T35 = 4,
  GST_AV1_METADATA_TYPE_TIMECODE = 5,
} GstAV1MetadataType;

/**
 * GstAV1ScalabilityModes:
 *
 */
typedef enum {
  GST_AV1_SCALABILITY_L1T2 = 0,
  GST_AV1_SCALABILITY_L1T3 = 1,
  GST_AV1_SCALABILITY_L2T1 = 2,
  GST_AV1_SCALABILITY_L2T2 = 3,
  GST_AV1_SCALABILITY_L2T3 = 4,
  GST_AV1_SCALABILITY_S2T1 = 5,
  GST_AV1_SCALABILITY_S2T2 = 6,
  GST_AV1_SCALABILITY_S2T3 = 7,
  GST_AV1_SCALABILITY_L2T1h = 8,
  GST_AV1_SCALABILITY_L2T2h = 9,
  GST_AV1_SCALABILITY_L2T3h =10,
  GST_AV1_SCALABILITY_S2T1h = 11,
  GST_AV1_SCALABILITY_S2T2h = 12,
  GST_AV1_SCALABILITY_S2T3h = 13,
  GST_AV1_SCALABILITY_SS = 14,
} GstAV1ScalabilityModes;

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
 * GstAV1ChromaSamplePositions:
 * GST_AV1_CSP_UNKNOWN: Unknown (in this case the source video transfer function must be
 *                      signaled outside the AV1 bitstream).
 * GST_AV1_CSP_VERTICAL: Horizontally co-located with (0, 0) luma sample, vertical position in
 *                       the middle between two luma samples.
 * GST_AV1_CSP_COLOCATED: co-located with (0, 0) luma sample.
 * GST_AV1_CSP_RESERVED: For future use.
 *
 */

typedef enum {
  GST_AV1_CSP_UNKNOWN = 0,
  GST_AV1_CSP_VERTICAL = 1,
  GST_AV1_CSP_COLOCATED = 2,
  GST_AV1_CSP_RESERVED = 3,
} GstAV1ChromaSamplePositions;

/**
 * GstAV1FrameType:
 *
 */
typedef enum {
  GST_AV1_KEY_FRAME = 0,
  GST_AV1_INTER_FRAME = 1,
  GST_AV1_INTRA_ONLY_FRAME = 2,
  GST_AV1_SWITCH_FRAME = 3,
} GstAV1FrameType;

/**
 * GstAV1InterpolationFilter:
 *
 */
typedef enum {
  GST_AV1_INTERPOLATION_FILTER_EIGHTTAP = 0,
  GST_AV1_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH = 1,
  GST_AV1_INTERPOLATION_FILTER_EIGHTTAP_SHARP = 2,
  GST_AV1_INTERPOLATION_FILTER_BILINEAR = 3,
  GST_AV1_INTERPOLATION_FILTER_SWITCHABLE = 4,
} GstAV1InterpolationFilter;

/**
 * GstAV1TXModes:
 *
 */
typedef enum {
  GST_AV1_TX_MODE_ONLY_4x4 = 0,
  GST_AV1_TX_MODE_LARGEST = 1,
  GST_AV1_TX_MODE_SELECT = 2,
} GstAV1TXModes;


/**
 * GstAV1FrameRestorationType:
 *
 */
typedef enum {
  GST_AV1_FRAME_RESTORE_NONE = 0,
  GST_AV1_FRAME_RESTORE_WIENER = 1,
  GST_AV1_FRAME_RESTORE_SGRPROJ = 2,
  GST_AV1_FRAME_RESTORE_SWITCHABLE = 3,
} GstAV1FrameRestorationType;


/**
 * GstAV1ReferenceFrame:
 *
 */
 /* TODO: Reference Numbers need to checked */

typedef enum {
  GST_AV1_REF_INTRA_FRAME = 0,
  GST_AV1_REF_LAST_FRAME = 1,
  GST_AV1_REF_LAST2_FRAME = 2,
  GST_AV1_REF_LAST3_FRAME = 3,
  GST_AV1_REF_BWDREF_FRAME = 4,
  GST_AV1_REF_GOLDEN_FRAME = 5,
  GST_AV1_REF_ALTREF_FRAME = 6,
  GST_AV1_REF_ALTREF2_FRAME = 7,
} GstAV1ReferenceFrame;

/**
 * GstAV1WarpModelType:
 *
 * GST_AV1_WARP_MODEL_IDENTITY: Warp model is just an identity transform
 * GST_AV1_WARP_MODEL_TRANSLATION: Warp model is a pure translation
 * GST_AV1_WARP_MODEL_ROTZOOM: Warp model is a rotation + symmetric zoom + translation
 * GST_AV1_WARP_MODEL_AFFINE: Warp model is a general affine transform
 */
typedef enum {
  GST_AV1_WARP_MODEL_IDENTITY = 0,
  GST_AV1_WARP_MODEL_TRANSLATION = 1,
  GST_AV1_WARP_MODEL_ROTZOOM = 2,
  GST_AV1_WARP_MODEL_AFFINE = 3,
} GstAV1WarpModelType;


/**
 * _GstAV1OBUHeader:
 *
 * @obu_forbidden_bit: must be set to 0.
 * @obu_type specifies: the type of data structure contained in the OBU payload.
 * @obu_has_size_field: equal to 1 indicates that the obu_size syntax element will be present.
 *                      obu_has_size_field equal to 0 indicates that the obu_size syntax element will not be present.
 * @obu_reserved_1bit: must be set to 0. The value is ignored by a decoder.
 * @obu_size: contains the size in bytes of the OBU not including the bytes within obu_header
 *            or the obu_size syntax element.
 * @temporal_id: specifies the temporal level of the data contained in the OBU.
 * @spatial_id: specifies the spatial level of the data contained in the OBU.
 * @extension_header_reserved_3bits must be set to 0. The value is ignored by a decoder.
 *
 */

struct _GstAV1OBUHeader {
  GstAV1OBUType obu_type;
  guint8 obu_extention_flag;
  guint8 obu_has_size_field;
  guint8 obu_reserved_1bit;
  guint32 obu_size;
  guint8 obu_temporal_id;
  guint8 obu_spatial_id;
  guint8 obu_extension_header_reserved_3bits;
};


/**
 * _GstAV1OperatingPoint:
 *
 * @seq_level_idx: specifies the level that the coded video sequence conforms to.
 * @seq_tier: specifies the tier that the coded video sequence conforms to.
 * @idc: contains a bitmask that indicates which spatial and temporal layers should be decoded
 *       Bit k is equal to 1 if temporal layer k should be decoded (for k between 0 and 7).
 *       Bit j+8 is equal to 1 if spatial layer j should be decoded (for j between 0 and 3).
 *
 * @decoder_model_present_for_this_op: equal to one indicates that there is a decoder model associated
 *                                     with this operating point.decoder_model_present_for_this_op equal
 *                                     to zero indicates that there is not a decoder model associated.
 * @decoder_buffer_delay: specifies the time interval between the arrival of the first bit in the smoothing
 *                        buffer and the subsequent removal of the data that belongs to the first coded
 *                        frame for operating point op, measured in units of 1/90000 seconds. The length of
 *                        decoder_buffer_delay is specified by buffer_delay_length_minus_1 + 1, in bits.
 * @encoder_buffer_delay: specifies, in combination with decoder_buffer_delay syntax element, the first bit
 *                        arrival time of frames to be decoded to the smoothing buffer. encoder_buffer_delay
 *                        is measured in units of 1/90000 seconds. For a video sequence that includes one or
 *                        more random access points the sum of decoder_buffer_delay and encoder_buffer_delay
 *                        shall be kept constant.
 * @low_delay_mode_flag: equal to 1 indicates that the smoothing buffer operates in low-delay mode for operating
 *                       point op. In low-delay mode late decode times and buffer underflow are both permitted.
 *                       low_delay_mode_flag equal to 0 indicates that the smoothing buffer operates in strict
 *                       mode, where buffer underflow is not allowed.
 * @initial_display_delay_present_for_this_op: equal to 1 indicates that initial_display_delay_minus_1
 *                                             is specified for this operating. 0 indicates that
 *                                             initial_display_delay_minus_1 is not specified for this
 *                                             operating point.
 * initial_display_delay_minus_1: plus 1 specifies, for operating point i, the number of decoded frames
 *                                that should be present in the buffer pool before the first presentable
 *                                frame is displayed. This will ensure that all presentable frames in the
 *                                sequence can be decoded at or before the time that they are scheduled
 *                                for display.
 */

struct _GstAV1OperatingPoint {
  guint8 seq_level_idx;
  guint8 seq_tier;
  guint16 idc;
  gboolean decoder_model_present_for_this_op;
  guint8 decoder_buffer_delay;
  guint8 encoder_buffer_delay;
  gboolean low_delay_mode_flag;
  gboolean initial_display_delay_present_for_this_op;
  guint8 initial_display_delay_minus_1;
};


/**
 * _GstAV1DecoderModelInfo:
 * @bitrate_scale, buffer_size_scale, encoder_decoder_buffer_delay_length_minus_1, num_units_in_decoding_tick,
 *  buffer_removal_delay_length_minus_1, and frame_presentation_delay_length_minus_1: These are syntax elements used
 *  by the decoder model. They do not affect the decoding process.
 */

struct _GstAV1DecoderModelInfo {
  guint8 buffer_delay_length_minus_1;
  guint32 num_units_in_decoding_tick;
  guint8 buffer_removal_time_length_minus_1;
  guint8 frame_presentation_time_length_minus_1;
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
 * _GstAV1ColorConfig:
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
 * @BitDepth: BitDepth
 * @NumPlanes: Number of planes
 */

struct _GstAV1ColorConfig {
  guint8 high_bitdepth;
  guint8 twelve_bit;
  gboolean mono_chrome;
  gboolean color_description_present_flag;
  GstAV1ColorPrimaries color_primaries;
  GstAV1TransferCharacteristics transfer_characteristics;
  GstAV1MatrixCoefficients matrix_coefficients;
  guint8 color_range;
  guint8 subsampling_x;
  guint8 subsampling_y;
  GstAV1ChromaSamplePositions chroma_sample_position;
  gboolean separate_uv_delta_q;
  guint8 BitDepth;
  guint8 NumPlanes;
};

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
 * @initial_display_delay_present_flag: specifies whether initial display delay information is present in the bitstream or not.
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
  gboolean still_picture;
  gboolean reduced_still_picture_header;


  guint8 frame_width_bits_minus_1;
  guint8 frame_height_bits_minus_1;
  guint16 max_frame_width_minus_1;
  guint16 max_frame_height_minus_1;
  guint8 frame_id_numbers_present_flag;
  guint8 delta_frame_id_length_minus_2;
  guint8 additional_frame_id_length_minus_1;
  guint8 use_128x128_superblock;
  gboolean enable_filter_intra;
  gboolean enable_intra_edge_filter;
  gboolean enable_interintra_compound;
  gboolean enable_masked_compound;
  gboolean enable_warped_motion;
  gboolean enable_dual_filter;
  gboolean enable_order_hint;
  gboolean enable_jnt_comp;
  gboolean enable_ref_frame_mvs;
  guint8 seq_choose_screen_content_tools;
  guint8 seq_force_screen_content_tools;
  guint8 seq_choose_integer_mv;
  guint8 seq_force_integer_mv;
  guint8 order_hint_bits_minus_1;

  gboolean enable_superres;
  gboolean enable_cdef;
  gboolean enable_restoration;

  gboolean film_grain_params_present;


  guint8 operating_points_cnt_minus_1;
  GstAV1OperatingPoint operating_points[GST_AV1_MAX_OPERATING_POINTS];

  gboolean decoder_model_info_present_flag;
  GstAV1DecoderModelInfo decoder_model_info;
  gboolean initial_display_delay_present_flag;

  gboolean timing_info_present_flag;
  GstAV1TimingInfo timing_info;

  GstAV1ColorConfig color_config;
};

/**
 * _GstAV1MetadataITUT_T35:
 *
 * @itu_t_t35_country_code: shall be a byte having a value specified as a country code by Annex A of Recommendation ITU-T T.35.
 * @itu_t_t35_country_code_extension_byte: shall be a byte having a value specified as a country code by Annex B of
 *                                         Recommendation ITU-T T.35.
 * @itu_t_t35_payload_bytes: shall be bytes containing data registered as specified in Recommendation ITU-T T.35.
 */
struct _GstAV1MetadataITUT_T35 {
  guint8 itu_t_t35_country_code;
  guint8 itu_t_t35_country_code_extention_byte;
  /* itu_t_t35_payload_bytes - not supported at the moment */
};

/**
 * _GstAV1MetadataHdrCll:
 * high dynamic range content light level syntax metadata
 *
 * @max_cll: specifies the maximum content light level as specified in CEA-861.3, Appendix A.
 * @max_fall: specifies the maximum frame-average light level as specified in CEA-861.3, Appendix A.
 *
 */

struct _GstAV1MetadataHdrCll {
  guint16 max_cll;
  guint16 max_fall;
};

/**
 * _GstAV1MetadataHdrCll:
 * high dynamic range mastering display color volume metadata
 *
 * @primary_chromaticity_x[]: specifies a 0.16 fixed-point X chromaticity coordinate as defined by CIE 1931, where i =
 *                            0,1,2 specifies Red, Green, Blue respectively.
 * @primary_chromaticity_y[]: specifies a 0.16 fixed-point Y chromaticity coordinate as defined by CIE 1931, where i =
 *                            0,1,2 specifies Red, Green, Blue respectively.
 * @white_point_chromaticity_x: specifies a 0.16 fixed-point white X chromaticity coordinate as defined by CIE 1931.
 * @white_point_chromaticity_y: specifies a 0.16 fixed-point white Y chromaticity coordinate as defined by CIE 1931.
 * @luminance_max: is a 24.8 fixed-point maximum luminance, represented in candelas per square meter.
 * @luminance_min: is a 18.14 fixed-point minimum luminance, represented in candelas per square meter.
 */

struct _GstAV1MetadataHdrMdcv {
  guint16 primary_chromaticity_x[3];
  guint16 primary_chromaticity_y[3];
  guint16 white_point_chromaticity_x;
  guint16 white_point_chromaticity_y;
  guint32 luminance_max;
  guint32 luminance_min;
};

/**
 * _GstAV1MetadataScalability:
 * high dynamic range mastering display color volume metadata
 *
 * @scalability_mode_idc: indicates the picture prediction structure of the bitstream.
 * @spatial_layers_cnt_minus_1: indicates the number of spatial layers present in the video sequence minus one.
 * @spatial_layer_description_present_flag: indicates when set to 1 that the spatial_layer_ref_id is present for each of the
 *                                          (spatial_layers_cnt_minus_1 + 1) layers, or that it is not present when set to 0.
 * @spatial_layer_dimensions_present_flag: indicates when set to 1 that the spatial_layer_max_width and
 *                                         spatial_layer_max_height parameters are present for each of the
 *                                         (spatial_layers_cnt_minus_1 + 1) layers, or that it they are not present when set to 0.
 * @temporal_group_description_present_flag: indicates when set to 1 that the temporal dependency information is
 *                                           present, or that it is not when set to 0.
 *
 * @scalability_structure_reserved_3bits: must be set to zero and be ignored by decoders.
 * @spatial_layer_max_width[i]: specifies the maximum frame width for the frames with spatial_id equal to i. This number
 *                              must not be larger than max_frame_width_minus_1 + 1.
 * @spatial_layer_max_height[i]: specifies the maximum frame height for the frames with spatial_id equal to i. This number
 *                               must not be larger than max_frame_height_minus_1 + 1.
 * @spatial_layer_ref_id[i]: specifies the spatial_id value of the frame within the current temporal unit that the frame of layer i
 *                           uses for reference. If no frame within the current temporal unit is used for reference the value must
 *                           be equal to 255.
 * @temporal_group_size: indicates the number of pictures in a temporal picture group. If the temporal_group_size is greater
 *                       than 0, then the scalability structure data allows the inter-picture temporal dependency structure of
 *                       the video sequence to be specified. If the temporal_group_size is greater than 0, then for temporal_group_size
 *                       pictures in the temporal group, each picture’s temporal layer id (temporal_id), switch up points
 *                       (temporal_group_temporal_switching_up_point_flag and temporal_group_spatial_switching_up_point_flag), and the
 *                       reference picture indices (temporal_group_ref_pic_diff) are specified.
 *                       The first picture specified in a temporal group must have temporal_id equal to 0.
 *                       If the parameter temporal_group_size is not present or set to 0, then either there is only one temporal layer
 *                       or there is no fixed inter-picture temporal dependency present going forward in the video sequence.
 *                       Note that for a given picture, all frames follow the same inter-picture temporal dependency structure.
 *                       However, the frame rate of each layer can be different from each other. The specified dependency structure
 *                       in the scalability structure data must be for the highest frame rate layer.
 * @temporal_group_temporal_id[i]: specifies the temporal_id value for the i-th picture in the temporal group.
 * @temporal_group_temporal_switching_up_point_flag[i]: is set to 1 if subsequent (in decoding order) pictures with a temporal_id higher
 *                                                      than temporal_group_temporal_id[ i ] do not depend on any picture preceding the
 *                                                      current picture (in coding order) with temporal_id higher than
 *                                                      temporal_group_temporal_id[ i ].
 * @temporal_group_spatial_switching_up_point_flag[i]: is set to 1 if spatial layers of the current picture in the temporal group
 *                                                     (i.e., pictures with a spatial_id higher than zero) do not depend on any picture
 *                                                     preceding the current picture in the temporal group.
 * @temporal_group_ref_cnt[i]: indicates the number of reference pictures used by the i-th picture in the temporal group.
 * @temporal_group_ref_pic_diff[i][j]: indicates, for the i-th picture in the temporal group, the temporal distance between the i-th picture
 *                                     and the j-th reference picture used by the i-th picture. The temporal distance is measured in frames,
 *                                     counting only frames of identical spatial_id values.
 */



struct _GstAV1MetadataScalability {
  GstAV1ScalabilityModes scalability_mode_idc;
  guint8 spatial_layers_cnt_minus_1;
  guint8 spatial_layer_dimensions_present_flag;
  guint8 spatial_layer_description_present_flag;
  guint8 temporal_group_description_present_flag;
  guint8 scalability_structure_reserved_3bits;
  guint16 spatial_layer_max_width[GST_AV1_MAX_SPATIAL_LAYERS];
  guint16 spatial_layer_max_height[GST_AV1_MAX_SPATIAL_LAYERS];
  guint8 spatial_layer_ref_id[GST_AV1_MAX_SPATIAL_LAYERS];
  guint8 temporal_group_size;

  guint8 temporal_group_temporal_id[GST_AV1_MAX_TEMPORAL_GROUP_SIZE];
  guint8 temporal_group_temporal_switching_up_point_flag[GST_AV1_MAX_TEMPORAL_GROUP_SIZE];
  guint8 temporal_group_spatial_switching_up_point_flag[GST_AV1_MAX_TEMPORAL_GROUP_SIZE];
  guint8 temporal_group_ref_cnt[GST_AV1_MAX_TEMPORAL_GROUP_SIZE];
  guint8 temporal_group_ref_pic_diff[GST_AV1_MAX_TEMPORAL_GROUP_SIZE][GST_AV1_MAX_TEMPORAL_GROUP_REFERENCES];
};


/**
 * _GstAV1MetadataTimecode:
 * high dynamic range mastering display color volume metadata
 *
 * @counting_type: specifies the method of dropping values of the n_frames syntax element as specified in AV1 Spec 6.1.1.
 *                 counting_type should be the same for all pictures in the coded video sequence.
 * @full_timestamp_flag: equal to 1 indicates that the the seconds_value, minutes_value, hours_value syntax elements will be
 *                       present. full_timestamp_flag equal to 0 indicates that there are flags to control the presence of
 *                       these syntax elements.
 * @discontinuity_flag: equal to 0 indicates that the difference between the current value of clockTimestamp and the value of
 *                      clockTimestamp computed from the previous set of timestamp syntax elements in output order can be
 *                      interpreted as the time difference between the times of origin or capture of the associated frames or
 *                      fields. discontinuity_flag equal to 1 indicates that the difference between the current value of
 *                      clockTimestamp and the value of clockTimestamp computed from the previous set of clock timestamp syntax
 *                      elements in output order should not be interpreted as the time difference between the times of origin
 *                      or capture of the associated frames or fields.
 * @cnt_dropped_flag: specifies the skipping of one or more values of n_frames using the counting method specified by
 *                    counting_type.
 * @n_frames: is used to compute clockTimestamp. When timing_info_present_flag is equal to 1, n_frames shall be less than
 *            maxFps, where maxFps is specified by maxFps = ceil( time_scale / ( 2 * num_units_in_display_tick ) ).
 * @seconds_flag equal: to 1 specifies that seconds_value and minutes_flag are present when full_timestamp_flag is equal to
 *                      0. seconds_flag equal to 0 specifies that seconds_value and minutes_flag are not present.
 * @seconds_value: is used to compute clockTimestamp and shall be in the range of 0 to 59. When seconds_value is not
 *                 present, its value is inferred to be equal to the value of seconds_value for the previous set of clock
 *                 timestamp syntax elements in decoding order, and it is required that such a previous seconds_value shall
 *                 have been present.
 * @minutes_flag equal: to 1 specifies that minutes_value and hours_flag are present when full_timestamp_flag is equal to 0
 *                      and seconds_flag is equal to 1. minutes_flag equal to 0 specifies that minutes_value and hours_flag
 *                      are not present.
 * @minutes_value specifies: the value of mm used to compute clockTimestamp and shall be in the range of 0 to 59, inclusive.
 *                           When minutes_value is not present, its value is inferred to be equal to the value of minutes_value
 *                           for the previous set of clock timestamp syntax elements in decoding order, and it is required that
 *                           such a previous minutes_value shall have been present.
 * @hours_flag: equal to 1 specifies that hours_value is present when full_timestamp_flag is equal to 0 and seconds_flag is
 *              equal to 1 and minutes_flag is equal to 1.
 * @hours_value: is used to compute clockTimestamp and shall be in the range of 0 to 23, inclusive. When hours_value is not
 *               present, its value is inferred to be equal to the value of hours_value for the previous set of clock timestamp
 *               syntax elements in decoding order, and it is required that such a previous hours_value shall have been present.
 * @time_offset_length: greater than 0 specifies the length in bits of the time_offset_value syntax element. time_offset_length
 *                      equal to 0 specifies that the time_offset_value syntax element is not present. time_offset_length should
 *                      be the same for all pictures in the coded video sequence.
 * @time_offset_value: is used to compute clockTimestamp. The number of bits used to represent time_offset_value is equal to
 *                     time_offset_length. When time_offset_value is not present, its value is inferred to be equal to 0.
 *
 */

struct _GstAV1MetadataTimecode {
  guint8 counting_type; /* candidate for sperate Type GstAV1TimecodeCountingType */
  gboolean full_timestamp_flag;
  gboolean discontinuity_flag;
  gboolean cnt_dropped_flag;
  guint8 n_frames;
  gboolean seconds_flag;
  guint8 seconds_value;
  gboolean minutes_flag;
  guint8 minutes_value;
  gboolean hours_flag;
  guint8 hours_value;
  guint8 time_offset_length;
  guint32 time_offset_value;
};

/**
 * _GstAV1MetadataOBU:
 *
 * @metadata_type: type of metadata
 * @itut_t35: ITUT T35 metadata
 * @hdrcll: high dynamic range content light level metadata
 * @hdrcmdcv: high dynamic range mastering display color volume metadata_type
 * @scalability: Scalability metadata
 * @timecode: Timecode metadata
 */

struct _GstAV1MetadataOBU {
  GstAV1MetadataType metadata_type;
  union {
    GstAV1MetadataITUT_T35 itut_t35;
    GstAV1MetadataHdrCll hdr_cll;
    GstAV1MetadataHdrMdcv hdr_mdcv;
    GstAV1MetadataScalability scalability;
    GstAV1MetadataTimecode timecode;
  };
};

/**
 * _GstAV1LoopFilterParams:
 *
 * @loop_filter_level[]: is an array containing loop filter strength values. Different loop filter strength values
 *                       from the array are used depending on the image plane being filtered, and the edge direction
 *                       (vertical or horizontal) being filtered.
 * @loop_filter_sharpness: indicates the sharpness level. The loop_filter_level and loop_filter_sharpness together
 *                         determine when a block edge is filtered, and by how much the filtering can change the
 *                         sample values. The loop filter process is described in AV1 Bitstream Spec. section 7.14.
 * @loop_filter_delta_enabled: equal to 1 means that the filter level depends on the mode and reference frame used to
 *                             predict a block. loop_filter_delta_enabled equal to 0 means that the filter level does
 *                             not depend on the mode and reference frame.
 * @loop_filter_delta_update: equal to 1 means that the bitstream contains additional syntax elements that specify which
 *                            mode and reference frame deltas are to be updated. loop_filter_delta_update equal to 0
 *                            means that these syntax elements are not present
 * @update_ref_delta[]: equal to 1 means that the bitstream contains the syntax element loop_filter_ref_delta;
 *                      update_ref_delta equal to 0 means that the bitstream does not contain this syntax element.
 * @loop_filter_ref_deltas[]: contains the adjustment needed for the filter level based on the chosen reference frame.
 *                            If this syntax element is not present in the bitstream, it maintains its previous value.
 * @update_mode_delta[]: equal to 1 means that the bitstream contains the syntax element loop_filter_mode_deltas;
 *                       update_mode_delta equal to 0 means that the bitstream does not contain this syntax element.
 * @loop_filter_mode_deltas[]: contains the adjustment needed for the filter level based on the chosen mode. If this syntax
 *                             element is not present in the bitstream, it maintains its previous value.
 * --- includes Loop filter delta parameters:
 * @delta_lf_present specifies whether loop filter delta values are present in the bitstream.
 * @delta_lf_res: specifies the left shift which should be applied to decoded loop filter delta values.
 * @delta_lf_multi: equal to 1 specifies that separate loop filter deltas are sent for horizontal luma edges, vertical luma
 *                  edges, the U edges, and the V edges. delta_lf_multi equal to 0 specifies that the same loop filter delta
 *                  is used for all edges.
 *
 */

struct _GstAV1LoopFilterParams {
  guint8 loop_filter_level[4]; /*is 4 fixed?? */
  guint8 loop_filter_sharpness;
  gboolean loop_filter_delta_enabled;
  gboolean loop_filter_delta_update;

  gboolean update_ref_deltas[GST_AV1_TOTAL_REFS_PER_FRAME];
  guint8 loop_filter_ref_deltas[GST_AV1_TOTAL_REFS_PER_FRAME];

  guint8 update_mode_deltas[2]; /* is 2 fixed? */
  guint8 loop_filter_mode_deltas[2];

  gboolean delta_lf_present;
  guint8 delta_lf_res;
  guint8 delta_lf_multi;
};

/**
 * _GstAV1QuantizationParams:
 *
 * @base_q_idx: indicates the base frame qindex. This is used for Y AC coefficients and as the base value for
 *              the other quantizers.
 * @DeltaQYDc: indicates the Y DC quantizer relative to base_q_idx.
 * @diff_uv_delta: equal to 1 indicates that the U and V delta quantizer values are coded separately. diff_uv_delta
 *                 equal to 0 indicates that the U and V delta quantizer values share a common value.
 * @DeltaQUDc: indicates the U DC quantizer relative to base_q_idx.
 * @DeltaQUAc: indicates the U AC quantizer relative to base_q_idx.
 * @DeltaQVAc: indicates the V AC quantizer relative to base_q_idx.
 * @using_qmatrix: specifies that the quantizer matrix will be used to compute quantizers.
 * @qm_y: specifies the level in the quantizer matrix that should be used for luma plane decoding.
 * @qm_u: specifies the level in the quantizer matrix that should be used for chroma U plane decoding.
 * @qm_v: specifies the level in the quantizer matrix that should be used for chroma V plane decoding.
 * --- includes Quantizer index delta parameters:
 * @delta_q_present: specifies whether quantizer index delta values are present in the bitstream.
 * @delta_q_res: specifies the left shift which should be applied to decoded quantizer index delta values.
 */
struct _GstAV1QuantizationParams {
  guint8 base_q_idx;
  gint8 DeltaQYDc;
  guint8 diff_uv_delta;
  gint8 DeltaQUDc;
  gint8 DeltaQUAc;
  gint8 DeltaQVDc;
  gint8 DeltaQVAc;
  guint8 using_qmatrix;
  guint8 qm_y;
  guint8 qm_u;
  guint8 qm_v;
  guint8 delta_q_present;
  guint8 delta_q_res;
};

/**
 * _GstAV1SegmenationParams:
 *
 * @segmentation_enabled: equal to 1 indicates that this frame makes use of the segmentation tool;
 *                        segmentation_enabled equal to 0 indicates that the frame does not use segmentation.
 * @segmentation_update_map: equal to 1 indicates that the segmentation map are updated during the decoding
 *                           of this frame. segmentation_update_map equal to 0 means that the segmentation map
 *                           from the previous frame is used.
 * @segmentation_temporal_update: equal to 1 indicates that the updates to the segmentation map are coded relative to
 *                                the existing segmentation map. segmentation_temporal_update equal to 0 indicates that
 *                                the new segmentation map is coded without reference to the existing segmentation map.
 * @segmentation_update_data: equal to 1 indicates that new parameters are about to be specified for each segment.
 *                            segmentation_update_data equal to 0 indicates that the segmentation parameters should keep
 *                            their existing values.
 * @FeatureEnabled[]: equal to 0 indicates that the corresponding feature is unused and has value equal to 0.
 *                    FeatureEnabled[] equal to 1 indicates that the feature value is coded in the bitstream.
 * @FeatureData[]: specifies the feature data for a segment feature.
 * @SegIdPreSkip: equal to 1 indicates that the segment id will be read before the skip syntax element. SegIdPreSkip
 *                equal to 0 indicates that the skip syntax element will be read first.
 * @LastActiveSegId: indicates the highest numbered segment id that has some enabled feature. This is used when decoding
 *                   the segment id to only decode choices corresponding to used segments.
 */


struct _GstAV1SegmenationParams {
  gboolean segmentation_enabled;
  guint8 segmentation_update_map;
  guint8 segmentation_temporal_update;
  guint8 segmentation_update_data;
  gint8 FeatureEnabled[GST_AV1_MAX_SEGMENTS][GST_AV1_SEG_LVL_MAX];
  gint16 FeatureData[GST_AV1_MAX_SEGMENTS][GST_AV1_SEG_LVL_MAX];
  guint8 SegIdPreSkip;
  guint8 LastActiveSegId;
};


/**
 * _GstAV1TileInfo:
 * @uniform_tile_spacing_flag: equal to 1 means that the tiles are uniformly spaced across the frame. (In other words, all
 *                             tiles are the same size except for the ones at the right and bottom edge which can be smaller.)
 *                             uniform_tile_spacing_flag equal to 0 means that the tile sizes are coded.
 * @TileColsLog2: specifies the base 2 logarithm of the desired number of tiles across the frame.
 * @TileCols: specifies the number of tiles across the frame. It is a requirement of bitstream conformance that TileCols is
 *            less than or equal to MAX_TILE_COLS.
 * @TileRowsLog2: specifies the base 2 logarithm of the desired number of tiles down the frame.
 * @TileRows: specifies the number of tiles down the frame. It is a requirement of bitstream conformance that TileRows is
 *            less than or equal to MAX_TILE_ROWS.
 */

/*
 * @width_in_sbs_minus_1[]: specifies the width of a tile minus 1 in units of superblocks.
 * @tileWidthSb[]: is used to specify the width of each tile in units of superblocks. It is a requirement of bitstream conformance
 *                 that tileWidthSb is less than maxTileWidthSb.
 * @height_in_sbs_minus_1[]: specifies the height of a tile minus 1 in units of superblocks.
 * @tileHeightSb[]: is used to specify the height of each tile in units of superblocks. It is a requirement of bitstream conformance
 *                   that tileWidthSb * tileHeightSb is less than maxTileAreaSb.
 * @tile_size_bytes_minus_1: is used to compute TileSizeBytes
 */

/*
 * @MiColStarts[]: is an array specifying the start column (in units of 4x4 luma samples) for each tile across the image.
 *                 If uniform_tile_spacing_flag is equal to 0, it is a requirement of bitstream conformance that startSb is equal
 *                 to sbCols when the loop writing MiColStarts exits.
 * @MiRowStarts[]: is an array specifying the start row (in units of 4x4 luma samples) for each tile down the image.
 *                 If uniform_tile_spacing_flag is equal to 0, it is a requirement of bitstream conformance that startSb is equal
 *                 to sbRows when the loop writing MiRowStarts exits.
 * @maxTileHeightSb: specifies the maximum height (in units of superblocks) that can be used for a tile (to avoid making tiles
 *                   with too much area).
 * @context_update_tile_id: specifies which tile to use for the CDF update.
 * @TileSizeBytes specifies the number of bytes needed to code each tile size.
 *
 */

struct _GstAV1TileInfo {
  gboolean uniform_tile_spacing_flag;
  guint8 TileColsLog2;
  guint8 TileCols;
  guint8 TileRowsLog2;
  guint8 TileRows;
  /*
  guint32 width_in_sbs_minus_1[GST_AV1_MAX_TILE_COUNT;
  guint32 tileWidthSb[GST_AV1_MAX_TILE_COUNT];
  guint32 height_in_sbs_minus_1[GST_AV1_MAX_TILE_COUNT];
  guint32 tileHeightSb[GST_AV1_MAX_TILE_COUNT];
  */
  guint32 MiColStarts[GST_AV1_MAX_TILE_COUNT];
  guint32 MiRowStarts[GST_AV1_MAX_TILE_COUNT];
  guint8 maxTileHeightSb;
  guint8 context_update_tile_id;
  /*guint8 tile_size_bytes_minus_1; */
  guint8 TileSizeBytes;
};

/**
 * _GstAV1CDEFParams:
 * @cdef_damping_minus_3 controls the amount of damping in the deringing filter.
 * @cdef_bits specifies the number of bits needed to specify which CDEF filter to apply.
 * @cdef_y_pri_strength: specify the strength of the primary filter (Y component)
 * @cdef_uv_pri_strength: specify the strength of the primary filter (UV components).
 * @cdef_y_sec_strength: specify the strength of the secondary filter (Y component).
 * @cdef_uv_sec_strength: specify the strength of the secondary filter (UV components).
 *
 */

struct _GstAV1CDEFParams {
  guint8 cdef_damping_minus_3;
  guint8 cdef_bits;
  guint8 cdef_y_pri_strength[GST_AV1_CDEF_MAX];
  guint8 cdef_y_sec_strength[GST_AV1_CDEF_MAX];
  guint8 cdef_uv_pri_strength[GST_AV1_CDEF_MAX];
  guint8 cdef_uv_sec_strength[GST_AV1_CDEF_MAX];
};

/**
 * _GstAV1LoopRestorationParams:
 *
 * @lr_unit_shift: specifies if the luma restoration size should be halved.
 * @lr_unit_extra_shift: specifies if the luma restoration size should be halved again.
 * @lr_uv_shift: is only present for 4:2:0 formats and specifies if the chroma size should be half the luma size.
 * @UsesLr: indicates if any plane uses loop restoration.
 * @usesChromaLr: indicates if chroma plane(s) use loop restoration.
 * @FrameRestorationType[]: specifies the type of restoration used for each plane.
 * @LoopRestorationSize[]: specifies the size of loop restoration units in units of samples in the current plane.
 *
 */

struct _GstAV1LoopRestorationParams {
  guint8 lr_unit_shift;
  guint8 lr_unit_extra_shift;
  guint8 lr_uv_shift;
  gboolean usesChromaLr;
  gboolean UsesLr;
  GstAV1FrameRestorationType FrameRestorationType[GST_AV1_MAX_NUM_PLANES];
  guint32 LoopRestorationSize[GST_AV1_MAX_NUM_PLANES];

};

/**
 * _GstAV1GlobalMotionParams:
 *
 * @is_global: specifies whether global motion parameters are present for a particular reference frame.
 * @is_rot_zoom: specifies whether a particular reference frame uses rotation and zoom global motion.
 * @is_translation: specifies whether a particular reference frame uses translation global motion.
 *
 */
struct _GstAV1GlobalMotionParams {
  gboolean is_global[GST_AV1_NUM_REF_FRAMES];
  gboolean is_rot_zoom[GST_AV1_NUM_REF_FRAMES];
  gboolean is_translation[GST_AV1_NUM_REF_FRAMES];
  GstAV1WarpModelType GmType[GST_AV1_NUM_REF_FRAMES];
  gint32 gm_params[GST_AV1_NUM_REF_FRAMES][6];

};

/**
 * _GstAV1FilmGrainParams:
 *
 * @apply_grain: equal to 1 specifies that film grain should be added to this frame. apply_grain equal to 0
 *               specifies that film grain should not be added.
 * @grain_seed: specifies the starting value for the pseudo-random numbers used during film grain synthesis.
 * @update_grain: equal to 1 means that a new set of parameters should be sent. update_grain equal to 0 means
 *                that the previous set of parameters should be used.
 * @film_grain_params_ref_idx: indicates which reference frame contains the film grain parameters to be used
 *                             for this frame.
 * @num_y_points: specifies the number of points for the piece-wise linear scaling function of the luma component.
 *                It is a requirement of bitstream conformance that num_y_points is less than or equal to 14.
 * @point_y_value[i]: represents the x (luma value) coordinate for the i-th point of the piecewise linear scaling
 *                    function for luma component. The values are signaled on the scale of 0..255. (In case of 10
 *                    bit video, these values correspond to luma values divided by 4. In case of 12 bit video,
 *                    these values correspond to luma values divided by 16.) If i is greater than 0, it is a r
 *                    equirement of bitstream conformance that point_y_value[ i ] is greater than
 *                    point_y_value[ i - 1 ] (this ensures the x coordinates are specified in increasing order).
 * @point_y_scaling[i]: represents the scaling (output) value for the i-th point of the piecewise linear scaling
 *                      function for luma component. chroma_scaling_from_luma specifies that the chroma scaling
 *                      is inferred from the luma scaling.
 * @num_cb_points: specifies the number of points for the piece-wise linear scaling function of the cb component.
 *                 It is a requirement of bitstream conformance that num_cb_points is less than or equal to 10.
 * @point_cb_value[i]: represents the x coordinate for the i-th point of the piece-wise linear scaling function
 *                     for cb component. The values are signaled on the scale of 0..255. If i is greater than 0,
 *                     it is a requirement of bitstream conformance that point_cb_value[ i ] is greater than
 *                     point_cb_value[ i - 1 ].
 * @point_cb_scaling[i]: represents the scaling (output) value for the i-th point of the piecewise linear scaling
 *                       function for cb component.
 * @num_cr_points: specifies represents the number of points for the piece-wise linear scaling function of the cr
 *                 component.
 *                 It is a requirement of bitstream conformance that num_cr_points is less than or equal to 10.
 *                 If subsampling_x is equal to 1 and subsampling_y is equal to 1 and num_cb_points is equal to 0,
 *                 it is a requirement of bitstream conformance that num_cr_points is equal to 0.
 *                 If subsampling_x is equal to 1 and subsampling_y is equal to 1 and num_cb_points is not equal
 *                 to 0, it is a requirement of bitstream conformance that num_cr_points is not equal to 0.
 *
 * @point_cr_value[i]: represents the x coordinate for the i-th point of the piece-wise linear scaling function for
 *                     cr component. The values are signaled on the scale of 0..255. If i is greater than 0, it is a
 *                     requirement of bitstream conformance that point_cr_value[ i ] is greater than
 *                     point_cr_value[ i - 1 ].
 * @point_cr_scaling[i]: represents the scaling (output) value for the i-th point of the piecewise linear scaling
 *                       function for cr component.
 * @grain_scaling_minus_8: represents the shift – 8 applied to the values of the chroma component. The
 *                         grain_scaling_minus_8 can take values of 0..3 and determines the range and quantization
 *                         step of the standard deviation of film grain.
 * @ar_coeff_lag: specifies the number of auto-regressive coefficients for luma and chroma.
 * @ar_coeffs_y_plus_128[i]: specifies auto-regressive coefficients used for the Y plane.
 * @ar_coeffs_cb_plus_128[i]: specifies auto-regressive coefficients used for the U plane.
 * @ar_coeffs_cr_plus_128[i]: specifies auto-regressive coefficients used for the V plane.
 * @ar_coeff_shift_minus_6: specifies the range of the auto-regressive coefficients. Values of 0, 1, 2, and 3
 *                          correspond to the ranges for auto-regressive coefficients of [-2, 2), [-1, 1),
 *                          [-0.5, 0.5) and [-0.25, 0.25) respectively.
 * @grain_scale_shift: specifies how much the Gaussian random numbers should be scaled down during the grain
 *                     synthesis process.
 * @cb_mult: represents a multiplier for the cb component used in derivation of the input index to the cb component
 *           scaling function.
 * @cb_luma_mult: represents a multiplier for the average luma component used in derivation of the input index to
 *                the cb component scaling function.
 * @cb_offset: represents an offset used in derivation of the input index to the cb component scaling function.
 * @cr_mult: represents a multiplier for the cr component used in derivation of the input index to the cr component
 *           scaling function.
 * @cr_luma_mult: represents a multiplier for the average luma component used in derivation of the input index to
 *                the cr component scaling function.
 * @cr_offset: represents an offset used in derivation of the input index to the cr component scaling function.
 * @overlap_flag: equal to 1 indicates that the overlap between film grain blocks shall be applied. overlap_flag
 *                equal to 0 indicates that the overlap between film grain blocks shall not be applied.
 * @clip_to_restricted_range: equal to 1 indicates that clipping to the restricted (studio) range shall be applied
 *                            to the sample values after adding the film grain (see the semantics for color_range
 *                            for an explanation of studio swing).
 *                            clip_to_restricted_range equal to 0 indicates that clipping to the full range shall
 *                            be applied to the sample values after adding the film grain.
 *
 */


struct _GstAV1FilmGrainParams {
  gboolean apply_grain;
  guint16 grain_seed;
  gboolean update_grain;
  guint8 film_grain_params_ref_idx;
  guint8 num_y_points;
  guint8 point_y_value[GST_AV1_MAX_NUM_Y_POINTS];
  guint8 point_y_scaling[GST_AV1_MAX_NUM_Y_POINTS];
  guint8 chroma_scaling_from_luma;
  guint8 num_cb_points;
  guint8 point_cb_value[GST_AV1_MAX_NUM_CB_POINTS];
  guint8 point_cb_scaling[GST_AV1_MAX_NUM_CB_POINTS];
  guint8 num_cr_points;
  guint8 point_cr_value[GST_AV1_MAX_NUM_CR_POINTS];
  guint8 point_cr_scaling[GST_AV1_MAX_NUM_CR_POINTS];
  guint8 grain_scaling_minus_8;
  guint8 ar_coeff_lag;
  guint8 ar_coeffs_y_plus_128[GST_AV1_MAX_NUM_POS_LUMA];
  guint8 ar_coeffs_cb_plus_128[GST_AV1_MAX_NUM_POS_LUMA];
  guint8 ar_coeffs_cr_plus_128[GST_AV1_MAX_NUM_POS_LUMA];
  guint8 ar_coeff_shift_minus_6;
  guint8 grain_scale_shift;
  guint8 cb_mult;
  guint8 cb_luma_mult;
  guint8 cb_offset;
  guint8 cr_mult;
  guint8 cr_luma_mult;
  guint8 cr_offset;
  gboolean overlap_flag;
  guint8 clip_to_restricted_range;
};
/**
 * _GstAV1FrameHeaderOBU:
 *
 * @show_existing_frame: equal to 1, indicates the frame indexed by frame_to_show_map_idx is to be output;
 *                       show_existing_frame equal to 0 indicates that further processing is required.
 *                       If obu_type is equal to OBU_FRAME, it is a requirement of bitstream conformance
 *                       that show_existing_frame is equal to 0.
 * @frame_to_show_map_idx: specifies the frame to be output. It is only available if show_existing_frame is 1.
 * @frame_presentation_time: specifies the presentation time of the frame in clock ticks DispCT counted from the removal
 *                           time of the last frame with frame_type equal to KEY_FRAME for the operating point that is being
 *                           decoded. The syntax element is signaled as a fixed length unsigned integer with a length in bits
 *                           given by frame_presentation_time_length_minus_1 + 1. The frame_presentation_time is the remainder
 *                           of a modulo 1 << (frame_presentation_time_length_minus_1 + 1) counter.
 * @tu_presentation_delay: is a syntax element used by the decoder model. It does not affect the decoding process.
 * @display_frame_id provides: the frame id number for the frame to output. It is a requirement of bitstream conformance
 *                             that whenever display_frame_id is read, the value matches RefFrameId[ frame_to_show_map_idx ]
 *                             (the value of current_frame_id at the time that the frame indexed by frame_to_show_map_idx was
 *                             stored), and that RefValid[ frame_to_show_map_idx ] is equal to 1.
 *                             It is a requirement of bitstream conformance that the number of bits needed to read
 *                             display_frame_id does not exceed 16. This is equivalent to the constraint that idLen <= 16
 * @frame_type: specifies the type of the frame.
 * @FrameIsIntra: 0 indicates that this frame may use inter prediction), the requirements described in the frame size with
 *                refs semantics of section 6.7.5 must also be satisfied.
 * @show_frame: equal to 1 specifies that this frame should be immediately output once decoded. show_frame equal to 0
 *              specifies that this frame should not be immediately output. (It may be output later if a later uncompressed
 *              header uses show_existing_frame equal to 1).
 * @showable_frame: equal to 1 specifies that the frame may be output using the show_existing_frame mechanism.
 *                  showable_frame equal to 0 specifies that this frame will not be output using the show_existing_frame
 *                  mechanism. It is a requirement of bitstream conformance that when show_existing_frame is used to show
 *                  a previous frame, that the value of showable_frame for the previous frame was equal to 1.
 *                  It is a requirement of bitstream conformance that a particular showable frame is output via the
 *                  show_existing_frame mechanism at most once.
 * @error_resilient_mode: equal to 1 indicates that error resilient mode is enabled; error_resilient_mode equal to 0 indicates
 *                        that error resilient mode is disabled.
 * @disable_cdf_update: specifies whether the CDF update in the symbol decoding process should be disabled.
 * @allow_screen_content_tools: equal to 1 indicates that intra blocks may use palette encoding; allow_screen_content_tools
 *                              equal to 0 indicates that palette encoding is never used.
 * @force_integer_mv: equal to 1 specifies that motion vectors will always be integers. force_integer_mv equal to 0 specifies
 *                    that motion vectors can contain fractional bits.
 * @current_frame_id: specifies the frame id number for the current frame. Frame id numbers are additional information that
 *                    do not affect the decoding process, but provide decoders with a way of detecting missing reference frames
 *                    so that appropriate action can be taken.
 * @frame_size_override_flag: equal to 0 specifies that the frame size is equal to the size in the sequence header.
 *                            frame_size_override_flag equal to 1 specifies that the frame size will either be specified as the
 *                            size of one of the reference frames, or computed from the frame_width_minus_1 and
 *                            frame_height_minus_1 syntax elements.
 * @order_hint: is used to compute OrderHint.
 * @primary_ref_frame: specifies which reference frame contains the CDF values and other state that should be loaded at the
 *                     start of the frame.
 * @buffer_removal_time_present_flag: equal to 1 specifies that buffer_removal_time is present in the bitstream.
 *                                    buffer_removal_time_present_flag equal to 0 specifies that buffer_removal_time is not present
 *                                    in the bitstream.
 * @buffer_removal_time[]: specifies the frame removal time in units of DecCT clock ticks counted from the removal time of the last
 *                         frame with frame_type equal to KEY_FRAME for operating point opNum. buffer_removal_time is signaled as a
 *                         fixed length unsigned integer with a length in bits given by buffer_removal_time_length_minus_1 + 1.
 *                         buffer_removal_time is the remainder of a modulo 1 << ( buffer_removal_time_length_minus_1 + 1 ) counter.
 * @refresh_frame_flags: contains a bitmask that specifies which reference frame slots will be updated with the current frame
 *                       after it is decoded.
 *                       If frame_type is equal to GST_AV1_INTRA_ONLY_FRAME, it is a requirement of bitstream conformance that
 *                       refresh_frame_flags is not equal to 0xff.
 * @ref_order_hint[i]: specifies the expected output order hint for each reference buffer.
 * @allow_intrabc: equal to 1 indicates that intra block copy may be used in this frame. allow_intrabc equal to 0 indicates that
 *                 intra block copy is not allowed in this frame.
 * @frame_refs_short_signaling: equal to 1 indicates that only two reference frames are explicitly signaled.
 *                              frame_refs_short_signaling equal to 0 indicates that all reference frames are explicitly signaled.
 * @last_frame_idx: specifies the reference frame to use for LAST_FRAME.
 * @gold_frame_idx: specifies the reference frame to use for GOLDEN_FRAME.
 * @delta_frame_id_minus_1 is used to calculate DeltaFrameId.
 * @allow_high_precision_mv: equal to 0 specifies that motion vectors are specified to quarter pel precision;
 *                           allow_high_precision_mv equal to 1 specifies that motion vectors are specified to eighth pel
 *                           precision.
 * @is_motion_mode_switchable: equal to 0 specifies that only the SIMPLE motion mode will be used.
 * @use_ref_frame_mvs: equal to 1 specifies that motion vector information from a previous frame can be used when
 *                     decoding the current frame. use_ref_frame_mvs equal to 0 specifies that this information will
 *                     not be used.
 * @disable_frame_end_update_cdf: equal to 1 indicates that the end of frame CDF update is disabled;
 *                                disable_frame_end_update_cdf equal to 0 indicates that the end of frame CDF update
 *                                is enabled.
 * @allow_warped_motion: equal to 1 indicates that the syntax element motion_mode may be present. allow_warped_motion
 *                       equal to 0 indicates that the syntax element motion_mode will not be present (this means that
 *                       LOCALWARP cannot be signaled if allow_warped_motion is equal to 0).
 * @reduced_tx_set: equal to 1 specifies that the frame is restricted to a reduced subset of the full set of transform
 *                  types.
 * @frame_width_minus_1: plus one is the width of the frame in luma samples.
 *                       It is a requirement of bitstream conformance that frame_width_minus_1 is less than or equal to
 *                       max_frame_width_minus_1.
 * @frame_height_minus_1: plus one is the height of the frame in luma samples.
 *                        It is a requirement of bitstream conformance that frame_height_minus_1 is less than or equal to
 *                        max_frame_height_minus_1.
 * @render_and_frame_size_different: equal to 0 means that the render width and height are inferred from the frame width
 *                                   and height. render_and_frame_size_different equal to 1 means that the render width
 *                                   and height are explicitly coded in the bitstream.
 * @render_width_minus_1: plus one is the render width of the frame in luma samples.
 * @render_height_minus_1: plus one is the render height of the frame in luma samples.
 * @found_ref: equal to 1 indicates that the frame dimensions can be inferred from reference frame i where i is the loop
 *             counter in the syntax parsing process for frame_size_with_refs. found_ref equal to 0 indicates that the
 *             frame dimensions are not inferred from reference frame i.
 * @use_superres: equal to 0 indicates that no upscaling is needed. use_superres equal to 1 indicates that upscaling is
 *                needed.
 * @coded_denom: is used to compute the amount of upscaling.
 * @SuperresDenom: is the denominator of a fraction that specifies the ratio between the superblock width before and after
 *                 upscaling. The numerator of this fraction is equal to the constant SUPERRES_NUM.
 * @MiCols: is the number of 4x4 block columns in the frame.
 * @MiRows: is the number of 4x4 block rows in the frame.
 * @FrameWidth:
 * @FrameHeight:
 * @RenderWidth:
 * @RenderHeight:
 * @UpscaledWidth:
 * @is_filter_switchable: equal to 1 indicates that the filter selection is signaled at the block level;
 *                         is_filter_switchable equal to 0 indicates that the filter selection is signaled at the
 *                        frame level.
 * @interpolation_filter: specifies the filter selection used for performing inter prediction.
 * @skip_mode_present: equal to 1 specifies that the syntax element skip_mode will be coded in the bitstream.
 *                     skip_mode_present equal to 0 specifies that skip_mode will not be used for this frame.
 * @reference_select: equal to 1 specifies that the mode info for inter blocks contains the syntax element comp_mode that
 *                    indicates whether to use single or compound reference prediction. Reference_select equal to 0
 *                    specifies that all interblocks will use single prediction.
 * @expectedFrameId[]: specifies the frame id for each frame used for reference. It is a requirement of bitstream conformance
 *                     that whenever expectedFrameId[ i ] is calculated, the value matches RefFrameId[ ref_frame_idx[ i ] ]
 *                     (this contains the value of current_frame_id at the time that the frame indexed by ref_frame_idx was stored).

 * @OrderHints[]: specifies the expected output order for each reference frame.
 * @RefFrameSignBias[]: specifies the intended direction of the motion vector in time for each reference frame. A sign bias
 *                      equal to 0 indicates that the reference frame is a forwards reference (i.e. the reference frame is expected
 *                      to be output before the current frame); a sign bias equal to 1 indicates that the reference frame is a backwards
 *                      reference.
 * @CodedLossless: is a variable that is equal to 1 when all segments use lossless encoding. This indicates that the frame is
 *                 fully lossless at the coded resolution of FrameWidth by FrameHeight. In this case, the loop filter and CDEF filter
 *                 are disabled.
 * @AllLossless: is a variable that is equal to 1 when CodedLossless is equal to 1 and FrameWidth is equal to UpscaledWidth.
 *               This indicates that the frame is fully lossless at the upscaled resolution. In this case, the loop filter, CDEF filter, and
 *               loop restoration are disabled.
 * @LossLessArray:
 * @SegQMLevel:
 * @skipModeAllowed:
 * @SkipModeFrame[]: specifies the frames to use for compound prediction when skip_mode is equal to 1.
 */

struct _GstAV1FrameHeaderOBU {
  gboolean show_existing_frame;
  guint8 frame_to_show_map_idx;
  guint32 frame_presentation_time;
  guint32 tu_presentation_delay;
  guint32 display_frame_id;
  GstAV1FrameType frame_type;
  gboolean FrameIsIntra;
  gboolean show_frame;
  gboolean showable_frame;
  gboolean error_resilient_mode;
  gboolean disable_cdf_update;
  gboolean allow_screen_content_tools;
  guint8 force_integer_mv;
  guint32 current_frame_id;
  gboolean frame_size_override_flag;
  guint32 order_hint;
  guint8 primary_ref_frame;
  guint8 buffer_removal_time_present_flag;
  guint32 buffer_removal_time[GST_AV1_MAX_OPERATING_POINTS];
  guint8 refresh_frame_flags;
  guint32 ref_order_hint[GST_AV1_NUM_REF_FRAMES];
  gboolean allow_intrabc;
  guint8 frame_refs_short_signaling;
  guint8 last_frame_idx;
  guint8 gold_frame_idx;
  guint8 ref_frame_idx[GST_AV1_NUM_REF_FRAMES];
  guint32 delta_frame_id_minus_1; /* present in for loop --> check obu.c */
  gboolean allow_high_precision_mv;
  gboolean is_motion_mode_switchable;
  gboolean use_ref_frame_mvs;
  gboolean disable_frame_end_update_cdf;
  gboolean allow_warped_motion;
  gboolean reduced_tx_set;
  guint16 frame_width_minus_1;
  guint16 frame_height_minus_1;
  gboolean render_and_frame_size_different;
  guint16 render_width_minus_1;
  guint16 render_height_minus_1;
  gboolean found_ref;
  gboolean use_superres;
  guint8 coded_denom;
  guint32 UpscaledWidth;
  guint32 FrameWidth;
  guint32 FrameHeight;
  guint32 MiCols;
  guint32 MiRows;
  guint32 RenderWidth;
  guint32 RenderHeight;
  guint32 SuperresDenom;
  gboolean is_filter_switchable;
  GstAV1InterpolationFilter interpolation_filter;
  GstAV1LoopFilterParams loop_filter_params;
  GstAV1QuantizationParams quantization_params;
  GstAV1SegmenationParams segmentation_params;
  GstAV1TileInfo tile_info;
  GstAV1CDEFParams cdef_params;
  GstAV1LoopRestorationParams loop_restoration_params;
  gboolean tx_mode_select;
  GstAV1TXModes TxMode;
  gboolean skip_mode_present;
  gboolean reference_select;
  GstAV1GlobalMotionParams global_motion_params;
  GstAV1FilmGrainParams film_grain_params;
  guint32 expectedFrameId[GST_AV1_NUM_REF_FRAMES];
  guint32 OrderHints[GST_AV1_NUM_REF_FRAMES]; /* is guint32 appropiat? */
  guint32 RefFrameSignBias[GST_AV1_NUM_REF_FRAMES]; /* is guint32 appropiat? */
  gboolean CodedLossless;
  gboolean AllLossless;
  guint8 LosslessArray[GST_AV1_MAX_SEGMENTS];
  guint8 SegQMLevel[3][GST_AV1_MAX_SEGMENTS];
  guint8 skipModeAllowed;
  guint8 SkipModeFrame[2]; /* is 2 appropiat? */


};

/**
 * _GstAV1ReferenceFrameInfo:
 *
 * @RefValid:
 * @RefFrameId:
 * @RefUpscaledWidth:
 * @RefFrameHeight:
 * @RefFrameWidth:
 * @RefRenderHeight:
 * @RefRenderWidth:
 * @RefFrameType:
 * @RefOrderHint:
 * @RefMiCols;
 * @RefMiRows;
 * @RefBitDepth;
 * @RefSubsamplingX;
 * @RefSubsamplingY;
 *
 * Note: The Reference Info is only partly implemented - check chapter 7.20 and 7.21 of Spec
 */

struct _GstAV1ReferenceFrameInfo {
  struct {
    gboolean RefValid;
    guint32 RefFrameId;
    guint32 RefUpscaledWidth;
    guint32 RefFrameHeight;
    guint32 RefFrameWidth;
    guint32 RefRenderHeight;
    guint32 RefRenderWidth;
    GstAV1FrameType RefFrameType;
    guint32 RefOrderHint; /* is guint32 appropiat? */
    guint32 RefMiCols;
    guint32 RefMiRows;
    guint8 RefBitDepth;
    guint8 RefSubsamplingX;
    guint8 RefSubsamplingY;
  } entry[GST_AV1_REFS_PER_FRAME];
};


/**
 * _GstAV1TileListOBU:
 *
 * @output_frame_width_in_tiles_minus_1: plus one is the width of the output frame, in tile units.
 * @output_frame_height_in_tiles_minus_1: plus one is the height of the output frame, in tile units.
 * @tile_count_minus_1: plus one is the number of tile_list_entry in the list.
 *                      It is a requirement of bitstream conformance that tile_count_minus_1 is less
 *                      than or equal to 511.
 *
 *
 * @anchor_frame_idx: is the index into an array AnchorFrames of the frames that the tile uses for
 *                    prediction. The AnchorFrames array is provided by external means and may change
 *                    for each tile list OBU. The process for creating the AnchorFrames array is outside
 *                    of the scope of this specification.
 *                    It is a requirement of bitstream conformance that anchor_frame_idx is less than or
 *                    equal to 127.
 * @anchor_tile_row_ is: the row coordinate of the tile in the frame that it belongs, in tile units. It is
 *                       a requirement of bitstream conformance that anchor_tile_row is less than TileRows.
 * @anchor_tile_col: is the column coordinate of the tile in the frame that it belongs, in tile units.
 *                   It is a requirement of bitstream conformance that anchor_tile_col is less than TileCols.
 * @tile_data_size_minus_1: plus one is the size of the coded tile data, coded_tile_data, in bytes.
 *
 */

struct _GstAV1TileListOBU {
  guint8 output_frame_width_in_tiles_minus_1;
  guint8 output_frame_height_in_tiles_minus_1;
  guint16 tile_count_minus_1;
  struct {
    guint8 anchor_frame_idx;
    guint8 anchor_tile_row;
    guint8 anchor_tile_col;
    guint16 tile_data_size_minus_1;
    /*guint8 coded_tile_data[]; skipped */
  } entry[GST_AV1_MAX_TILE_COUNT];
};

/**
 * _GstAV1TileListOBU:
 *
 *
 * @NumTiles: specifies the total number of tiles in the frame.
 * @tile_start_and_end_present_flag: specifies whether tg_start and tg_end are present in the bitstream.
 *                                   If tg_start and tg_end are not present in the bitstream, this tile group covers the
 *                                   entire frame. If obu_type is equal to OBU_FRAME, it is a requirement of bitstream conformance
 *                                   that the value of tile_start_and_end_present_flag is equal to 0.
 * @tg_start: specifies the zero-based index of the first tile in the current tile group.
 *            It is a requirement of bitstream conformance that the value of tg_start is equal to the value of TileNum at the point
 *            that tile_group_obu is invoked.
 * @tg_end: specifies the zero-based index of the last tile in the current tile group.
 *          It is a requirement of bitstream conformance that the value of tg_end is greater than or equal to tg_start.
 *          It is a requirement of bitstream conformance that the value of tg_end for the last tile group in each frame is equal to
 *          NumTiles-1.
 * @tileRow:
 * @tileCol:
 * @tileSize: specifies the size in bytes of the next coded tile.
 * @MiRowStart:
 * @MiRowEnd:
 * @MiColStart:
 * @MiColEnd:
 * @CurrentQIndex:
 *
 */
struct _GstAV1TileGroupOBU {
  guint32 NumTiles;
  gboolean tile_start_and_end_present_flag;
  guint8 tg_start;
  guint8 tg_end;
  struct {
    guint32 tileRow;
    guint32 tileCol;
    guint32 tileSize;
    guint32 MiRowStart;
    guint32 MiRowEnd;
    guint32 MiColStart;
    guint32 MiColEnd;
    guint8 CurrentQIndex;
  } entry[GST_AV1_MAX_TILE_COUNT];
};

/**
 * _GstAV1FrameOBU:
 *
 */
struct _GstAV1FrameOBU {
  GstAV1TileGroupOBU tile_group;
  GstAV1FrameHeaderOBU frame_header;
};

/**
 * GstAV1Parser:
 *
 * @SeenFrameHeader: is a variable used to mark whether the frame header for the current frame has been received.
 *                   It is initialized to zero.
 *
 */

struct _GstAV1Parser {

  void *priv;

  guint8 subsampling_x;
  guint8 subsampling_y;
  guint8 BitDepth;

  GstAV1ReferenceFrameInfo ref_info;
  /*References */
  GstAV1SequenceHeaderOBU *seq_header;
  GstAV1FrameHeaderOBU *frame_header;
};

/**
 * GstAV1AnnexB:
 *
 */


GST_CODEC_PARSERS_API
GstAV1Parser *     gst_av1_parser_new (void);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_obu_header (GstAV1Parser * parser, GstBitReader * br, GstAV1OBUHeader * obu_header, gsize annexb_sz);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_sequence_header_obu (GstAV1Parser * parser, GstBitReader * br, GstAV1SequenceHeaderOBU * seq_header);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_temporal_delimiter_obu (GstAV1Parser * parser, GstBitReader * br);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_metadata_obu (GstAV1Parser * parser, GstBitReader * br, GstAV1MetadataOBU * metadata);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_tile_list_obu (GstAV1Parser * parser, GstBitReader * br, GstAV1TileListOBU * tile_list);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_tile_group_obu (GstAV1Parser * parser, GstBitReader * br, gsize sz, GstAV1TileGroupOBU * tile_group);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_frame_header_obu (GstAV1Parser * parser, GstBitReader * br, GstAV1FrameHeaderOBU * frame_header);

GST_CODEC_PARSERS_API
GstAV1ParserResult gst_av1_parse_frame_obu (GstAV1Parser * parser, GstBitReader * br, GstAV1FrameOBU * frame);

GstAV1ParserResult
gst_av1_parse_annexb_unit_size(GstAV1Parser * parser, GstBitReader * br, gsize * unit_size);

GST_CODEC_PARSERS_API
void               gst_av1_parser_free (GstAV1Parser * parser);



G_END_DECLS

#endif /* GST_AV1_PARSER_H */
