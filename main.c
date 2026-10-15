// Xbox 360 Pad Tester for PS3 (PSL1GHT homebrew)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ppu-types.h>
#include <sysutil/video.h>
#include <sysutil/sysutil.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#define HOST_SIZE (32 * 1024 * 1024)
#define CB_SIZE   (0x100000)

static gcmContextData *ctx;
static u32 W, H;
static u32 *fb[2];
static int cur = 0, first_flip = 1;
static int running = 1;

static void init_screen(void)
{
    void *host = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&ctx, CB_SIZE, HOST_SIZE, host);

    videoState st;
    videoGetState(0, 0, &st);
    videoResolution res;
    videoGetResolution(st.displayMode.resolution, &res);

    videoConfiguration vc;
    memset(&vc, 0, sizeof(vc));
    vc.resolution = st.displayMode.resolution;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = res.width * 4;
    vc.aspect = st.displayMode.aspect;
    videoConfigure(0, &vc, NULL, 0);
    videoGetState(0, 0, &st);

    gcmSetFlipMode(GCM_FLIP_VSYNC);
    W = res.width;
    H = res.height;

    for (int i = 0; i < 2; i++) {
        fb[i] = (u32 *)rsxMemalign(64, W * H * 4);
        u32 off;
        rsxAddressToOffset(fb[i], &off);
        gcmSetDisplayBuffer(i, off, W * 4, W, H);
    }
    gcmResetFlipStatus();
}

static void wait_flip(void)
{
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
}

static void flip(void)
{
    if (!first_flip) wait_flip(); else gcmResetFlipStatus();
    if (gcmSetFlip(ctx, cur) == 0) {
        rsxFlushBuffer(ctx);
        gcmSetWaitFlip(ctx);
        first_flip = 0;
    }
    cur ^= 1;
}

static void rect(int x, int y, int w, int h, u32 c)
{
    u32 *p = fb[cur];
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= (int)H) continue;
        for (int i = x; i < x + w; i++)
            if (i >= 0 && i < (int)W) p[j * W + i] = c;
    }
}

static const char FONT_CHARS[] = " ()-./:+>0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const unsigned char FONT[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00},
    {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x00,0x36,0x36,0x00,0x00}, {0x08,0x08,0x3E,0x08,0x08}, {0x00,0x41,0x22,0x14,0x08},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31}, {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03}, {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43},
};

static void text(int x, int y, int s, u32 c, const char *str)
{
    for (; *str; str++, x += 6 * s) {
        char ch = *str;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        const char *f = strchr(FONT_CHARS, ch);
        if (!f || ch == 0) continue;
        const unsigned char *g = FONT[f - FONT_CHARS];
        for (int col = 0; col < 5; col++)
            for (int row = 0; row < 7; row++)
                if (g[col] & (1 << row))
                    rect(x + col * s, y + row * s, s, s, c);
    }
}

typedef struct { const char *xbox; const char *ps3; int on; int prev; } Btn;

