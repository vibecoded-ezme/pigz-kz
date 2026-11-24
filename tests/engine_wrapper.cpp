/* Direct test of the ksflate_compress() wrapper used by level 10.
 *
 * The wrapper owns the chunk framing that the engine does not: it appends the
 * empty stored sync marker after a non-final segment, sets BFINAL on the last
 * one, and passes the caller's history through to the parser. This test
 * concatenates a non-final segment and a final segment, primed with the tail
 * of the first, and checks the result with zlib's raw inflate.
 */
#include "ksflate.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

namespace {

std::vector<std::uint8_t> make_data(std::size_t size, std::uint32_t seed) {
   std::vector<std::uint8_t> data(size);
   std::uint32_t state = seed;
   for (std::size_t i = 0; i < size; ++i) {
      state = state * 1664525u + 1013904223u;
      data[i] = (i % 13 == 0) ? static_cast<std::uint8_t>(state >> 24)
                              : static_cast<std::uint8_t>((i / 5 + seed) & 0x3f);
   }
   return data;
}

bool inflate_all(const std::vector<std::uint8_t>& packed,
                 const std::vector<std::uint8_t>& dictionary,
                 std::vector<std::uint8_t>& out) {
   z_stream strm{};
   if (inflateInit2(&strm, -15) != Z_OK)
      return false;
   if (!dictionary.empty() &&
       inflateSetDictionary(&strm, dictionary.data(),
                            static_cast<unsigned>(dictionary.size())) != Z_OK) {
      inflateEnd(&strm);
      return false;
   }
   out.assign(2 * 1024 * 1024, 0);
   strm.next_in = const_cast<Bytef *>(packed.data());
   strm.avail_in = static_cast<unsigned>(packed.size());
   strm.next_out = out.data();
   strm.avail_out = static_cast<unsigned>(out.size());
   const int ret = inflate(&strm, Z_FINISH);
   out.resize(out.size() - strm.avail_out);
   inflateEnd(&strm);
   return ret == Z_STREAM_END;
}

std::vector<std::uint8_t> compress(std::span<const std::uint8_t> input,
                                   std::span<const std::uint8_t> dict,
                                   int literal_only, int final) {
   size_t out_len = 0;
   unsigned char *packed =
      ksflate_compress(input.data(), input.size(),
                       dict.empty() ? nullptr : dict.data(), dict.size(),
                       literal_only, final, &out_len);
   std::vector<std::uint8_t> result;
   if (packed != nullptr) {
      result.assign(packed, packed + out_len);
      ksflate_free(packed);
   }
   return result;
}

std::vector<std::uint8_t> tail(std::span<const std::uint8_t> data,
                               std::size_t count) {
   if (count > data.size())
      count = data.size();
   return std::vector<std::uint8_t>(data.end() - count, data.end());
}

} // namespace

int main() {
   const std::vector<std::uint8_t> dictionary = make_data(32768, 0x1234);
   std::vector<std::uint8_t> body = make_data(300000, 0x5678);
   for (std::size_t i = 0; i < 20000; ++i)
      body[i] = dictionary[7000 + (i % 25000)];

   const std::size_t split = body.size() / 2;
   const std::span<const std::uint8_t> first_input(body.data(), split);
   const std::span<const std::uint8_t> second_input(body.data() + split,
                                                    body.size() - split);

   std::vector<std::uint8_t> first =
      compress(first_input, dictionary, 0, 0);
   if (first.empty()) {
      std::printf("wrapper: first segment failed\n");
      return 1;
   }

   std::vector<std::uint8_t> chained = dictionary;
   chained.insert(chained.end(), body.begin(), body.begin() + split);
   std::vector<std::uint8_t> second =
      compress(second_input, tail(chained, 32768), 0, 1);
   if (second.empty()) {
      std::printf("wrapper: second segment failed\n");
      return 1;
   }

   std::vector<std::uint8_t> joined = first;
   joined.insert(joined.end(), second.begin(), second.end());

   std::vector<std::uint8_t> round;
   if (!inflate_all(joined, dictionary, round)) {
      std::printf("wrapper: chained inflate failed\n");
      return 1;
   }
   if (round != body) {
      std::printf("wrapper: chained round-trip mismatch (%zu vs %zu)\n",
                  round.size(), body.size());
      return 1;
   }

   // Literal-only mode must also produce a valid final segment.
   std::vector<std::uint8_t> literal = compress(body, {}, 1, 1);
   if (literal.empty() || !inflate_all(literal, {}, round) || round != body) {
      std::printf("wrapper: literal-only round-trip failed\n");
      return 1;
   }

   std::printf("wrapper ok: chained=%zu literal=%zu\n", joined.size(),
               literal.size());
   return 0;
}
