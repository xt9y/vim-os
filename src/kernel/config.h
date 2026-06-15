#ifndef CONFIG_H
#define CONFIG_H

int config_load(void);
const char *config_get(const char *key);
int config_get_int(const char *key);

#endif
