#include "config.h"
#include "string.h"
#include "serial.h"
#include "fs.h"
#include "heap.h"

#define CONFIG_MAX 32
#define KEY_MAX 48
#define VAL_MAX 64

struct entry {
    char key[KEY_MAX];
    char val[VAL_MAX];
};

static struct {
    struct entry entries[CONFIG_MAX];
    int count;
} cfg;

static const char *defaults[][2] = {
    {"keyboard_layout", "us"},
    {"timezone", "0"},
    {0, 0}
};

static int find(const char *key)
{
    for (int i = 0; i < cfg.count; i++)
        if (strcmp(cfg.entries[i].key, key) == 0)
            return i;
    return -1;
}

const char *config_get(const char *key)
{
    int i = find(key);
    if (i < 0) return 0;
    return cfg.entries[i].val;
}

int config_get_int(const char *key)
{
    const char *v = config_get(key);
    return v ? atoi(v) : 0;
}

static void set(const char *key, const char *val)
{
    int i = find(key);
    if (i < 0) {
        if (cfg.count >= CONFIG_MAX) return;
        i = cfg.count++;
    }
    strncpy(cfg.entries[i].key, key, KEY_MAX - 1);
    cfg.entries[i].key[KEY_MAX - 1] = '\0';
    strncpy(cfg.entries[i].val, val, VAL_MAX - 1);
    cfg.entries[i].val[VAL_MAX - 1] = '\0';
}

static const char default_content[] =
    "# OS Configuration\n"
    "# Edit this file and run :r to apply changes on reboot\n"
    "\n"
    "#define keyboard_layout de\n"
    "#define timezone 2\n";

int config_load(void)
{
    cfg.count = 0;

    for (int i = 0; defaults[i][0]; i++)
        set(defaults[i][0], defaults[i][1]);

    char *buf = malloc(2048);
    if (!buf) { serial_printf("LOG: config: no memory\n"); return -1; }

    uint32_t size = 2047;
    if (fs_read("config", buf, &size) < 0) {
        fs_write("config", default_content, strlen(default_content));
        free(buf);
        serial_printf("LOG: config: created default /config\n");
        return 0;
    }

    buf[size] = '\0';

    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) { line = nl ? nl + 1 : 0; continue; }

        if (strncmp(p, "#define", 7) == 0 && (p[7] == ' ' || p[7] == '\t' || !p[7])) {
            p += 7;
            while (*p == ' ' || *p == '\t') p++;
            char *key = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) { *p++ = '\0'; while (*p == ' ' || *p == '\t') p++; }
            char *val = p;
            while (*val && *val != '\r' && *val != '\n') val++;
            *val = '\0';
            val = p;
            if (*key) set(key, val);
        }

        line = nl ? nl + 1 : 0;
    }

    free(buf);
    serial_printf("LOG: config: loaded %d settings\n", cfg.count);
    return 0;
}
