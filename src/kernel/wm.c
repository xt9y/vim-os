#include "wm.h"
#include "gfx.h"
#include "term.h"
#include "string.h"
#include "heap.h"

#define NODE_NULL (-1)
#define GAP 1
#define CMD_BAR_H 16

static struct window windows[WM_MAX];
static int window_leaf[WM_MAX];
static int window_used[WM_MAX];

static int win_count;
static int focused_slot;
static uint32_t screen_w, screen_h;

static const uint32_t colors[WM_MAX] = {
    0x00223344, 0x00332244, 0x00442233,
    0x00224433, 0x00334422, 0x00443322,
    0x00112233, 0x00332211,
};

static struct {
    int left, right, parent;
    int slot;
    int dir;
    uint32_t x, y, w, h;
} nodes[2 * WM_MAX - 1];

static uint16_t used_mask;
static int root;

static char cmdline[256];
static int cmdline_len;
static int cmdline_active;

void wm_set_cmdline(const char *text, int len, int active)
{
    cmdline_active = active;
    cmdline_len = len;
    for (int i = 0; i < len && i < 255; i++)
        cmdline[i] = text[i];
    cmdline[len] = '\0';
}

static int alloc_node(void)
{
    for (int i = 0; i < 2 * WM_MAX - 1; i++)
        if (!(used_mask & (1u << i))) {
            used_mask |= (1u << i);
            nodes[i].left = nodes[i].right = nodes[i].parent = NODE_NULL;
            nodes[i].slot = -1;
            nodes[i].dir = -1;
            return i;
        }
    return NODE_NULL;
}

static void free_node(int i)
{
    if (i != NODE_NULL) used_mask &= ~(1u << i);
}

static int find_free_slot(void)
{
    for (int i = 0; i < WM_MAX; i++)
        if (!window_used[i]) return i;
    return -1;
}

void wm_init(uint32_t w, uint32_t h)
{
    screen_w = w; screen_h = h;
    used_mask = 0;
    root = NODE_NULL;
    win_count = 0;
    focused_slot = 0;
    cmdline_active = 0;
    cmdline_len = 0;
    cmdline[0] = '\0';
    for (int i = 0; i < WM_MAX; i++) {
        window_used[i] = 0;
        windows[i].app = APP_NONE;
        windows[i].app_data = 0;
    }
}

int wm_new(const char *title)
{
    if (win_count >= WM_MAX) return -1;

    int slot = find_free_slot();
    int j;
    for (j = 0; title[j] && j < 63; j++)
        windows[slot].title[j] = title[j];
    windows[slot].title[j] = '\0';
    windows[slot].bg = colors[win_count % WM_MAX];
    windows[slot].app = APP_TERMINAL;
    windows[slot].app_data = term_create(0, 0, 0, 0);
    window_used[slot] = 1;
    win_count++;

    if (root == NODE_NULL) {
        int leaf = alloc_node();
        nodes[leaf].slot = slot;
        window_leaf[slot] = leaf;
        root = leaf;
        focused_slot = slot;
        return 0;
    }

    int leaf = window_leaf[focused_slot];
    int p = nodes[leaf].parent;
    int dir = (p == NODE_NULL) ? 1 : !nodes[p].dir;

    int new_leaf = alloc_node();
    nodes[new_leaf].slot = slot;
    window_leaf[slot] = new_leaf;

    int internal = alloc_node();
    nodes[internal].left   = leaf;
    nodes[internal].right  = new_leaf;
    nodes[internal].dir    = dir;
    nodes[internal].parent = p;
    nodes[leaf].parent     = internal;
    nodes[new_leaf].parent = internal;

    if (p == NODE_NULL) {
        root = internal;
    } else {
        if (nodes[p].left == leaf)  nodes[p].left  = internal;
        else                        nodes[p].right = internal;
    }

    return 0;
}

void wm_close_focused(void)
{
    if (win_count == 0) return;

    if (windows[focused_slot].app == APP_TERMINAL && windows[focused_slot].app_data) {
        term_destroy(windows[focused_slot].app_data);
        windows[focused_slot].app_data = 0;
    }

    int leaf = window_leaf[focused_slot];
    int p = nodes[leaf].parent;

    if (p == NODE_NULL) {
        free_node(leaf);
        root = NODE_NULL;
    } else {
        int sibling = (nodes[p].left == leaf) ? nodes[p].right : nodes[p].left;
        int gp = nodes[p].parent;
        nodes[sibling].parent = gp;
        if (gp == NODE_NULL) {
            root = sibling;
        } else {
            if (nodes[gp].left == p)  nodes[gp].left  = sibling;
            else                      nodes[gp].right = sibling;
        }
        free_node(p);
        free_node(leaf);
    }

    window_used[focused_slot] = 0;
    window_leaf[focused_slot] = -1;
    windows[focused_slot].app = APP_NONE;
    windows[focused_slot].app_data = 0;
    win_count--;

    if (win_count == 0) {
        focused_slot = 0;
        return;
    }

    for (int i = 0; i < WM_MAX; i++)
        if (window_used[i]) { focused_slot = i; break; }
}

