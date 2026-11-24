/* ksflate.h -- public C API for the ksflate deflate engine.
 *
 * ksflate produces raw DEFLATE data (RFC 1951). It does not add zlib or gzip
 * framing; use zlib's wrappers or the libksf shim for that.
 *
 * The engine is a whole-buffer, multi-pass optimizer, so each call compresses
 * one independent segment. To compress a large stream, split it into segments
 * and chain them: pass the previous segment's last <= KSFLATE_WINDOW_SIZE
 * uncompressed bytes as `dict`, and set `final_block` to 0 on every segment
 * except the last. A non-final segment ends on a byte boundary with an empty
 * stored sync marker, so the segments concatenate directly.
 *
 * Typical use:
 *
 *   size_t len;
 *   unsigned char *out =
 *       ksflate_compress(in, in_len, NULL, 0, 0, 1, &len);
 *   if (out == NULL) { ... }
 *   write(fd, out, len);
 *   ksflate_free(out);
 *
 * Calls are independent and may run concurrently: the engine has no mutable
 * global state. The same inputs always produce the same output bytes. This
 * implementation uses two optimization passes, the same configuration as
 * pigz-ks level 10.
 */
#ifndef KSFLATE_H
#define KSFLATE_H

#include <stddef.h>

#define KSFLATE_VERSION_MAJOR 1
#define KSFLATE_VERSION_MINOR 0
#define KSFLATE_VERSION_PATCH 0

/* Largest useful history, and the maximum DEFLATE back-reference distance. */
#define KSFLATE_WINDOW_SIZE 32768

#if defined(_WIN32) && defined(KSFLATE_SHARED)
#  ifdef KSFLATE_BUILDING
#    define KSFLATE_API __declspec(dllexport)
#  else
#    define KSFLATE_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__)
#  define KSFLATE_API __attribute__((visibility("default")))
#else
#  define KSFLATE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Compress `in_len` bytes at `in` into raw deflate.
 *
 * `dict` / `dict_len` are up to KSFLATE_WINDOW_SIZE bytes of uncompressed data
 * that precede `in` in the same deflate stream. Only the last window's worth is
 * used. Pass NULL / 0 for no history.
 *
 * `literal_only` selects Huffman-only mode (no LZ77 matches), equivalent to
 * zlib's Z_HUFFMAN_ONLY.
 *
 * `final_block` non-zero sets BFINAL on the last block, producing a complete
 * stream. Zero ends the segment with an empty stored sync marker so another
 * segment can follow.
 *
 * Returns a malloc'd buffer and stores its size in *out_len, or NULL on
 * allocation failure or if out_len is NULL. Release it with ksflate_free().
 */
KSFLATE_API unsigned char *ksflate_compress(const unsigned char *in,
                                            size_t in_len,
                                            const unsigned char *dict,
                                            size_t dict_len,
                                            int literal_only,
                                            int final_block,
                                            size_t *out_len);

/* Release a buffer returned by ksflate_compress(). NULL is accepted. */
KSFLATE_API void ksflate_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* KSFLATE_H */
