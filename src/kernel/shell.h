#ifndef SHELL_H
#define SHELL_H

struct terminal;

int shell_exec(const char *cmd, struct terminal *term);

#endif
