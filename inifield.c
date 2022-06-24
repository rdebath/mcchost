
#include "inifield.h"

#if INTERFACE
enum ini_type {ini_char, ini_int, ini_array, ini_array437 };

struct inifield_t {
    char * name;
    enum ini_type type;
    union {
	void * void_val;
	int * int_val;
	char ** charp_val;
    };
    int init_type_len;
};

#define INI_INT(_v) .type = ini_int, .int_val = (_v),
#define INI_CHARP(_v) .type = ini_char, .charp_val = (_v),
#define INI_ARRAY(_v) .type = ini_array, .charp_val = (_v), .init_type_len = sizeof(ini_array),
#define INI_ARRAY437(_v) .type = ini_array437, .charp_val = (_v), .init_type_len = sizeof(ini_array),

#endif
