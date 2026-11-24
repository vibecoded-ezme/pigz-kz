/* ksflate_engine.cpp -- level-10 backend for the pigz fork.
 *
 * The engine's optimization loop returns a byte-aligned raw deflate stream.
 * For a non-final chunk we append an empty stored block so the next chunk can
 * start on a byte boundary; the engine is given the previous chunk's tail as
 * history, which preserves pigz's cross-chunk compression.
 */
#include "ksflate_engine.h"

#include "block_splitter.hpp"
#include "strategy0_encoder.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

namespace {

// Number of optimization passes for level 10. Two passes is the same setting
// the zlib shim uses for its level 6, and gives most of the ratio without the
// much longer convergence of levels 8 and 9.
constexpr std::size_t kPasses = 2;

void append_stored(std::vector<std::uint8_t>& out, const std::uint8_t* data,
                   std::size_t size, bool final_block) {
   do {
      const std::size_t chunk = size > 0xffff ? 0xffff : size;
      const bool last = chunk == size;
      const std::uint16_t nlen = static_cast<std::uint16_t>(~chunk);
      out.push_back((final_block && last) ? 0x01 : 0x00); // BFINAL, BTYPE = 00
      out.push_back(static_cast<std::uint8_t>(chunk & 0xff));
      out.push_back(static_cast<std::uint8_t>((chunk >> 8) & 0xff));
      out.push_back(static_cast<std::uint8_t>(nlen & 0xff));
      out.push_back(static_cast<std::uint8_t>((nlen >> 8) & 0xff));
      if (chunk != 0) {
         out.insert(out.end(), data, data + chunk);
         data += chunk;
      }
      size -= chunk;
   } while (size > 0);
}

// Append the empty stored block that marks a sync point. The engine pads its
// final byte with zeros, so those padding bits are the first bits of this
// block's header. See the matching function in ksflate/zlib/ksf.cpp.
void append_sync_marker(std::vector<std::uint8_t>& out, std::uint8_t end_bits) {
   if (end_bits >= 1 && end_bits <= 5) {
      static const std::uint8_t marker[4] = {0x00, 0x00, 0xff, 0xff};
      out.insert(out.end(), marker, marker + 4);
   } else {
      static const std::uint8_t marker[5] = {0x00, 0x00, 0x00, 0xff, 0xff};
      out.insert(out.end(), marker, marker + 5);
   }
}

} // namespace

extern "C" unsigned char *ksflate_compress(const unsigned char *in,
                                          size_t in_len,
                                          const unsigned char *dict,
                                          size_t dict_len, int literal_only,
                                          int final, size_t *out_len) {
   if (out_len == NULL)
      return NULL;
   *out_len = 0;

   const bool final_block = final != 0;
   std::vector<std::uint8_t> out;

   if (in_len == 0) {
      if (final_block) {
         // Fixed-Huffman block holding only EOB: 10 bits, two bytes.
         out = {0x03, 0x00};
      } else {
         // Empty stored block, non-final.
         append_stored(out, NULL, 0, false);
      }
   } else {
      kzip::EncoderOptions options;
      options.strategy = literal_only ? 3 : 0;
      options.literal_only = literal_only != 0;
      options.max_passes = kPasses;
      options.final_block = final_block;
      if (dict_len != 0 && dict != NULL)
         options.history = std::span<const std::uint8_t>(dict, dict_len);

      const std::vector<std::size_t> ends =
         kzip::choose_block_ends(std::span<const std::uint8_t>(in, in_len),
                                 kzip::SplitOptions{});
      kzip::EncoderResult result =
         kzip::encode_strategy0(std::span<const std::uint8_t>(in, in_len),
                                ends, options);

      const std::size_t blocks =
         in_len == 0 ? 1 : (in_len + 0xffff) / 0x10000;
      const std::size_t stored_size = in_len + blocks * 5;
      if (result.stored || result.deflate.size() > stored_size) {
         out.reserve(stored_size + 8);
         append_stored(out, in, in_len, final_block);
      } else {
         out = std::move(result.deflate);
         if (!final_block)
            append_sync_marker(out, result.end_bits);
      }
   }

   unsigned char *buffer =
      static_cast<unsigned char *>(std::malloc(out.size() == 0 ? 1 : out.size()));
   if (buffer == NULL)
      return NULL;
   if (!out.empty())
      std::memcpy(buffer, out.data(), out.size());
   *out_len = out.size();
   return buffer;
}

extern "C" void ksflate_free(void *p) { std::free(p); }
