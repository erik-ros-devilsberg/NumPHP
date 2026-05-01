/* io.c — file I/O for numphp.
 *
 * Stream layer: php_stream_open_wrapper everywhere — gets us open_basedir
 * honoring + stream wrappers (phar://, data://) for free.
 *
 * Endianness: little-endian only in v1. The compile-time check below makes
 * the deferral explicit; big-endian users get a clear error.
 *
 * CSV: delegates to php_fgetcsv / php_fputcsv from ext/standard/file.h to
 * avoid re-implementing RFC 4180. Float writes save and restore LC_NUMERIC
 * around the loop so the user's PHP locale can't corrupt the output. */

#include "io.h"
#include "ndarray.h"

#include "Zend/zend_exceptions.h"
#include "Zend/zend_strtod.h"
#include "ext/standard/file.h"
#include "main/php_streams.h"

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <locale.h>
#include <stdio.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#  error "numphp binary format is little-endian only in v1; big-endian platforms are unsupported."
#endif

/* ============================================================================
 * Binary format — save / load
 * ============================================================================ */

int numphp_io_save(numphp_ndarray *a, const char *path, size_t path_len)
{
    (void)path_len;

    /* Materialize a C-contiguous owner if `a` isn't already. We always write
     * row-major order, regardless of the source's stride layout. */
    int owns_contig = 0;
    numphp_ndarray *contig = a;
    if (!(a->flags & NUMPHP_C_CONTIGUOUS)) {
        contig = numphp_materialize_contiguous(a);
        owns_contig = 1;
    }

    php_stream *s = php_stream_open_wrapper((char *)path, "wb",
                        0, NULL);
    if (!s) {
        if (owns_contig) numphp_ndarray_free(contig);
        /* php_stream_open_wrapper already raised a PHP warning on REPORT_ERRORS;
         * also throw an explicit exception so user-facing code can catch it. */
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::save: failed to open '%s' for writing", path);
        return 0;
    }

    unsigned char header[NUMPHP_BINARY_HEADER_BYTES] = {0};
    memcpy(header, NUMPHP_BINARY_MAGIC, NUMPHP_BINARY_MAGIC_LEN);
    header[7] = (unsigned char)NUMPHP_BINARY_FORMAT_VERSION;
    header[8] = (unsigned char)contig->dtype;
    header[9] = (unsigned char)contig->ndim;
    /* bytes 10..15 left zero — reserved */

    if (php_stream_write(s, (const char *)header, sizeof(header)) != sizeof(header)) {
        php_stream_close(s);
        if (owns_contig) numphp_ndarray_free(contig);
        zend_throw_exception(numphp_ndarray_exception_ce,
            "NDArray::save: failed to write header", 0);
        return 0;
    }

    /* Shape: int64 little-endian, ndim entries. */
    for (int i = 0; i < contig->ndim; i++) {
        int64_t dim = (int64_t)contig->shape[i];
        if (php_stream_write(s, (const char *)&dim, sizeof(dim)) != sizeof(dim)) {
            php_stream_close(s);
            if (owns_contig) numphp_ndarray_free(contig);
            zend_throw_exception(numphp_ndarray_exception_ce,
                "NDArray::save: failed to write shape", 0);
            return 0;
        }
    }

    /* Body */
    size_t body_bytes = (size_t)(contig->size * contig->itemsize);
    if (body_bytes > 0) {
        if ((size_t)php_stream_write(s, (const char *)contig->buffer->data, body_bytes) != body_bytes) {
            php_stream_close(s);
            if (owns_contig) numphp_ndarray_free(contig);
            zend_throw_exception(numphp_ndarray_exception_ce,
                "NDArray::save: failed to write body", 0);
            return 0;
        }
    }

    php_stream_close(s);
    if (owns_contig) numphp_ndarray_free(contig);
    return 1;
}

