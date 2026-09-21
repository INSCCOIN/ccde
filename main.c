#define _GNU_SOURCE
#include "fb.h"
#include "edit.h"
#include "build.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>

enum { M_NONE, M_FILE, M_BUILD, M_SET };
enum { F_EDIT, F_OUT, F_PICK };

static int menu, msel, focus = F_EDIT, font = 1, tabw = 4, ask;
static int toprow, outtop, pick_n, pick_sel, pinning, want_quit;
static char pin[400], prompt[80];
static char picks[64][80];
static char work[400] = "/home/working";

static uint16_t Cbg, Cfg, Cbar, Cbarfg, Cacc, Cdim, Csel, Ckw, Cstr, Ccom, Ccur;

static struct termios oldt;
static int rawon;

static void colors(void)
{
    Cbg = rgb565(12, 16, 22);
    Cfg = rgb565(210, 214, 220);
    Cbar = rgb565(22, 28, 38);
    Cbarfg = rgb565(230, 220, 180);
    Cacc = rgb565(70, 140, 200);
    Cdim = rgb565(90, 100, 120);
    Csel = rgb565(28, 40, 56);
    Ckw = rgb565(120, 180, 255);
    Cstr = rgb565(200, 170, 90);
    Ccom = rgb565(80, 160, 100);
    Ccur = rgb565(240, 220, 90);
}

static void io_open(void)
{
    struct termios t;
    tcgetattr(0, &oldt);
    t = oldt;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    rawon = 1;
}

static void io_close(void)
{
    if (rawon)
        tcsetattr(0, TCSANOW, &oldt);
}

static int is_kw(const char *s, int n)
{
    static const char *k[] = {
        "auto","break","case","char","const","continue","default","do",
        "double","else","enum","extern","float","for","goto","if","int",
        "long","register","return","short","signed","sizeof","static",
        "struct","switch","typedef","union","unsigned","void","volatile",
        "while","include","define","ifdef","ifndef","endif", NULL
    };
    int i;
    for (i = 0; k[i]; i++)
        if ((int)strlen(k[i]) == n && !memcmp(k[i], s, (size_t)n))
            return 1;
    return 0;
}

static void draw_code(int x, int y, const char *s, int sc)
{
    int i = 0, n = (int)strlen(s);
    while (i < n) {
        uint16_t col = Cfg;
        int j = i, w;
        if (s[i] == '/' && s[i + 1] == '/') {
            text_s(x, y, s + i, Ccom, sc);
            return;
        }
        if (s[i] == '"' || s[i] == '\'') {
            char q = s[i];
            j = i + 1;
            while (s[j] && s[j] != q) {
                if (s[j] == '\\' && s[j + 1])
                    j++;
                j++;
            }
            if (s[j])
                j++;
            col = Cstr;
        } else if (s[i] == '#' || (s[i] >= 'A' && s[i] <= 'z' && (s[i] == '_' || ((s[i] | 32) >= 'a')))) {
            j = i + 1;
            while ((s[j] >= '0' && s[j] <= '9') || s[j] == '_' ||
                   (s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= 'a' && s[j] <= 'z'))
                j++;
            if (s[i] == '#' || is_kw(s + (s[i] == '#' ? i + 1 : i),
                                     s[i] == '#' ? j - i - 1 : j - i))
                col = Ckw;
        } else {
            j = i + 1;
        }
        {
            char tmp[ECMAX];
            w = j - i;
            if (w >= ECMAX)
                w = ECMAX - 1;
            memcpy(tmp, s + i, (size_t)w);
            tmp[w] = 0;
            text_s(x, y, tmp, col, sc);
            x += w * font_w(sc);
        }
        i = j;
    }
}

static int ed_rows(void)
{
    int h = (int)FB_H - 16 - 70 - 14;
    int ch = font_h(font);
    int r = ch ? h / ch : 12;
    return r < 4 ? 4 : r;
}

static int ed_cols(void)
{
    int w = (int)FB_W - 28;
    int cw = font_w(font);
    int c = cw ? w / cw : 40;
    return c < 10 ? 10 : c;
}