void wm_focus_next(void)
{
    if (win_count < 2) return;
    do focused_slot = (focused_slot + 1) % WM_MAX;
    while (!window_used[focused_slot]);
}

void wm_focus_prev(void)
{
    if (win_count < 2) return;
    do focused_slot = (focused_slot - 1 + WM_MAX) % WM_MAX;
    while (!window_used[focused_slot]);
}

static void assign_rects(int n, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    nodes[n].x = x; nodes[n].y = y; nodes[n].w = w; nodes[n].h = h;

    if (nodes[n].left == NODE_NULL) return;

    if (nodes[n].dir) {
        uint32_t mid = w / 2;
        assign_rects(nodes[n].left,  x,      y, mid, h);
        assign_rects(nodes[n].right, x+mid,  y, w-mid, h);
    } else {
        uint32_t mid = h / 2;
        assign_rects(nodes[n].left,  x, y,      w, mid);
        assign_rects(nodes[n].right, x, y+mid,  w, h-mid);
    }
}

void wm_render(void)
{
    if (win_count == 0 || root == NODE_NULL) return;

    uint32_t usable_h = (screen_h > CMD_BAR_H) ? screen_h - CMD_BAR_H : 0;

    assign_rects(root, 0, 0, screen_w, usable_h);

    gfx_clear(0x00000000);

    for (int i = 0; i < 2 * WM_MAX - 1; i++) {
        if (!(used_mask & (1u << i))) continue;
        if (nodes[i].left != NODE_NULL) continue;

        uint32_t rx = nodes[i].x + GAP;
        uint32_t ry = nodes[i].y + GAP;
        uint32_t rw = (nodes[i].w > 2 * GAP) ? nodes[i].w - 2 * GAP : 0;
        uint32_t rh = (nodes[i].h > 2 * GAP) ? nodes[i].h - 2 * GAP : 0;
        if (rw > 0 && rh > 0)
            gfx_rect(rx, ry, rw, rh, windows[nodes[i].slot].bg);
    }

    for (int i = 0; i < 2 * WM_MAX - 1; i++) {
        if (!(used_mask & (1u << i))) continue;
        if (nodes[i].left != NODE_NULL) continue;

        int slot = nodes[i].slot;
        if (windows[slot].app == APP_TERMINAL && windows[slot].app_data) {
            struct terminal *t = windows[slot].app_data;
            uint32_t rx = nodes[i].x + GAP + 2;
            uint32_t ry = nodes[i].y + GAP + 2;
            uint32_t rw = (nodes[i].w > 2 * GAP + 4) ? nodes[i].w - 2 * GAP - 4 : 0;
            uint32_t rh = (nodes[i].h > 2 * GAP + 4) ? nodes[i].h - 2 * GAP - 4 : 0;
            t->x = rx; t->y = ry; t->w = rw; t->h = rh;
            term_render(t);
        }
    }

    int leaf = window_leaf[focused_slot];
    uint32_t bx = nodes[leaf].x;
    uint32_t by = nodes[leaf].y;
    uint32_t bw = nodes[leaf].w;
    uint32_t bh = nodes[leaf].h;
    uint32_t b = 1;
    gfx_rect(bx,      by,      bw, b,  0x00FFFFFF);
    gfx_rect(bx,      by+bh-b, bw, b,  0x00FFFFFF);
    gfx_rect(bx,      by,      b,  bh, 0x00FFFFFF);
    gfx_rect(bx+bw-b, by,      b,  bh, 0x00FFFFFF);

    uint32_t bar_y = screen_h - CMD_BAR_H;
    gfx_rect(0, bar_y, screen_w, CMD_BAR_H, 0x00111111);
    if (cmdline_active && cmdline_len > 0) {
        gfx_str(0, bar_y, cmdline, 0x00EEEEEE, 0x00111111);
        int cx = (cmdline_len < (int)(screen_w / 8)) ? cmdline_len * 8 : ((int)(screen_w / 8) - 1) * 8;
        gfx_rect(cx, bar_y, 1, CMD_BAR_H, 0x00EEEEEE);
    }
}

int wm_count(void)
{
    return win_count;
}

int wm_focused_slot(void)
{
    return focused_slot;
}

void *wm_get_app_data(int slot)
{
    if (slot < 0 || slot >= WM_MAX || !window_used[slot]) return 0;
    return windows[slot].app_data;
}

enum app_type wm_get_app_type(int slot)
{
    if (slot < 0 || slot >= WM_MAX || !window_used[slot]) return APP_NONE;
    return windows[slot].app;
}

int wm_slot_used(int slot)
{
    if (slot < 0 || slot >= WM_MAX) return 0;
    return window_used[slot];
}

const char *wm_window_title(int slot)
{
    if (slot < 0 || slot >= WM_MAX || !window_used[slot]) return "";
    return windows[slot].title;
}
