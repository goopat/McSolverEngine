#pragma once

// Rename all zlib public symbols with McSolverEngine_ project prefix to prevent
// collision with any other zlib instance (static or dynamic) in the same process.
//
// This header is force-included when compiling zlib .c files (/FI on MSVC) and
// included directly by our C++ code. Every symbol that zconf.h renames under
// Z_PREFIX is covered — plus z_stream and z_streamp which Z_PREFIX skips.

// -- internal init macros (same pattern as zconf.h Z_PREFIX) ------------------
#define _dist_code                 McSolverEngine__dist_code
#define _length_code               McSolverEngine__length_code
#define _tr_align                  McSolverEngine__tr_align
#define _tr_flush_bits             McSolverEngine__tr_flush_bits
#define _tr_flush_block            McSolverEngine__tr_flush_block
#define _tr_init                   McSolverEngine__tr_init
#define _tr_stored_block           McSolverEngine__tr_stored_block
#define _tr_tally                  McSolverEngine__tr_tally

// -- core API ----------------------------------------------------------------
#define adler32                    McSolverEngine_adler32
#define adler32_combine            McSolverEngine_adler32_combine
#define adler32_combine64          McSolverEngine_adler32_combine64
#define adler32_z                  McSolverEngine_adler32_z
#define compress                   McSolverEngine_compress
#define compress2                  McSolverEngine_compress2
#define compressBound              McSolverEngine_compressBound
#define crc32                      McSolverEngine_crc32
#define crc32_combine              McSolverEngine_crc32_combine
#define crc32_combine64            McSolverEngine_crc32_combine64
#define crc32_combine_gen          McSolverEngine_crc32_combine_gen
#define crc32_combine_gen64        McSolverEngine_crc32_combine_gen64
#define crc32_combine_op           McSolverEngine_crc32_combine_op
#define crc32_z                    McSolverEngine_crc32_z
#define deflate                    McSolverEngine_deflate
#define deflateBound               McSolverEngine_deflateBound
#define deflateCopy                McSolverEngine_deflateCopy
#define deflateEnd                 McSolverEngine_deflateEnd
#define deflateInit                McSolverEngine_deflateInit
#define deflateInit2               McSolverEngine_deflateInit2
#define deflateInit2_              McSolverEngine_deflateInit2_
#define deflateInit_               McSolverEngine_deflateInit_
#define deflateParams              McSolverEngine_deflateParams
#define deflateReset               McSolverEngine_deflateReset
#define deflateSetDictionary       McSolverEngine_deflateSetDictionary
#define deflate_copyright          McSolverEngine_deflate_copyright
#define get_crc_table              McSolverEngine_get_crc_table
#define gzclose                    McSolverEngine_gzclose
#define gzdopen                    McSolverEngine_gzdopen
#define gzerror                    McSolverEngine_gzerror
#define gzopen                     McSolverEngine_gzopen
#define gzread                     McSolverEngine_gzread
#define gzwrite                    McSolverEngine_gzwrite
#define inflate                    McSolverEngine_inflate
#define inflateBack                McSolverEngine_inflateBack
#define inflateBackEnd             McSolverEngine_inflateBackEnd
#define inflateCodesUsed           McSolverEngine_inflateCodesUsed
#define inflateCopy                McSolverEngine_inflateCopy
#define inflateEnd                 McSolverEngine_inflateEnd
#define inflateGetDictionary       McSolverEngine_inflateGetDictionary
#define inflateGetHeader           McSolverEngine_inflateGetHeader
#define inflateInit                McSolverEngine_inflateInit
#define inflateInit2               McSolverEngine_inflateInit2
#define inflateInit2_              McSolverEngine_inflateInit2_
#define inflateInit_               McSolverEngine_inflateInit_
#define inflateMark                McSolverEngine_inflateMark
#define inflatePrime               McSolverEngine_inflatePrime
#define inflateReset               McSolverEngine_inflateReset
#define inflateReset2              McSolverEngine_inflateReset2
#define inflateResetKeep           McSolverEngine_inflateResetKeep
#define inflateSetDictionary       McSolverEngine_inflateSetDictionary
#define inflateSync                McSolverEngine_inflateSync
#define inflateSyncPoint           McSolverEngine_inflateSyncPoint
#define inflateUndermine           McSolverEngine_inflateUndermine
#define inflateValidate            McSolverEngine_inflateValidate
#define inflate_copyright          McSolverEngine_inflate_copyright
#define inflate_fast               McSolverEngine_inflate_fast
#define inflate_table              McSolverEngine_inflate_table
#define inflate_fixed              McSolverEngine_inflate_fixed
#define uncompress                 McSolverEngine_uncompress
#define uncompress2                McSolverEngine_uncompress2
#define zError                     McSolverEngine_zError
#define zcalloc                    McSolverEngine_zcalloc
#define zcfree                     McSolverEngine_zcfree
#define zlibCompileFlags           McSolverEngine_zlibCompileFlags
#define zlibVersion                McSolverEngine_zlibVersion

// -- typedefs ----------------------------------------------------------------
#define Byte                   McSolverEngine_Byte
#define Bytef                  McSolverEngine_Bytef
#define alloc_func             McSolverEngine_alloc_func
#define charf                  McSolverEngine_charf
#define free_func              McSolverEngine_free_func
#define gzFile                 McSolverEngine_gzFile
#define gz_header              McSolverEngine_gz_header
#define gz_headerp             McSolverEngine_gz_headerp
#define in_func                McSolverEngine_in_func
#define intf                   McSolverEngine_intf
#define out_func               McSolverEngine_out_func
#define uInt                   McSolverEngine_uInt
#define uIntf                  McSolverEngine_uIntf
#define uLong                  McSolverEngine_uLong
#define uLongf                 McSolverEngine_uLongf
#define voidp                  McSolverEngine_voidp
#define voidpc                 McSolverEngine_voidpc
#define voidpf                 McSolverEngine_voidpf

// -- struct names that Z_PREFIX does not rename ---------------------------------
#define z_stream               McSolverEngine_z_stream
#define z_streamp              McSolverEngine_z_streamp

// After all prefix macros are defined, pull in the real zlib header.
#include "zlib.h"