static void scan_dir(void)
{
    DIR *d = opendir(work[0] ? work : ".");
    struct dirent *e;
    pick_n = 0;
    if (!d)
        return;
    while ((e = readdir(d)) && pick_n < 64) {
        size_t n = strlen(e->d_name);
        if (e->d_name[0] == '.')
            continue;
        if (n > 2 && (!strcmp(e->d_name + n - 2, ".c") || !strcmp(e->d_name + n - 2, ".h")))
            snprintf(picks[pick_n++], 80, "%s", e->d_name);
        else if (!strcmp(e->d_name, "Makefile"))
            snprintf(picks[pick_n++], 80, "%s", e->d_name);
    }
    closedir(d);
    if (pick_sel >= pick_n)
        pick_sel = pick_n ? pick_n - 1 : 0;
}

static void draw_pick(void)
{
    int i, x = 40, y = 30, w = 200, h = 16 + pick_n * 12;
    if (h > (int)FB_H - 50)
        h = (int)FB_H - 50;
    fill(x, y, w, h, Csel);
    rect(x, y, w, h, Cacc);
    text(x + 6, y + 4, work, Cbarfg);
    for (i = 0; i < pick_n && 20 + i * 12 < h - 4; i++) {
        if (i == pick_sel)
            fill(x + 2, y + 16 + i * 12, w - 4, 12, Cacc);
        text(x + 8, y + 18 + i * 12, picks[i], i == pick_sel ? Cbg : Cfg);
    }
}

static void draw(void)
{
    int sc = font, ch = font_h(sc), cw = font_w(sc);
    int er = ed_rows(), ec = ed_cols();
    int i, y0 = 18, split, outh;
    char st[96], ln[8], vis[ECMAX];

    if (ecy < toprow)
        toprow = ecy;
    if (ecy >= toprow + er)
        toprow = ecy - er + 1;
    if (toprow < 0)
        toprow = 0;
    if (bsel < outtop)
        outtop = bsel;
    if (bsel >= outtop + 4)
        outtop = bsel - 3;

    fb_clear(Cbg);
    fill(0, 0, (int)FB_W, 16, Cbar);
    {
        const char *t[] = {"File", "Build", "Settings"};
        int ids[] = {M_FILE, M_BUILD, M_SET};
        int x = 4, k;
        for (k = 0; k < 3; k++) {
            int w = (int)strlen(t[k]) * 6 + 10;
            if (menu == ids[k])
                fill(x - 2, 1, w, 14, Cacc);
            text(x, 4, t[k], menu == ids[k] ? Cbg : Cbarfg);
            x += w + 8;
        }
        text((int)FB_W - 50, 4, "ccde", Cacc);
    }

    /* gutter + editor */
    fill(0, 16, 26, er * ch + 4, Cbar);
    for (i = 0; i < er; i++) {
        int li = toprow + i;
        int y = y0 + i * ch;
        if (li >= elines)
            break;
        snprintf(ln, sizeof ln, "%3d", li + 1);
        text_s(2, y, ln, li == ecy ? Cacc : Cdim, sc);
        {
            int n = (int)strlen(eline[li]);
            int off = 0;
            if (li == ecy && ecx >= ec)
                off = ecx - ec + 1;
            if (n - off > ec)
                n = off + ec;
            if (n < off)
                n = off;
            memcpy(vis, eline[li] + off, (size_t)(n - off));
            vis[n - off] = 0;
            draw_code(28, y, vis, sc);
            if (li == ecy && focus == F_EDIT && !menu && !ask && !pinning && focus != F_PICK) {
                int col = ecx - off;
                fill(28 + col * cw, y, cw, ch, Ccur);
            }
        }
    }

    split = 16 + er * ch + 4;
    fill(0, split, (int)FB_W, 12, Cbar);
    text(4, split + 2, blast_ok == 1 ? "build ok" : blast_ok == 0 ? "build fail" : "output",
         blast_ok == 1 ? Ccom : blast_ok == 0 ? rgb565(220, 80, 70) : Cbarfg);

    outh = (int)FB_H - split - 12 - 14;
    fill(0, split + 12, (int)FB_W, outh, rgb565(8, 10, 16));
    for (i = 0; i < 5 && i * 10 < outh; i++) {
        int li = outtop + i;
        int y = split + 14 + i * 10;
        if (li >= blogn)
            break;
        if (li == bsel)
            fill(0, y - 1, (int)FB_W, 10, Csel);
        text(4, y, blog[li], li == bsel ? Cfg : Cdim);
    }

    fill(0, (int)FB_H - 14, (int)FB_W, 14, Cbar);
    if (pinning)
        snprintf(st, sizeof st, "%s%s", prompt, pin);
    else
        snprintf(st, sizeof st, "%s%s  L%d  F5 build  F6 run  Esc menu  ^X quit",
                 edirty ? "*" : "", e_name(), ecy + 1);
    text(4, (int)FB_H - 11, st, Cbarfg);

    if (menu) {
        const char *it[8];
        int n = 0, x = 4, y = 16, w = 150, k;
        if (menu == M_FILE) {
            it[n++] = "Open…";
            it[n++] = "Save   ^S";
            it[n++] = "New";
            it[n++] = "Quit   ^X";
            x = 4;
        } else if (menu == M_BUILD) {
            it[n++] = "Build  F5";
            it[n++] = "Run    F6";
            it[n++] = "Goto error";
            x = 44;
        } else {
            it[n++] = font == 1 ? "Font small" : "Font large";
            it[n++] = tabw == 2 ? "Tab 2" : tabw == 8 ? "Tab 8" : "Tab 4";
            x = 96;
        }
        fill(x, y, w, 6 + n * 14, Csel);
        rect(x, y, w, 6 + n * 14, Cacc);
        for (k = 0; k < n; k++) {
            if (k == msel)
                fill(x + 1, y + 3 + k * 14, w - 2, 13, Cacc);
            text(x + 6, y + 6 + k * 14, it[k], k == msel ? Cbg : Cfg);
        }
    }
    if (focus == F_PICK)
        draw_pick();
    if (ask) {
        int w = 260, h = 60, x = ((int)FB_W - 260) / 2, y = 80;
        fill(x, y, w, h, Csel);
        rect(x, y, w, h, Cacc);
        text(x + 10, y + 12, "Save changes?", Cfg);
        text(x + 10, y + 32, "Y save   N discard   Esc", Cdim);
    }
    fb_flip();
}

