#ifndef U_BASE64_H
#define U_BASE64_H

int U_base64_decode(const char *in, unsigned inlen, unsigned char *out, unsigned outlen);
int U_base64_encode(const unsigned char *in, unsigned inlen, unsigned char *out, unsigned outlen);

#endif /* U_BASE64_H */