numphp_ndarray *numphp_io_load(const char *path, size_t path_len)
{
    (void)path_len;

    php_stream *s = php_stream_open_wrapper((char *)path, "rb",
                        0, NULL);
    if (!s) {
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::load: failed to open '%s' for reading", path);
        return NULL;
    }

    /* Read header. */
    unsigned char header[NUMPHP_BINARY_HEADER_BYTES];
    ssize_t got = php_stream_read(s, (char *)header, sizeof(header));
    if (got < (ssize_t)sizeof(header)) {
        php_stream_close(s);
        zend_throw_exception(numphp_ndarray_exception_ce,
            "NDArray::load: file is empty or shorter than the 16-byte header", 0);
        return NULL;
    }

    if (memcmp(header, NUMPHP_BINARY_MAGIC, NUMPHP_BINARY_MAGIC_LEN) != 0) {
        php_stream_close(s);
        zend_throw_exception(numphp_ndarray_exception_ce,
            "NDArray::load: bad magic (not a numphp binary file)", 0);
        return NULL;
    }

    /* Version byte FIRST — before any other header parsing. */
    unsigned char version = header[7];
    if (version != NUMPHP_BINARY_FORMAT_VERSION) {
        php_stream_close(s);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::load: NUMPHP binary format version %d is not supported; "
            "this build understands version %d. Re-save with an older numphp or upgrade.",
            (int)version, NUMPHP_BINARY_FORMAT_VERSION);
        return NULL;
    }

    unsigned char dtype_byte = header[8];
    unsigned char ndim       = header[9];

    if (dtype_byte > NUMPHP_INT64) {
        php_stream_close(s);
        zend_throw_exception_ex(numphp_dtype_exception_ce, 0,
            "NDArray::load: unknown dtype byte %u", (unsigned)dtype_byte);
        return NULL;
    }
    if (ndim > NUMPHP_MAX_NDIM) {
        php_stream_close(s);
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::load: ndim %u exceeds NUMPHP_MAX_NDIM (%d)",
            (unsigned)ndim, NUMPHP_MAX_NDIM);
        return NULL;
    }

    /* Shape. */
    zend_long shape[NUMPHP_MAX_NDIM] = {0};
    zend_long size = 1;
    for (int i = 0; i < (int)ndim; i++) {
        int64_t dim;
        got = php_stream_read(s, (char *)&dim, sizeof(dim));
        if (got < (ssize_t)sizeof(dim)) {
            php_stream_close(s);
            zend_throw_exception(numphp_ndarray_exception_ce,
                "NDArray::load: file truncated in shape array", 0);
            return NULL;
        }
        if (dim < 0) {
            php_stream_close(s);
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "NDArray::load: negative dimension at axis %d", i);
            return NULL;
        }
        shape[i] = (zend_long)dim;
        size *= shape[i];
    }

    numphp_ndarray *out = numphp_ndarray_alloc_owner((numphp_dtype)dtype_byte,
                                                     (int)ndim, shape);
    size_t body_bytes = (size_t)(size * out->itemsize);
    if (body_bytes > 0) {
        got = php_stream_read(s, (char *)out->buffer->data, body_bytes);
        if (got < (ssize_t)body_bytes) {
            numphp_ndarray_free(out);
            php_stream_close(s);
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "NDArray::load: file truncated; expected %zu body bytes, got %zd",
                body_bytes, (ssize_t)got);
            return NULL;
        }
    }

    php_stream_close(s);
    return out;
}

/* ============================================================================
 * CSV — write
 * ============================================================================ */

/* Locale-safe float formatting. We bracket the write loop with setlocale
 * save/restore so a user's LC_NUMERIC (e.g. 'de_DE' with `,` decimal) cannot
 * corrupt the output. */
static zend_string *format_cell(numphp_dtype dt, char *p)
{
    char buf[64];
    int n = 0;
    switch (dt) {
        case NUMPHP_FLOAT32:
            n = snprintf(buf, sizeof(buf), "%.17g", (double)*(float *)p);
            break;
        case NUMPHP_FLOAT64:
            n = snprintf(buf, sizeof(buf), "%.17g", *(double *)p);
            break;
        case NUMPHP_INT32:
            n = snprintf(buf, sizeof(buf), "%d", (int)*(int32_t *)p);
            break;
        case NUMPHP_INT64:
            n = snprintf(buf, sizeof(buf), "%lld", (long long)*(int64_t *)p);
            break;
    }
    if (n < 0) n = 0;
    return zend_string_init(buf, (size_t)n, 0);
}

