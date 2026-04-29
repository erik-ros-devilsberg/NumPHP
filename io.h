#ifndef NUMPHP_IO_H
#define NUMPHP_IO_H

#include "numphp.h"
#include "ndarray.h"

/* Binary file format
 * ------------------
 *   bytes 0..6   magic              "NUMPHP\0"
 *   byte  7      format version     \x01 (this build)
 *   byte  8      dtype              0=f32, 1=f64, 2=i32, 3=i64 (numphp_dtype enum)
 *   byte  9      ndim               0..NUMPHP_MAX_NDIM
 *   bytes 10..15 reserved (must be zero on write; ignored on read for forward compat)
 *   bytes 16..   shape[0..ndim-1]   int64 little-endian, 8 bytes each
 *   bytes ...    data buffer        size * itemsize bytes, little-endian
 *
 * Strides are NOT stored — always re-derived as C-contig on load.
 *
 * Big-endian platforms are unsupported in v1; io.c #errors at compile time.
 *
 * Loader checks the version byte FIRST. Bumping the version byte is the only
 * permitted breaking change. Forward extensions land in the 6 reserved bytes. */

#define NUMPHP_BINARY_MAGIC          "NUMPHP\0"   /* 7 bytes (sans version) */
#define NUMPHP_BINARY_MAGIC_LEN      7
#define NUMPHP_BINARY_FORMAT_VERSION 1
#define NUMPHP_BINARY_HEADER_BYTES   16

/* Public API used by ndarray.c PHP_METHOD wrappers. Each returns 1 on success
 * and 0 on error (exception thrown). */
int  numphp_io_save(numphp_ndarray *a, const char *path, size_t path_len);
numphp_ndarray *numphp_io_load(const char *path, size_t path_len);

int  numphp_io_csv_write(numphp_ndarray *a, const char *path, size_t path_len);
numphp_ndarray *numphp_io_csv_read(const char *path, size_t path_len,
                                   numphp_dtype dtype, int header);

#endif /* NUMPHP_IO_H */
