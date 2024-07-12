extern "C" void *hstream_new(char *infile, char *outfile);
extern "C" int hstream_del(void *stream);

extern "C" int hstream_next(void *stream);
extern "C" int hstream_type(void *stream);
extern "C" ssize_t hstream_size(void *stream);
extern "C" ssize_t hstream_get(void *stream, char *dest, size_t length);
extern "C" ssize_t hstream_set(void *stream, char *src, size_t length);
