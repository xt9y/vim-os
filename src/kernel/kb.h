#pragma once

#include <stdint.h>

#define KB_CTRL 0x100
#define KB_ALT  0x200

void kb_init(void);
void kb_reload_layout(void);
int kb_getc(void);
void kb_set_conin(void *conin);
