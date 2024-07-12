#include <fstream>
#include <memory>
#include <vector>

#include "h264.h"

#define NAL_UNIT_TYPE_I 5
#define NAL_UNIT_TYPE_MASK 0x1F

struct hstream {
  std::ifstream input;
  std::ofstream output;
  std::vector<char> unit;
};

void *hstream_new(char *infile, char *outfile) {
  hstream *st = new hstream;
  st->input = std::ifstream(infile, std::ios::in);
  st->output = std::ofstream(outfile, std::ios::out);
  return st;
}

int hstream_del(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st)
    return -1;

  delete st;
  return 0;
}

int hstream_next(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st || !st->input.is_open() || !st->output.is_open())
    return -1;

  // If the current NAL unit is not empty, append it to the output file
  if (!st->unit.empty()) {
    st->output.write(st->unit.data(), st->unit.size());
    st->unit.clear();
  }

  // Read the next NAL unit from the input file
  char c;
  bool found_nal_start = false;

  // Read the input file byte by byte
  while (st->input.get(c)) {
    st->unit.push_back(c);

    // Check for the NAL unit start code (0x000001 or 0x00000001)
    size_t n = st->unit.size();
    if (n >= 3 && st->unit[n - 3] == 0x00 && st->unit[n - 2] == 0x00 &&
        st->unit[n - 1] == 0x01) {
      found_nal_start = true;
      break;
    }
  }

  if (!found_nal_start)
    return -1; // End of file or no NAL unit found

  // Read the rest of the NAL unit
  while (st->input.get(c)) {
    st->unit.push_back(c);

    // Check for the next NAL unit start code
    size_t n = st->unit.size();
    if (n >= 3 && st->unit[n - 3] == 0x00 && st->unit[n - 2] == 0x00 &&
        st->unit[n - 1] == 0x01) {
      // Remove the start code of the next NAL unit from the current NAL unit
      st->unit.erase(st->unit.end() - 3, st->unit.end());
      st->input.seekg(-3, std::ios::cur);
      break;
    }
    if (n >= 4 && st->unit[n - 4] == 0x00 && st->unit[n - 3] == 0x00 &&
        st->unit[n - 2] == 0x00 && st->unit[n - 1] == 0x01) {
      // Remove the start code of the next NAL unit from the current NAL unit
      st->unit.erase(st->unit.end() - 4, st->unit.end());
      st->input.seekg(-4, std::ios::cur);
      break;
    }
  }

  return 0;
}

int hstream_type(void *stream) {
  hstream *st = (hstream *)stream;
  if (!st || st->unit.empty())
    return -1;

  // Find the NAL unit type from the first byte after the start code
  size_t nal_start_code_length = 3;
  if (st->unit.size() > 4 && st->unit[0] == 0x00 && st->unit[1] == 0x00 &&
      st->unit[2] == 0x00 && st->unit[3] == 0x01) {
    nal_start_code_length = 4;
  }

  if (st->unit.size() <= nal_start_code_length)
    return -1;

  unsigned char nal_header = st->unit[nal_start_code_length];
  unsigned char nal_unit_type = nal_header & NAL_UNIT_TYPE_MASK;

  return nal_unit_type;
}

int hstream_get(void *stream, char *dest, size_t length) {
  hstream *st = (hstream *)stream;
  if (!st || st->unit.empty() || !dest || length <= 0)
    return -1;

  // Determine the NAL start code length
  size_t nal_start_code_length = 3;
  if (st->unit.size() > 4 && st->unit[0] == 0x00 && st->unit[1] == 0x00 &&
      st->unit[2] == 0x00 && st->unit[3] == 0x01) {
    nal_start_code_length = 4;
  }

  // Preserve the nal_unit_type
  size_t header_length = nal_start_code_length + 1;

  // Check if there is at least one byte of payload data
  if (st->unit.size() <= header_length + 1)
    return -1;

  // Calculate the starting position of the payload
  size_t payload_start = header_length + 1;

  // Calculate the number of bytes to copy
  size_t payload_length = std::min(st->unit.size() - payload_start, length);

  // Copy the payload to the destination buffer
  auto start = st->unit.begin() + payload_start;
  auto stop = start + payload_length;
  std::copy(start, stop, dest);

  return payload_length;
}

int hstream_set(void *stream, char *src, size_t length) {
  hstream *st = (hstream *)stream;
  if (!st || st->unit.empty() || !src || length <= 0)
    return -1;

  // Determine the NAL start code length
  size_t nal_start_code_length = 3;
  if (st->unit.size() > 4 && st->unit[0] == 0x00 && st->unit[1] == 0x00 &&
      st->unit[2] == 0x00 && st->unit[3] == 0x01) {
    nal_start_code_length = 4;
  }

  // Preserve the nal_unit_type
  size_t header_length = nal_start_code_length + 1;

  // Ensure the provided length does not exceed the maximum allowed
  if (length > st->unit.capacity() - header_length)
    return -1;

  // Clear any existing payload data in the unit buffer
  st->unit.resize(header_length);

  // Insert the new payload into the unit buffer
  st->unit.insert(st->unit.end(), src, src + length);

  return 0;
}
