#ifndef U_SSTREAM_H
#define U_SSTREAM_H

/* string stream */
typedef struct U_SStream
{
	char *str;
	unsigned pos;
	unsigned len;
} U_SStream;

/* string stream */
void U_sstream_init(U_SStream *ss, void *str, unsigned size);
unsigned U_sstream_pos(const U_SStream *ss);
const char *U_sstream_str(const U_SStream *ss);
unsigned U_sstream_remaining(const U_SStream *ss);
int U_sstream_at_end(const U_SStream *ss);
int U_sstream_get_i32(U_SStream *ss, int base);
float U_sstream_get_f32(U_SStream *ss);
double U_sstream_get_f64(U_SStream *ss);
char U_sstream_peek_char(U_SStream *ss);
void U_sstream_skip_whitespace(U_SStream *ss);
const char *U_sstream_next_token(U_SStream *ss, const char *delim);
int U_sstream_starts_with(U_SStream *ss, const char *str);
void U_sstream_seek(U_SStream *ss, unsigned pos);
void U_sstream_put_str(U_SStream *ss, const char *str);
void U_sstream_put_i32(U_SStream *ss, long int num);
void U_sstream_put_u32(U_SStream *ss, unsigned long num);
void U_sstream_put_hex(U_SStream *ss, const void *data, unsigned size);

#endif /* U_SSTREAM_H */
