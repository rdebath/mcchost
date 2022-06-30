
#include <stddef.h>

#include "inifield.h"

#if INTERFACE
enum ini_type_t { ini_char, ini_int, ini_unsigned, ini_uint8,
    ini_bool, ini_array, ini_array437 };

enum ini_struct_t { ini_server_t, ini_map_info_t };

struct inifield_t {
    char * name;
    enum ini_type_t type;
    enum ini_struct_t ini_struct;
    int offset;
    union {
	void * void_val;
	int * int_val;
	unsigned int * uint_val;
	uint8_t * uint8_val;
	char * charp_val;
	int * bool_val;
    };
    int init_type_len;
};

#define INI_TYPECHECK(_t, _c, _v) ._t = &(((_c*)0x1000)->_v)
#define INI_PTR_TYPECHECK(_t, _c, _v) ._t = (((_c*)0x1000)->_v)

#define INI_INT(_n, _c, _v) {.name = #_n, .type = ini_int, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    INI_TYPECHECK(int_val, _c, _v), \
    }

#define INI_UINT(_n, _c, _v) {.name = #_n, .type = ini_unsigned, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    INI_TYPECHECK(uint_val, _c, _v), \
    }

#define INI_UINT8(_n, _c, _v) {.name = #_n, .type = ini_uint8, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    INI_TYPECHECK(uint8_val, _c, _v), \
    }

#define INI_BOOL(_n, _c, _v) {.name = #_n, .type = ini_bool, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    INI_TYPECHECK(bool_val, _c, _v), \
    }

#define INI_CHARP(_n, _c, _v) {.name = #_n, .type = ini_char, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    INI_TYPECHECK(charp_val, _c, _v), \
    }

#define INI_ARRAY(_n, _c, _v) {.name = #_n, .type = ini_array, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    .init_type_len = sizeof(ini_array), \
    INI_PTR_TYPECHECK(charp_val, _c, _v), \
    }

#define INI_ARRAY437(_n, _c, _v) {.name = #_n, .type = ini_array437, \
    .ini_struct = ini_##_c, .offset = offsetof(_c,_v), \
    .init_type_len = sizeof(ini_array), \
    INI_PTR_TYPECHECK(charp_val, _c, _v), \
    }

#endif

#ifdef TEST

inifield_t ini_fields[] =
{
    INI_ARRAY437(Software, server_t, software),
    INI_ARRAY437(Name, server_t, name),
    INI_ARRAY437(Motd, server_t, motd),
    INI_ARRAY437(Main, server_t, main_level),
    INI_ARRAY437(Salt, server_t, secret),

    INI_BOOL(Private, server_t, private),
    INI_BOOL(NoCPE, server_t, cpe_disabled),

    INI_UINT(CellsX, map_info_t, cells_x),
    INI_UINT(CellsY, map_info_t, cells_y),
    INI_UINT(CellsZ, map_info_t, cells_z),
    INI_UINT8(Weather, map_info_t, weather),
    INI_INT(ClickDistance, map_info_t, click_distance),

    {0}
};

#endif
