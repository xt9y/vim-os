#ifndef KB_H
#define KB_H

#include <stdint.h>

#define KB_CTRL 0x100
#define KB_ALT  0x200

void kb_init(void);
int kb_getc(void);

#endif
