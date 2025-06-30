#include "../core/types.h"

// No clue what's happening here but compiler really dislikes "signed int"
#undef int32
#define int32 int32_t
