#pragma once

struct terminal;

int shell_exec(const char *cmd, struct terminal *term);
