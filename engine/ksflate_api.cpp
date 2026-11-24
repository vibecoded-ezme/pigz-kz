/* ksflate_api.cpp -- implementation of the public C API in include/ksflate.h.
 *
 * The engine's optimization loop returns a byte-aligned raw deflate stream.
 * For a non-final segment an empty stored block is appended so the next
 * segment can start on a byte boundary; the caller's dictionary is passed as
 * history, which preserves cross-segment compression.
 */
#include "ksflate.h"

#include "block_splitter.hpp"
#include "strategy0_encoder.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

namespace {

// Level-10 configuration: two strategy-0 optimization passes give most of the
// ratio without the much longer convergence of higher pass counts.
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
// block's header. See the matching function in zlib/ksf.cpp.
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

extern "C" KSFLATE_API unsigned char*
ksflate_compress(const unsigned char* in, size_t in_len,
                 const unsigned char* dict, size_t dict_len, int literal_only,
                 int final_block, size_t* out_len) {
   if (out_len == NULL)
      return NULL;
   *out_len = 0;

   const bool is_final = final_block != 0;
   std::vector<std::uint8_t> out;

   if (in_len == 0) {
      if (is_final) {
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
      options.final_block = is_final;
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
         append_stored(out, in, in_len, is_final);
      } else {
         out = std::move(result.deflate);
         if (!is_final)
            append_sync_marker(out, result.end_bits);
      }
   }

   unsigned char* buffer =
      static_cast<unsigned char*>(std::malloc(out.empty() ? 1 : out.size()));
   if (buffer == NULL)
      return NULL;
   if (!out.empty())
      std::memcpy(buffer, out.data(), out.size());
   *out_len = out.size();
   return buffer;
}

extern "C" KSFLATE_API void ksflate_free(void* p) { std::free(p); }
