
#if !defined(__GNUC__) || defined(__GNUC_STDC_INLINE__)
// STDC inline
#if defined(LIBINLINE)
#define INLINE_STDC_REDEF  /* Enable the duplicate declaration */
#endif
#define INLINE_EXTERN

#else
// GNU inline
#if !defined(LIBINLINE)
#define INLINE_EXTERN extern
#else
#define INLINE_EXTERN
#endif

#endif

#ifdef INLINE_STDC_REDEF
extern int E(int n, char * err);
#endif

INLINE_EXTERN inline
int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

#ifdef INLINE_STDC_REDEF
extern void cpy_nbstring(char *buf, char *str);
#endif

INLINE_EXTERN inline
void cpy_nbstring(char *buf, char *str)
{
    memcpy(buf, str, MB_STRLEN);
    for(int i = 0; i<MB_STRLEN; i++) if (buf[i] == 0) buf[i] = 0xFF;//CP437 nbs
    buf[MB_STRLEN] = 0;
    char * p = buf+MB_STRLEN;
    while (p>buf && p[-1] == ' ') *(--p) = 0;
}