static void exit_cb(u64 status, u64 param, void *usrdata)
{
    (void)param; (void)usrdata;
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

int main(void)
{
    init_screen();
    sysUtilRegisterCallback(0, exit_cb, NULL);
    ioPadInit(7);

    Btn b[17] = {
        {"A","CROSS",0,0}, {"B","CIRCLE",0,0}, {"X","SQUARE",0,0}, {"Y","TRIANGLE",0,0},
        {"LB","L1",0,0}, {"RB","R1",0,0}, {"LT","L2",0,0}, {"RT","R2",0,0},
        {"LS CLICK","L3",0,0}, {"RS CLICK","R3",0,0},
        {"START","START",0,0}, {"BACK","SELECT",0,0},
        {"DPAD UP","UP",0,0}, {"DPAD DOWN","DOWN",0,0},
        {"DPAD LEFT","LEFT",0,0}, {"DPAD RIGHT","RIGHT",0,0}, {NULL,NULL,0,0}
    };
    int nb = 16;
    char msg[96] = "PRESS ANY BUTTON";
    int msg_timer = 0;
    int lh = 128, lv = 128, rh = 128, rv = 128;
    int pad_idx = -1;

    int s = (W >= 1600) ? 4 : 3;

    while (running) {
        sysUtilCheckCallback();

        padInfo info;
        padData d;
        ioPadGetInfo(&info);
        pad_idx = -1;
        for (int i = 0; i < MAX_PADS; i++)
            if (info.status[i]) { pad_idx = i; break; }

        if (pad_idx >= 0) {
            ioPadGetData(pad_idx, &d);
            b[0].on = d.BTN_CROSS;   b[1].on = d.BTN_CIRCLE;
            b[2].on = d.BTN_SQUARE;  b[3].on = d.BTN_TRIANGLE;
            b[4].on = d.BTN_L1;      b[5].on = d.BTN_R1;
            b[6].on = d.BTN_L2;      b[7].on = d.BTN_R2;
            b[8].on = d.BTN_L3;      b[9].on = d.BTN_R3;
            b[10].on = d.BTN_START;  b[11].on = d.BTN_SELECT;
            b[12].on = d.BTN_UP;     b[13].on = d.BTN_DOWN;
            b[14].on = d.BTN_LEFT;   b[15].on = d.BTN_RIGHT;
            lh = d.ANA_L_H; lv = d.ANA_L_V; rh = d.ANA_R_H; rv = d.ANA_R_V;

            for (int i = 0; i < nb; i++) {
                if (b[i].on && !b[i].prev) {
                    snprintf(msg, sizeof(msg), "XBOX %s -> PS3 %s WORKS", b[i].xbox, b[i].ps3);
                    msg_timer = 120;
                }
                b[i].prev = b[i].on;
            }
            if (b[4].on && b[5].on && b[10].on && b[11].on) running = 0;
        } else {
            for (int i = 0; i < nb; i++) b[i].on = b[i].prev = 0;
        }
        if (msg_timer > 0) msg_timer--;

        rect(0, 0, W, H, 0x00101820);
        text(40, 30, s, 0x00FFFFFF, "XBOX 360 PAD TESTER");
        if (pad_idx >= 0) {
            char t[64];
            snprintf(t, sizeof(t), "PAD %d CONNECTED", pad_idx);
            text(40, 30 + 10 * s, s, 0x0000FF00, t);
        } else {
            text(40, 30 + 10 * s, s, 0x00FF4040, "NO PAD - PLUG IN THE USB CABLE");
        }

        int colw = W / 2 - 40;
        int y0 = 30 + 24 * s, rowh = 10 * s;
        for (int i = 0; i < nb; i++) {
            int col = i / 8, row = i % 8;
            int x = 40 + col * (colw + 40), y = y0 + row * rowh;
            u32 bg = b[i].on ? 0x0020A040 : 0x00202C3C;
            rect(x, y, colw, rowh - 6, bg);
            char t[64];
            snprintf(t, sizeof(t), "%s -> %s", b[i].xbox, b[i].ps3);
            text(x + 10, y + 3, s, b[i].on ? 0x00FFFFFF : 0x00909090, t);
        }

        char t[96];
        int ya = y0 + 8 * rowh + 10;
        snprintf(t, sizeof(t), "LEFT STICK %d %d   RIGHT STICK %d %d", lh, lv, rh, rv);
        text(40, ya, s, 0x00B0C0FF, t);

        if (msg_timer > 0)
            text(40, ya + 12 * s, s + 1, 0x0000FFFF, msg);
        else
            text(40, ya + 12 * s, s, 0x00606060, msg);

        text(40, H - 12 * s, s - 1 > 0 ? s - 1 : 1, 0x00707070, "EXIT: L1 + R1 + START + SELECT");

        flip();
    }

    ioPadEnd();
    return 0;
}
