#include "range.h"
#include "buffer.h"
#include "global.h"

struct settings global_settings = { 8, 0, 0, 0 };

buffer_t *global_buffer;

int global_x     = 0, global_y = 0;
int global_top   = 0;
int global_max_x = 0, global_max_y = 0;
