
#include "static_data.h"



blockdef_t default_blocks[66] = {
/*  0 */ { .name="Air", 0, 1, 0, 0, 16, 4, 1.0, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  1 */ { .name="Stone", 2, 0, 4, 0, 16, 0, 1.0, {1, 1, 1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  2 */ { .name="Grass", 2, 0, 3, 0, 16, 0, 1.0, {0, 2, 3, 3, 3, 3}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  3 */ { .name="Dirt", 2, 0, 2, 0, 16, 0, 1.0, {2, 2, 2, 2, 2, 2}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  4 */ { .name="Cobblestone", 2, 0, 4, 0, 16, 0, 1.0, {16, 16, 16, 16, 16, 16}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  5 */ { .name="Wood", 2, 0, 1, 0, 16, 0, 1.0, {4, 4, 4, 4, 4, 4}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  6 */ { .name="Sapling", 0, 1, 0, 0, 0, 1, 1.0, {15, 15, 15, 15, 15, 15}, {0, 0, 0, 0}, {3, 0, 3, 13, 15, 13}},
/*  7 */ { .name="Bedrock", 2, 0, 4, 0, 16, 0, 1.0, {17, 17, 17, 17, 17, 17}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/*  8 */ { .name="Water", 1, 0, 0, 0, 16, 3, 1.0, {14, 14, 14, 14, 14, 14}, {11, 5, 5, 51}, {0, 0, 0, 16, 16, 16}},
/*  9 */ { .name="Still water", 1, 0, 0, 0, 16, 3, 1.0, {14, 14, 14, 14, 14, 14}, {11, 5, 5, 51}, {0, 0, 0, 16, 16, 16}},
/* 10 */ { .name="Lava", 1, 0, 0, 1, 16, 1, 1.0, {30, 30, 30, 30, 30, 30}, {229, 153, 25, 0}, {0, 0, 0, 16, 16, 16}},
/* 11 */ { .name="Still lava", 1, 0, 0, 1, 16, 1, 1.0, {30, 30, 30, 30, 30, 30}, {229, 153, 25, 0}, {0, 0, 0, 16, 16, 16}},
/* 12 */ { .name="Sand", 2, 0, 8, 0, 16, 0, 1.0, {18, 18, 18, 18, 18, 18}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 13 */ { .name="Gravel", 2, 0, 2, 0, 16, 0, 1.0, {19, 19, 19, 19, 19, 19}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 14 */ { .name="Gold ore", 2, 0, 4, 0, 16, 0, 1.0, {32, 32, 32, 32, 32, 32}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 15 */ { .name="Iron ore", 2, 0, 4, 0, 16, 0, 1.0, {33, 33, 33, 33, 33, 33}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 16 */ { .name="Coal ore", 2, 0, 4, 0, 16, 0, 1.0, {34, 34, 34, 34, 34, 34}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 17 */ { .name="Log", 2, 0, 1, 0, 16, 0, 1.0, {21, 21, 20, 20, 20, 20}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 18 */ { .name="Leaves", 2, 1, 3, 0, 16, 2, 1.0, {22, 22, 22, 22, 22, 22}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 19 */ { .name="Sponge", 2, 0, 3, 0, 16, 0, 1.0, {48, 48, 48, 48, 48, 48}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 20 */ { .name="Glass", 2, 1, 4, 0, 16, 1, 1.0, {49, 49, 49, 49, 49, 49}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 21 */ { .name="Red", 2, 0, 7, 0, 16, 0, 1.0, {64, 64, 64, 64, 64, 64}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 22 */ { .name="Orange", 2, 0, 7, 0, 16, 0, 1.0, {65, 65, 65, 65, 65, 65}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 23 */ { .name="Yellow", 2, 0, 7, 0, 16, 0, 1.0, {66, 66, 66, 66, 66, 66}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 24 */ { .name="Lime", 2, 0, 7, 0, 16, 0, 1.0, {67, 67, 67, 67, 67, 67}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 25 */ { .name="Green", 2, 0, 7, 0, 16, 0, 1.0, {68, 68, 68, 68, 68, 68}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 26 */ { .name="Teal", 2, 0, 7, 0, 16, 0, 1.0, {69, 69, 69, 69, 69, 69}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 27 */ { .name="Aqua", 2, 0, 7, 0, 16, 0, 1.0, {70, 70, 70, 70, 70, 70}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 28 */ { .name="Cyan", 2, 0, 7, 0, 16, 0, 1.0, {71, 71, 71, 71, 71, 71}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 29 */ { .name="Blue", 2, 0, 7, 0, 16, 0, 1.0, {72, 72, 72, 72, 72, 72}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 30 */ { .name="Indigo", 2, 0, 7, 0, 16, 0, 1.0, {73, 73, 73, 73, 73, 73}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 31 */ { .name="Violet", 2, 0, 7, 0, 16, 0, 1.0, {74, 74, 74, 74, 74, 74}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 32 */ { .name="Magenta", 2, 0, 7, 0, 16, 0, 1.0, {75, 75, 75, 75, 75, 75}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 33 */ { .name="Pink", 2, 0, 7, 0, 16, 0, 1.0, {76, 76, 76, 76, 76, 76}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 34 */ { .name="Black", 2, 0, 7, 0, 16, 0, 1.0, {77, 77, 77, 77, 77, 77}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 35 */ { .name="Gray", 2, 0, 7, 0, 16, 0, 1.0, {78, 78, 78, 78, 78, 78}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 36 */ { .name="White", 2, 0, 7, 0, 16, 0, 1.0, {79, 79, 79, 79, 79, 79}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 37 */ { .name="Dandelion", 0, 1, 0, 0, 0, 1, 1.0, {13, 13, 13, 13, 13, 13}, {0, 0, 0, 0}, {5, 0, 5, 10, 8, 10}},
/* 38 */ { .name="Rose", 0, 1, 0, 0, 0, 1, 1.0, {12, 12, 12, 12, 12, 12}, {0, 0, 0, 0}, {5, 0, 5, 10, 11, 10}},
/* 39 */ { .name="Brown mushroom", 0, 1, 0, 0, 0, 1, 1.0, {29, 29, 29, 29, 29, 29}, {0, 0, 0, 0}, {5, 0, 5, 10, 6, 10}},
/* 40 */ { .name="Red mushroom", 0, 1, 0, 0, 0, 1, 1.0, {28, 28, 28, 28, 28, 28}, {0, 0, 0, 0}, {5, 0, 5, 10, 6, 10}},
/* 41 */ { .name="Gold", 2, 0, 5, 0, 16, 0, 1.0, {24, 56, 40, 40, 40, 40}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 42 */ { .name="Iron", 2, 0, 5, 0, 16, 0, 1.0, {23, 55, 39, 39, 39, 39}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 43 */ { .name="Double slab", 2, 0, 4, 0, 16, 0, 1.0, {6, 6, 5, 5, 5, 5}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 44 */ { .name="Slab", 2, 0, 4, 0, 8, 0, 1.0, {6, 6, 5, 5, 5, 5}, {0, 0, 0, 0}, {0, 0, 0, 16, 8, 16}},
/* 45 */ { .name="Brick", 2, 0, 4, 0, 16, 0, 1.0, {7, 7, 7, 7, 7, 7}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 46 */ { .name="TNT", 2, 0, 3, 0, 16, 0, 1.0, {9, 10, 8, 8, 8, 8}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 47 */ { .name="Bookshelf", 2, 0, 1, 0, 16, 0, 1.0, {4, 4, 35, 35, 35, 35}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 48 */ { .name="Mossy rocks", 2, 0, 4, 0, 16, 0, 1.0, {36, 36, 36, 36, 36, 36}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 49 */ { .name="Obsidian", 2, 0, 4, 0, 16, 0, 1.0, {37, 37, 37, 37, 37, 37}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},

/* Below here not available in pure classic */
/* 50 */ { .name="Cobblestone slab", 2, 0, 4, 0, 8, 0, 1.0, {16, 16, 16, 16, 16, 16}, {0, 0, 0, 0}, {0, 0, 0, 16, 8, 16}},
/* 51 */ { .name="Rope", 7, 1, 7, 0, 0, 1, 1.0, {11, 11, 11, 11, 11, 11}, {0, 0, 0, 0}, {2, 0, 2, 13, 16, 13}},
/* 52 */ { .name="Sandstone", 2, 0, 4, 0, 16, 0, 1.0, {25, 57, 41, 41, 41, 41}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 53 */ { .name="Snow", 0, 0, 9, 0, 2, 1, 1.0, {50, 50, 50, 50, 50, 50}, {0, 0, 0, 0}, {0, 0, 0, 16, 2, 16}},
/* 54 */ { .name="Fire", 0, 1, 0, 1, 0, 1, 1.0, {38, 38, 38, 38, 38, 38}, {0, 0, 0, 0}, {2, 0, 2, 13, 13, 13}},
/* 55 */ { .name="Light pink", 2, 0, 7, 0, 16, 0, 1.0, {80, 80, 80, 80, 80, 80}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 56 */ { .name="Forest green", 2, 0, 7, 0, 16, 0, 1.0, {81, 81, 81, 81, 81, 81}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 57 */ { .name="Brown", 2, 0, 7, 0, 16, 0, 1.0, {82, 82, 82, 82, 82, 82}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 58 */ { .name="Deep blue", 2, 0, 7, 0, 16, 0, 1.0, {83, 83, 83, 83, 83, 83}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 59 */ { .name="Turquoise", 2, 0, 7, 0, 16, 0, 1.0, {84, 84, 84, 84, 84, 84}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 60 */ { .name="Ice", 2, 0, 4, 0, 16, 3, 1.0, {51, 51, 51, 51, 51, 51}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 61 */ { .name="Ceramic tile", 2, 0, 4, 0, 16, 0, 1.0, {54, 54, 54, 54, 54, 54}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 62 */ { .name="Magma", 2, 0, 4, 1, 16, 0, 1.0, {86, 86, 86, 86, 86, 86}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 63 */ { .name="Pillar", 2, 0, 4, 0, 16, 0, 1.0, {26, 58, 42, 42, 42, 42}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 64 */ { .name="Crate", 2, 0, 1, 0, 16, 0, 1.0, {53, 53, 53, 53, 53, 53}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
/* 65 */ { .name="Stone brick", 2, 0, 4, 0, 16, 0, 1.0, {52, 52, 52, 52, 52, 52}, {0, 0, 0, 0}, {0, 0, 0, 16, 16, 16}},
};
