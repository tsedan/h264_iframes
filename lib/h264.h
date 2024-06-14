#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "../h264bitstream/h264_stream.h"

extern bool replace_i_frame(int index, char *payload, int length);

extern bool init(char *infile);
extern bool load();
extern bool write();
extern bool close();