int numphp_io_csv_write(numphp_ndarray *a, const char *path, size_t path_len)
{
    (void)path_len;

    if (a->ndim > 2) {
        zend_throw_exception_ex(numphp_shape_exception_ce, 0,
            "NDArray::toCsv: input must be 1-D or 2-D (got %d-D)", a->ndim);
        return 0;
    }

    php_stream *s = php_stream_open_wrapper((char *)path, "wb",
                        0, NULL);
    if (!s) {
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::toCsv: failed to open '%s' for writing", path);
        return 0;
    }

    /* Shape normalisation: 1-D writes one cell per row. */
    int rows, cols;
    if (a->ndim == 1) {
        rows = (int)a->shape[0];
        cols = 1;
    } else {
        rows = (int)a->shape[0];
        cols = (int)a->shape[1];
    }

    /* Locale save/restore: lock LC_NUMERIC to "C" while we format floats so
     * `1.5` is always written `1.5`, never `1,5`. */
    char *prev_locale = setlocale(LC_NUMERIC, NULL);
    char saved[64] = {0};
    if (prev_locale) {
        strncpy(saved, prev_locale, sizeof(saved) - 1);
    }
    setlocale(LC_NUMERIC, "C");

    char *base = (char *)a->buffer->data + a->offset;
    int rc = 1;

    for (int i = 0; i < rows && rc; i++) {
        zval row;
        array_init_size(&row, (uint32_t)cols);
        for (int j = 0; j < cols; j++) {
            char *p;
            if (a->ndim == 1) {
                p = base + i * a->strides[0];
            } else {
                p = base + i * a->strides[0] + j * a->strides[1];
            }
            zend_string *cell = format_cell(a->dtype, p);
            zval z;
            ZVAL_STR(&z, cell);
            zend_hash_next_index_insert(Z_ARRVAL(row), &z);
        }
        ssize_t wrote = php_fputcsv(s, &row, ',', '"', '\\', NULL);
        zval_ptr_dtor(&row);
        if (wrote < 0) {
            zend_throw_exception(numphp_ndarray_exception_ce,
                "NDArray::toCsv: failed to write row", 0);
            rc = 0;
        }
    }

    /* Restore locale BEFORE closing, in case close triggers unrelated locale-
     * sensitive code. */
    setlocale(LC_NUMERIC, saved[0] ? saved : "C");

    php_stream_close(s);
    return rc;
}

/* ============================================================================
 * CSV — read
 * ============================================================================ */

static int parse_cell_to(numphp_dtype dt, const char *txt, size_t len,
                         char *dst, int row, int col)
{
    /* zend_strtod is locale-independent — always honours `.` decimal. */
    char *endp = NULL;
    if (dt == NUMPHP_FLOAT32 || dt == NUMPHP_FLOAT64) {
        double v = zend_strtod(txt, (const char **)&endp);
        if (endp == txt) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "NDArray::fromCsv: cannot parse float at row %d, col %d ('%s')",
                row, col, txt);
            return 0;
        }
        /* Allow trailing whitespace only. */
        while (endp && *endp && (*endp == ' ' || *endp == '\t' || *endp == '\r' || *endp == '\n')) endp++;
        if (endp && *endp != '\0' && (size_t)(endp - txt) < len) {
            zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
                "NDArray::fromCsv: trailing garbage in float at row %d, col %d ('%s')",
                row, col, txt);
            return 0;
        }
        if (dt == NUMPHP_FLOAT32) *(float *)dst = (float)v;
        else                       *(double *)dst = v;
        return 1;
    }
    /* int32 / int64 */
    long long v = strtoll(txt, &endp, 10);
    if (endp == txt) {
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::fromCsv: cannot parse integer at row %d, col %d ('%s')",
            row, col, txt);
        return 0;
    }
    while (endp && *endp && (*endp == ' ' || *endp == '\t' || *endp == '\r' || *endp == '\n')) endp++;
    if (endp && *endp != '\0' && (size_t)(endp - txt) < len) {
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::fromCsv: trailing garbage in integer at row %d, col %d ('%s')",
            row, col, txt);
        return 0;
    }
    if (dt == NUMPHP_INT32) *(int32_t *)dst = (int32_t)v;
    else                     *(int64_t *)dst = (int64_t)v;
    return 1;
}

