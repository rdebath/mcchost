/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
typedef struct MD5_CTX MD5_CTX;
typedef unsigned int UINT4;
struct MD5_CTX {
  UINT4 i[2];                   /* number of _bits_ handled mod 2^64 */
  UINT4 buf[4];                                    /* scratch buffer */
  unsigned char in[64];                              /* input buffer */
  unsigned char digest[16];     /* actual digest after MD5Final call */
};
void MD5Final(MD5_CTX *mdContext);
void MD5Update(MD5_CTX *mdContext,unsigned char *inBuf,unsigned int inLen);
void MD5Init(MD5_CTX *mdContext);
#define INTERFACE 0
