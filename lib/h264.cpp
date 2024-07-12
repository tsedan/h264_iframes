#include <fstream>
#include <memory>
#include <vector>

#include "h264.h"

#define NAL_UNIT_TYPE_I 5
#define NAL_UNIT_TYPE_MASK 0x1F

struct hstream {
  std::ifstream input;
  std::ofstream output;

  std::vector<char> head, body;
};

void *hstream_new(char *infile, char *outfile) {
  hstream *st = new hstream;
  st->input = std::ifstream(infile, std::ios::in | std::ios::binary);
  st->output = std::ofstream(outfile, std::ios::out | std::ios::binary);
  return st;
}

int hstream_del(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st)
    return -1;

  delete st;
  return 0;
}

std::vector<char> emul_prev_enc(std::vector<char> &dec) {
  std::vector<char> ret;

  int zero_count = 0;
  for (char c : dec) {
    if (c >= 0x01 && c <= 0x03) {
      if (zero_count == 2) {
        ret.push_back(0x03);
        zero_count = 0;
      }
    } else if (c == 0x00) {
      if (zero_count == 2) {
        ret.push_back(0x03);
        zero_count = 1;
      } else {
        zero_count = std::min(zero_count + 1, 2);
      }
    } else {
      zero_count = 0;
    }

    ret.push_back(c);
  }

  return ret;
}

std::vector<char> emul_prev_dec(std::vector<char> &enc) {
  std::vector<char> ret;

  int zero_count = 0;
  for (char c : enc) {
    if (c == 0x03) {
      if (zero_count == 2) {
        zero_count = 0;
        continue;
      }
    } else if (c == 0) {
      zero_count = std::min(zero_count + 1, 2);
    } else {
      zero_count = 0;
    }

    ret.push_back(c);
  }

  return ret;
}

int hstream_next(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st || !st->input.is_open() || !st->output.is_open())
    return -1;

  // If the current NAL unit is not empty, append it to the output file
  if (!st->head.empty()) {
    std::vector<char> encoded = emul_prev_enc(st->body);

    st->output.write(st->head.data(), st->head.size());
    st->output.write(encoded.data(), encoded.size());

    st->head.clear();
    st->body.clear();
  }

  // Read the next NAL unit from the input file
  char c;
  bool found_nal_start = false;

  // Read the input file byte by byte
  while (st->input.get(c)) {
    st->head.push_back(c);

    if (found_nal_start)
      break;

    // Check for the NAL unit start code (0x000001 or 0x00000001)
    size_t n = st->head.size();
    if (n >= 3 && st->head[n - 3] == 0x00 && st->head[n - 2] == 0x00 &&
        st->head[n - 1] == 0x01) {
      found_nal_start = true;
    }
  }

  if (!found_nal_start)
    return -1; // End of file or no NAL unit found

  std::vector<char> payload;

  // Read the rest of the NAL unit
  while (st->input.get(c)) {
    payload.push_back(c);

    // Check for the next NAL unit start code
    size_t n = payload.size();
    if (n >= 3 && payload[n - 3] == 0x00 && payload[n - 2] == 0x00 &&
        payload[n - 1] == 0x01) {
      // Remove the start code of the next NAL unit from the current NAL unit
      payload.erase(payload.end() - 3, payload.end());
      st->input.seekg(-3, std::ios::cur);
      break;
    }
    if (n >= 4 && payload[n - 4] == 0x00 && payload[n - 3] == 0x00 &&
        payload[n - 2] == 0x00 && payload[n - 1] == 0x01) {
      // Remove the start code of the next NAL unit from the current NAL unit
      payload.erase(payload.end() - 4, payload.end());
      st->input.seekg(-4, std::ios::cur);
      break;
    }
  }

  st->body = emul_prev_dec(payload);

  return 0;
}

int hstream_type(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st || st->head.empty())
    return -1;

  // Find the NAL unit type from the first byte after the start code
  size_t nal_start_code_length = 3;
  if (st->head.size() > 4 && st->head[0] == 0x00 && st->head[1] == 0x00 &&
      st->head[2] == 0x00 && st->head[3] == 0x01) {
    nal_start_code_length = 4;
  }

  if (st->head.size() <= nal_start_code_length)
    return -1;

  unsigned char nal_header = st->head[nal_start_code_length];
  unsigned char nal_unit_type = nal_header & NAL_UNIT_TYPE_MASK;

  return nal_unit_type;
}

ssize_t hstream_size(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st || st->head.empty())
    return -1;

  return st->body.size();
}

ssize_t hstream_get(void *stream, char *dest, size_t length) {
  hstream *st = (hstream *)stream;
  if (!st || st->body.empty() || !dest || length <= 0)
    return -1;

  // Calculate the number of bytes to copy
  size_t amount = std::min(st->body.size(), length);

  // Copy the payload to the destination buffer
  std::copy(st->body.begin(), st->body.begin() + amount, dest);

  return amount;
}

ssize_t hstream_set(void *stream, char *src, size_t length) {
  hstream *st = (hstream *)stream;
  if (!st || st->body.empty() || !src || length <= 0)
    return -1;

  // Clear any existing payload data in the unit buffer
  st->body.clear();

  // Insert the new payload into the unit buffer
  st->body.insert(st->body.end(), src, src + length);

  return length;
}