static void open_pick(void)
{
    scan_dir();
    focus = F_PICK;
    pick_sel = 0;
}

static void open_sel(void)
{
    char full[512];
    if (pick_sel < 0 || pick_sel >= pick_n)
        return;
    snprintf(full, sizeof full, "%s/%s", work, picks[pick_sel]);
    e_load(full);
    b_setproj(work);
    focus = F_EDIT;
}

static void jump_err(void)
{
    char file[120], full[512];
    int line = 0;
    if (!b_parse_jump(&line, file, sizeof file))
        return;
    if (strchr(file, '/'))
        snprintf(full, sizeof full, "%s", file);
    else
        snprintf(full, sizeof full, "%s/%s", work, file);
    if (e_load(full) || !strcmp(e_name(), file) || strstr(epath, file)) {
        if (strcmp(epath, full))
            e_load(full);
        e_goto(line);
        focus = F_EDIT;
    }
}

static void do_quit_request(void)
{
    if (edirty)
        ask = 1;
    else
        want_quit = 1;
}

static void handle_ask(unsigned char c)
{
    if (c == 'y' || c == 'Y') {
        if (epath[0])
            e_save();
        ask = 0;
        want_quit = 1;
    } else if (c == 'n' || c == 'N') {
        ask = 0;
        want_quit = 1;
    } else if (c == 27)
        ask = 0;
}

static void menu_enter(void)
{
    if (menu == M_FILE) {
        if (msel == 0)
            open_pick();
        else if (msel == 1)
            e_save();
        else if (msel == 2)
            e_clear();
        else if (msel == 3)
            do_quit_request();
        menu = M_NONE;
    } else if (menu == M_BUILD) {
        if (msel == 0)
            b_make();
        else if (msel == 1)
            b_run();
        else if (msel == 2)
            jump_err();
        menu = M_NONE;
        focus = F_OUT;
    } else if (menu == M_SET) {
        if (msel == 0)
            font = font == 1 ? 2 : 1;
        else
            tabw = tabw == 2 ? 4 : tabw == 4 ? 8 : 2;
    }
}

