#include "h264.h"

#define NAL_UNIT_TYPE_I 5
#define NAL_UNIT_TYPE_MASK 0x1F

struct State {
  bool valid;

  FILE *infile;

  uint8_t *buf;
  size_t buf_size;

  // h264_stream_t *h;
} st = {.valid = false};

// replace an I frame at a given index with a certain payload
// true on success, false on failure
bool replace_i_frame(int index, char *payload, int length) {
  if (!st.valid)
    return false;

  for (size_t i = 0; i < st.buf_size - 4; i++) {
    // Look for start code prefix (0x000001 or 0x00000001)
    if (st.buf[i] == 0x00 && st.buf[i + 1] == 0x00 && st.buf[i + 2] == 0x01) {
      uint8_t nal_unit_type = st.buf[i + 3] & 0x1F;

      if (nal_unit_type == NAL_UNIT_TYPE_I) {
        // printf("Found I-frame at offset %zu\n", i);

        // Find the end of the NAL unit (next start code prefix)
        size_t nal_end = i + 3;
        while (nal_end < st.buf_size - 4) {
          if (st.buf[nal_end] == 0x00 && st.buf[nal_end + 1] == 0x00 &&
              (st.buf[nal_end + 2] == 0x01 ||
               (st.buf[nal_end + 2] == 0x00 && st.buf[nal_end + 3] == 0x01))) {
            break;
          }
          nal_end++;
        }

        // Replace the I-frame payload with the custom payload
        size_t nal_length = nal_end - (i + 3);
        if (length <= nal_length) {
          memcpy(&st.buf[i + 3], payload, length);
          // Fill remaining space with zeros if the payload is smaller
          if (length < nal_length) {
            memset(&st.buf[i + 3 + length], 0, nal_length - length);
          }
        } else {
          printf("Payload is too large to fit in the NAL unit.\n");
        }
      }
    }
  }

  return true;
}

// find the frame number of the i-th I frame
// returns -1 if not initialized, 0 if # of I frames < number, index otherwise
int i_frame_frame_num(int number) {
  if (!st.valid)
    return -1;

  int frame_count = 0;
  size_t i = 0;

  while (i < st.buf_size - 4) {
    // Look for the start code prefix (0x000001 or 0x00000001)
    if (st.buf[i] == 0x00 && st.buf[i + 1] == 0x00 &&
        ((st.buf[i + 2] == 0x01) ||
         (st.buf[i + 2] == 0x00 && st.buf[i + 3] == 0x01))) {
      size_t nal_start = (st.buf[i + 2] == 0x01) ? (i + 3) : (i + 4);
      uint8_t nal_unit_type = st.buf[nal_start] & NAL_UNIT_TYPE_MASK;

      // Check if the NAL unit is a coded slice of a non-IDR picture (P-frame)
      // or a coded slice of an IDR picture (I-frame)
      if (nal_unit_type >= 1 && nal_unit_type <= 5) {
        frame_count++;
        if (nal_unit_type == NAL_UNIT_TYPE_I)
          if (number-- == 0)
            return frame_count;
      }

      // Move to the next potential start code
      i = nal_start;
    } else {
      i++;
    }
  }

  return 0;
}

int count_i_frames() {
  if (!st.valid)
    return -1;

  int frame_count = 0;
  size_t i = 0;

  while (i < st.buf_size - 4) {
    // Look for the start code prefix (0x000001 or 0x00000001)
    if (st.buf[i] == 0x00 && st.buf[i + 1] == 0x00 &&
        ((st.buf[i + 2] == 0x01) ||
         (st.buf[i + 2] == 0x00 && st.buf[i + 3] == 0x01))) {
      size_t nal_start = (st.buf[i + 2] == 0x01) ? (i + 3) : (i + 4);
      uint8_t nal_unit_type = st.buf[nal_start] & NAL_UNIT_TYPE_MASK;

      // Check if the NAL unit is a coded slice of a non-IDR picture (P-frame)
      // or a coded slice of an IDR picture (I-frame)
      if (nal_unit_type == NAL_UNIT_TYPE_I)
        frame_count++;

      // Move to the next potential start code
      i = nal_start;
    } else {
      i++;
    }
  }

  return frame_count;
}

int count_frames() {
  if (!st.valid)
    return -1;

  int frame_count = 0;
  size_t i = 0;

  while (i < st.buf_size - 4) {
    // Look for the start code prefix (0x000001 or 0x00000001)
    if (st.buf[i] == 0x00 && st.buf[i + 1] == 0x00 &&
        ((st.buf[i + 2] == 0x01) ||
         (st.buf[i + 2] == 0x00 && st.buf[i + 3] == 0x01))) {
      size_t nal_start = (st.buf[i + 2] == 0x01) ? (i + 3) : (i + 4);
      uint8_t nal_unit_type = st.buf[nal_start] & NAL_UNIT_TYPE_MASK;

      // Check if the NAL unit is a coded slice of a non-IDR picture (P-frame)
      // or a coded slice of an IDR picture (I-frame)
      if (nal_unit_type >= 1 && nal_unit_type <= 5) {
        frame_count++;
      }

      // Move to the next potential start code
      i = nal_start;
    } else {
      i++;
    }
  }

  return frame_count;
}

// write bitstream to a file
// true on success, false on failure
bool write(char *outfile) {
  if (!st.valid)
    return false;

  FILE *file = fopen(outfile, "wb");
  if (file == NULL)
    return false;

  size_t write_size = fwrite(st.buf, 1, st.buf_size, file);
  if (write_size != st.buf_size)
    return false;

  fclose(file);
  return true;
}

// load h264 bitstream for manipulation
// true on success, false on failure
bool load() {
  if (!st.valid)
    return false;

  // Read the file into the buffer
  size_t read_size = fread(st.buf, 1, st.buf_size, st.infile);
  if (read_size != st.buf_size)
    return false;

  return true;
}

// initialize state to prepare for loading
// true on success, false on failure
bool init(char *infile) {
  if (st.valid)
    return false;

  st.infile = fopen(infile, "rb");
  if (st.infile == NULL)
    return false;

  // Seek to the end of the file to determine its size
  fseek(st.infile, 0, SEEK_END);
  st.buf_size = ftell(st.infile);
  rewind(st.infile);

  st.buf = malloc(st.buf_size);
  if (st.buf == NULL) {
    fclose(st.infile);
    st.infile = NULL;
    return false;
  }

  // st.h = h264_new();
  // if (st.h == NULL) {
  //   free(st.buf);
  //   st.buf = NULL;
  //   fclose(st.infile);
  //   st.infile = NULL;
  //   return false;
  // }

  st.valid = true;

  return true;
}

// reset state
// true on success, false on failure
bool close() {
  if (!st.valid)
    return false;
  st.valid = false;

  fclose(st.infile);
  st.infile = NULL;
  st.buf_size = 0;

  free(st.buf);
  st.buf = NULL;

  // h264_free(st.h);
  // st.h = NULL;

  return true;
}
