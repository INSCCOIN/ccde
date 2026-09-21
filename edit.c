#include "edit.h"
#include <stdio.h>
#include <string.h>

char eline[ELMAX][ECMAX];
int elines = 1, ecx, ecy, edirty;
char epath[512];

void e_clear(void)
{
    memset(eline, 0, sizeof eline);
    elines = 1;
    ecx = ecy = edirty = 0;
    epath[0] = 0;
}

int e_load(const char *p)
{
    FILE *f;
    char buf[ECMAX];
    e_clear();
    snprintf(epath, sizeof epath, "%s", p);
    f = fopen(p, "r");
    if (!f)
        return 0;
    elines = 0;
    while (fgets(buf, sizeof buf, f) && elines < ELMAX) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = 0;
        snprintf(eline[elines++], ECMAX, "%s", buf);
    }
    fclose(f);
    if (!elines)
        elines = 1;
    edirty = 0;
    ecx = ecy = 0;
    return 1;
}

int e_saveas(const char *p)
{
    FILE *f;
    int i;
    f = fopen(p, "w");
    if (!f)
        return 0;
    for (i = 0; i < elines; i++) {
        fputs(eline[i], f);
        fputc('\n', f);
    }
    fclose(f);
    snprintf(epath, sizeof epath, "%s", p);
    edirty = 0;
    return 1;
}

int e_save(void)
{
    if (!epath[0])
        return 0;
    return e_saveas(epath);
}

const char *e_name(void)
{
    const char *s = strrchr(epath, '/');
    return epath[0] ? (s ? s + 1 : epath) : "untitled";
}

static void clamp(void)
{
    int n;
    if (ecy < 0)
        ecy = 0;
    if (ecy >= elines)
        ecy = elines - 1;
    n = (int)strlen(eline[ecy]);
    if (ecx < 0)
        ecx = 0;
    if (ecx > n)
        ecx = n;
}

void e_ins(int ch)
{
    int n = (int)strlen(eline[ecy]);
    if (n >= ECMAX - 2)
        return;
    if (ecx > n)
        ecx = n;
    memmove(eline[ecy] + ecx + 1, eline[ecy] + ecx, (size_t)(n - ecx + 1));
    eline[ecy][ecx++] = (char)ch;
    edirty = 1;
}

void e_tab(int w)
{
    int i;
    if (w < 2)
        w = 2;
    for (i = 0; i < w; i++)
        e_ins(' ');
}

void e_nl(void)
{
    int n;
    if (elines >= ELMAX)
        return;
    n = (int)strlen(eline[ecy]);
    if (ecx > n)
        ecx = n;
    memmove(eline[ecy + 1], eline[ecy], (size_t)(elines - ecy) * ECMAX);
    elines++;
    snprintf(eline[ecy + 1], ECMAX, "%s", eline[ecy] + ecx);
    eline[ecy][ecx] = 0;
    ecy++;
    ecx = 0;
    edirty = 1;
}

void e_bs(void)
{
    int n;
    if (ecx > 0) {
        n = (int)strlen(eline[ecy]);
        memmove(eline[ecy] + ecx - 1, eline[ecy] + ecx, (size_t)(n - ecx + 1));
        ecx--;
        edirty = 1;
        return;
    }
    if (ecy == 0)
        return;
    n = (int)strlen(eline[ecy - 1]);
    if (n + (int)strlen(eline[ecy]) >= ECMAX - 1)
        return;
    ecx = n;
    strncat(eline[ecy - 1], eline[ecy], ECMAX - 1 - n);
    memmove(eline[ecy], eline[ecy + 1], (size_t)(elines - ecy - 1) * ECMAX);
    elines--;
    ecy--;
    edirty = 1;
}

void e_left(void)
{
    if (ecx > 0)
        ecx--;
    else if (ecy > 0) {
        ecy--;
        ecx = (int)strlen(eline[ecy]);
    }
}

void e_right(void)
{
    int n = (int)strlen(eline[ecy]);
    if (ecx < n)
        ecx++;
    else if (ecy + 1 < elines) {
        ecy++;
        ecx = 0;
    }
}

void e_up(void)
{
    if (ecy > 0)
        ecy--;
    clamp();
}

void e_down(void)
{
    if (ecy + 1 < elines)
        ecy++;
    clamp();
}

void e_home(void) { ecx = 0; }
void e_end(void) { ecx = (int)strlen(eline[ecy]); }

void e_pg(int dir, int pagesize)
{
    ecy += dir * pagesize;
    clamp();
}

void e_goto(int line)
{
    if (line < 1)
        line = 1;
    ecy = line - 1;
    clamp();
    ecx = 0;
}