numphp_ndarray *numphp_io_csv_read(const char *path, size_t path_len,
                                   numphp_dtype dtype, int header)
{
    (void)path_len;

    php_stream *s = php_stream_open_wrapper((char *)path, "rb",
                        0, NULL);
    if (!s) {
        zend_throw_exception_ex(numphp_ndarray_exception_ce, 0,
            "NDArray::fromCsv: failed to open '%s' for reading", path);
        return NULL;
    }

    /* Skip UTF-8 BOM if present. We can't push back, so use php_stream_seek. */
    {
        unsigned char bom[3];
        ssize_t got = php_stream_read(s, (char *)bom, 3);
        int is_bom = (got == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);
        if (!is_bom) {
            if (php_stream_seek(s, 0, SEEK_SET) != 0) {
                php_stream_close(s);
                zend_throw_exception(numphp_ndarray_exception_ce,
                    "NDArray::fromCsv: stream is not seekable", 0);
                return NULL;
            }
        }
    }

    /* Optionally skip header line. */
    if (header) {
        size_t hbuf_len;
        char *hbuf = php_stream_get_line(s, NULL, 0, &hbuf_len);
        if (hbuf) efree(hbuf);
    }

    /* Two-pass: collect HashTable* per row, validate uniform column count, then
     * allocate the NDArray and parse cells.
     *
     * php_fgetcsv contract: caller passes a buf from php_stream_get_line. The
     * function copies cell values into independent zend_strings in the returned
     * HashTable AND efree()s buf itself when stream is non-NULL. We must NOT
     * efree(buf) ourselves — that would be a double-free. The HashTable values
     * are independent of buf, so buf's lifetime ends inside php_fgetcsv. */
    HashTable **rows = NULL;
    int rows_alloc = 0, rows_used = 0;
    int cols = -1;
    int row_no = header ? 1 : 0;

    while (1) {
        size_t buf_len;
        char *buf = php_stream_get_line(s, NULL, 0, &buf_len);
        if (!buf) break;
        HashTable *ht = php_fgetcsv(s, ',', '"', PHP_CSV_NO_ESCAPE, buf_len, buf);
        /* php_fgetcsv has freed buf for us (stream != NULL). */
        if (!ht) ht = php_bc_fgetcsv_empty_line();
        if (!ht) break;
        int n = (int)zend_hash_num_elements(ht);
        if (cols < 0) cols = n;
        else if (n != cols) {
            for (int i = 0; i < rows_used; i++) {
                zend_array_release(rows[i]);
            }
            if (rows) efree(rows);
            zend_array_release(ht);
            php_stream_close(s);
            zend_throw_exception_ex(numphp_shape_exception_ce, 0,
                "NDArray::fromCsv: ragged CSV — row %d has %d cells, expected %d",
                row_no + 1, n, cols);
            return NULL;
        }
        if (rows_used == rows_alloc) {
            rows_alloc = rows_alloc ? rows_alloc * 2 : 16;
            rows = erealloc(rows, sizeof(HashTable *) * rows_alloc);
        }
        rows[rows_used++] = ht;
        row_no++;
    }

    php_stream_close(s);

    if (rows_used == 0) {
        if (rows) efree(rows);
        zend_throw_exception(numphp_ndarray_exception_ce,
            "NDArray::fromCsv: CSV is empty", 0);
        return NULL;
    }

    /* Allocate NDArray with shape (rows_used, cols). */
    zend_long shape[2] = { rows_used, cols };
    numphp_ndarray *out = numphp_ndarray_alloc_owner(dtype, 2, shape);
    char *dst = (char *)out->buffer->data;
    zend_long itemsize = out->itemsize;

    int rc = 1;
    for (int i = 0; i < rows_used && rc; i++) {
        HashTable *ht = rows[i];
        zval *cell;
        int j = 0;
        ZEND_HASH_FOREACH_VAL(ht, cell) {
            if (Z_TYPE_P(cell) != IS_STRING) {
                convert_to_string(cell);
            }
            if (!parse_cell_to(dtype, Z_STRVAL_P(cell), Z_STRLEN_P(cell),
                               dst + (i * cols + j) * itemsize,
                               i + (header ? 2 : 1), j + 1)) {
                rc = 0;
                break;
            }
            j++;
        } ZEND_HASH_FOREACH_END();
    }

    /* Release the HashTables (zend_array_release decrements refcount and frees
     * when it hits zero — the right pairing with php_fgetcsv's zend_new_array). */
    for (int i = 0; i < rows_used; i++) {
        zend_array_release(rows[i]);
    }
    efree(rows);

    if (!rc) {
        numphp_ndarray_free(out);
        return NULL;
    }
    return out;
}
