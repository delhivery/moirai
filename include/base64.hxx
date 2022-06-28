#ifndef MOIRAI_BASE64
#define MOIRAI_BASE64

#include "concepts.hxx"
#include <ranges>
#include <string>

static constexpr auto ALPHABET =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

template <sized_range_of<char> RangeT>
auto encode(RangeT inputBuffer, unsigned char padding = '=') -> std::string {
  std::string encoded = "";
  encoded.reserve(inputBuffer.size() / 3 + (inputBuffer.size() % 3 > 0) * 4);

  auto cursor = inputBuffer.begin();

  for (size_t idx = 0; idx < inputBuffer.size() / 3; ++idx) {
    long pos = (*cursor++) << 16;
    pos += (*cursor++) << 8;
    pos += (*cursor++);
    encoded.append(1, ALPHABET[(pos & 0x00FC0000) >> 18]);
    encoded.append(1, ALPHABET[(pos & 0x0003F000) >> 12]);
    encoded.append(1, ALPHABET[(pos & 0x00000FC0) >> 6]);
    encoded.append(1, ALPHABET[(pos & 0x0000003F)]);
  }

  switch (inputBuffer.size() % 3) {
  case 1: {
    long pos = (*cursor++) << 16;
    encoded.append(1, ALPHABET[(pos & 0x00FC0000) >> 18]);
    encoded.append(1, ALPHABET[(pos & 0x0003F000) >> 12]);
    encoded.append(2, padding);
  } break;
  case 2: {
    long pos = (*cursor++) << 16;
    pos += (*cursor++) << 8;
    encoded.append(1, ALPHABET[(pos & 0x00FC0000) >> 18]);
    encoded.append(1, ALPHABET[(pos & 0x0003F000) >> 12]);
    encoded.append(1, ALPHABET[(pos & 0x00000FC0) >> 6]);
    encoded.append(1, padding);
  } break;
  }
  return encoded;
}

template <sized_range_of<char> RangeT>
auto decode(const RangeT &input, unsigned char padding = '=') -> std::string {
  size_t nPadding = 0;

  if (input.size() % 4) {
    throw std::runtime_error("Invalid base64 string");
  }

  if (input.size()) {

    if (input[input.size() - 1] == padding) {
      ++nPadding;
    }

    if (input[input.size() - 2] == padding) {
      ++nPadding;
    }
  }

  std::string decoded{""};
  decoded.reserve(input.size() / 4 * 3 - nPadding);
  long word = 0;

  for (auto cursor = input.begin(); cursor != input.end();) {

    for (auto pos = 0; pos < 4; ++pos) {
      word <<= 6;

      if (*cursor >= 0x41 and *cursor <= 0x5A) {
        word |= *cursor - 0x41;
      } else if (*cursor >= 0x61 and *cursor <= 0x7A) {
        word |= *cursor - 0x47;
      } else if (*cursor >= 0x30 and *cursor <= 0x39) {
        word |= *cursor + 0x04;
      } else if (*cursor == 0x2B) {
        word |= 0x3E;
      } else if (*cursor == 0x2F) {
        word |= 0x3F;
      } else if (*cursor == padding) {
        switch (input.end() - cursor) {
        case 1:
          decoded.push_back((word >> 16) & 0x000000FF);
          decoded.push_back((word >> 8) & 0x000000FF);
          return decoded;
        case 2:
          decoded.push_back((word >> 10) & 0x000000FF);
          return decoded;
        default:
          throw std::runtime_error("Invalid padding in base64 input");
        }
      } else {
        throw std::runtime_error("Invalid character in base64 input");
      }
      ++cursor;
    }
    decoded.push_back((word >> 16) & 0x000000FF);
    decoded.push_back((word >> 8) & 0x000000FF);
    decoded.push_back(word & 0x000000FF);
  }
  return decoded;
}

#endif
