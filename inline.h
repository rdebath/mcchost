
#ifdef LIBINLINE
extern int E(int n, char * err);
#endif
inline int E(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }

#ifdef LIBINLINE
extern void cpy_nbstring(char *buf, char *str);
#endif
inline void cpy_nbstring(char *buf, char *str)
{
    memcpy(buf, str, MB_STRLEN);
    for(int i = 0; i<MB_STRLEN; i++) if (buf[i] == 0) buf[i] = 0xFF;//CP437 nbs
    buf[MB_STRLEN] = 0;
    char * p = buf+MB_STRLEN;
    while (p>buf && p[-1] == ' ') *(--p) = 0;
}

