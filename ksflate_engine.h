/* ksflate_engine.h -- C interface to the ksflate deflate engine.
 *
 * This is the level-10 backend for the pigz fork. It produces raw deflate
 * (RFC 1951) blocks only; the caller supplies the gzip/zlib framing.
 */
#ifndef KSFLATE_ENGINE_H
#define KSFLATE_ENGINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compress `in_len` bytes at `in` into raw deflate.
 *
 * `dict`/`dict_len` are up to 32768 bytes of history that precede `in` in the
 * same deflate stream. Pass NULL/0 for no history. `literal_only` selects the
 * engine's Huffman-only mode (pigz -H). `final` non-zero emits BFINAL on the
 * last block; zero ends on a byte boundary with an empty stored sync marker so
 * another segment can follow.
 *
 * Returns a malloc'd buffer and stores its length in *out_len, or NULL on
 * allocation failure. Release it with ksflate_free().
 */
unsigned char *ksflate_compress(const unsigned char *in, size_t in_len,
                               const unsigned char *dict, size_t dict_len,
                               int literal_only, int final, size_t *out_len);

void ksflate_free(void *p);

#ifdef __cplusplus
}
#endif

#endif