static void handle(unsigned char c, unsigned char *seq, int n)
{
    if (ask) {
        handle_ask(c);
        return;
    }
    if (pinning) {
        size_t L = strlen(pin);
        if (c == 13 || c == 10)
            pinning = 0;
        else if (c == 27)
            pinning = 0;
        else if ((c == 8 || c == 127) && L)
            pin[L - 1] = 0;
        else if (c >= 32 && c < 127 && L + 1 < sizeof pin) {
            pin[L] = (char)c;
            pin[L + 1] = 0;
        }
        return;
    }
    if (c == 24) {
        do_quit_request();
        return;
    }
    if (c == 19) {
        e_save();
        return;
    }
    if (c == 15) {
        open_pick();
        return;
    }
    /* F5 = ESC [ 1 5 ~    F6 = ESC [ 1 7 ~ */
    if (c == 27 && n >= 4 && seq[1] == '[' && seq[2] == '1') {
        if (seq[3] == '5') {
            b_make();
            focus = F_OUT;
            return;
        }
        if (seq[3] == '7') {
            b_run();
            focus = F_OUT;
            return;
        }
    }
    if (focus == F_PICK) {
        if (c == 27) {
            focus = F_EDIT;
            return;
        }
        if (c == 13 || c == 10) {
            open_sel();
            return;
        }
        if (c == 27 && n >= 3 && seq[2] == 'A' && pick_sel > 0)
            pick_sel--;
        if (c == 27 && n >= 3 && seq[2] == 'B' && pick_sel + 1 < pick_n)
            pick_sel++;
        return;
    }
    if (menu) {
        if (c == 27 && n == 1) {
            menu = M_NONE;
            return;
        }
        if (c == 9) {
            menu = menu == M_FILE ? M_BUILD : menu == M_BUILD ? M_SET : M_FILE;
            msel = 0;
            return;
        }
        if (c == 13 || c == 10) {
            menu_enter();
            return;
        }
        if (c == 27 && n >= 3 && seq[1] == '[') {
            if (seq[2] == 'A' && msel > 0)
                msel--;
            if (seq[2] == 'B')
                msel++;
            if (seq[2] == 'C') {
                menu = menu == M_FILE ? M_BUILD : menu == M_BUILD ? M_SET : M_FILE;
                msel = 0;
            }
            if (seq[2] == 'D') {
                menu = menu == M_SET ? M_BUILD : menu == M_BUILD ? M_FILE : M_SET;
                msel = 0;
            }
            if (menu == M_FILE && msel > 3)
                msel = 3;
            if (menu == M_BUILD && msel > 2)
                msel = 2;
            if (menu == M_SET && msel > 1)
                msel = 1;
        }
        return;
    }
    if (c == 27 && n == 1) {
        menu = M_FILE;
        msel = 0;
        return;
    }
    if (c == 9 && focus == F_EDIT) {
        e_tab(tabw);
        return;
    }
    if (c == 9) {
        focus = focus == F_EDIT ? F_OUT : F_EDIT;
        return;
    }
    if (focus == F_OUT) {
        if (c == 27 && n >= 3 && seq[2] == 'A' && bsel > 0)
            bsel--;
        if (c == 27 && n >= 3 && seq[2] == 'B' && bsel + 1 < blogn)
            bsel++;
        if (c == 13 || c == 10)
            jump_err();
        return;
    }
    if (c == 27 && n >= 3 && seq[1] == '[') {
        if (seq[2] == 'A')
            e_up();
        else if (seq[2] == 'B')
            e_down();
        else if (seq[2] == 'C')
            e_right();
        else if (seq[2] == 'D')
            e_left();
        else if (seq[2] == 'H')
            e_home();
        else if (seq[2] == 'F')
            e_end();
        else if (seq[2] == '5')
            e_pg(-1, ed_rows());
        else if (seq[2] == '6')
            e_pg(1, ed_rows());
        return;
    }
    if (c == 13 || c == 10) {
        e_nl();
        return;
    }
    if (c == 8 || c == 127) {
        e_bs();
        return;
    }
    if (c >= 32 && c < 127)
        e_ins(c);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        char *slash;
        e_load(argv[1]);
        snprintf(work, sizeof work, "%s", argv[1]);
        slash = strrchr(work, '/');
        if (slash)
            *slash = 0;
        if (!work[0])
            snprintf(work, sizeof work, ".");
        b_setproj(work);
    } else
        b_setproj(work);
    colors();
    if (fb_open() < 0) {
        fprintf(stderr, "ccde needs /dev/fb0\n");
        return 1;
    }
    io_open();
    b_add("ccde — Esc menu, F5 build, F6 run");
    while (!want_quit) {
        unsigned char b[24];
        int n = (int)read(0, b, sizeof b), i;
        draw();
        if (n <= 0) {
            usleep(20000);
            continue;
        }
        for (i = 0; i < n; i++) {
            if (b[i] == 27 && i + 1 < n) {
                int len = n - i;
                handle(b[i], b + i, len);
                if (i + 3 < n && b[i + 1] == '[' && b[i + 2] == '1')
                    i += 4;
                else
                    i += (i + 2 < n) ? 2 : 0;
            } else
                handle(b[i], b + i, 1);
        }
    }
    io_close();
    fb_close();
    return 0;
}
