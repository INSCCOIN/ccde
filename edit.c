#include "edit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Doc tabs[NTAB];
int curtab;
Doc *D = &tabs[0];

static char *xdup(const char *s)
{
    size_t n = strlen(s ? s : "");
    char *p = malloc(n + 1);
    if (!p)
        return NULL;
    memcpy(p, s ? s : "", n + 1);
    return p;
}

static void doc_free(Doc *d)
{
    int i;
    if (!d->l)
        return;
    for (i = 0; i < d->n; i++)
        free(d->l[i]);
    free(d->l);
    d->l = NULL;
    d->n = d->cap = 0;
}

static int doc_grow(Doc *d, int need)
{
    char **p;
    int cap = d->cap;
    if (need <= cap)
        return 1;
    cap = cap ? cap * 2 : 64;
    while (cap < need)
        cap *= 2;
    p = realloc(d->l, (size_t)cap * sizeof *p);
    if (!p)
        return 0;
    d->l = p;
    d->cap = cap;
    return 1;
}

static int line_grow(char **lp, size_t need)
{
    size_t n = *lp ? strlen(*lp) : 0;
    char *p;
    if (need <= n + 1 && *lp)
        return 1;
    p = realloc(*lp, need + 32);
    if (!p)
        return 0;
    if (!*lp)
        p[0] = 0;
    *lp = p;
    return 1;
}

void e_init(void)
{
    int i;
    memset(tabs, 0, sizeof tabs);
    for (i = 0; i < NTAB; i++) {
        doc_grow(&tabs[i], 1);
        tabs[i].l[0] = xdup("");
        tabs[i].n = 1;
    }
    curtab = 0;
    D = &tabs[0];
}

void e_select(int i)
{
    if (i < 0 || i >= NTAB)
        return;
    curtab = i;
    D = &tabs[i];
}

void e_next(int dir)
{
    e_select((curtab + dir + NTAB) % NTAB);
}

static void clamp(void)
{
    int n;
    if (D->cy < 0)
        D->cy = 0;
    if (D->cy >= D->n)
        D->cy = D->n - 1;
    n = D->l[D->cy] ? (int)strlen(D->l[D->cy]) : 0;
    if (D->cx < 0)
        D->cx = 0;
    if (D->cx > n)
        D->cx = n;
}

void e_clear(void)
{
    doc_free(D);
    doc_grow(D, 1);
    D->l[0] = xdup("");
    D->n = 1;
    D->cx = D->cy = D->dirty = D->top = 0;
    D->path[0] = 0;
}

int e_load(const char *p)
{
    FILE *f;
    char buf[4096];
    doc_free(D);
    doc_grow(D, 8);
    D->n = 0;
    D->cx = D->cy = D->top = 0;
    D->dirty = 0;
    snprintf(D->path, sizeof D->path, "%s", p);
    f = fopen(p, "r");
    if (!f) {
        D->l[0] = xdup("");
        D->n = 1;
        return 0;
    }
    while (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = 0;
        if (!doc_grow(D, D->n + 1))
            break;
        D->l[D->n++] = xdup(buf);
    }
    fclose(f);
    if (!D->n) {
        D->l[0] = xdup("");
        D->n = 1;
    }
    return 1;
}

int e_saveas(const char *p)
{
    FILE *f;
    int i;
    f = fopen(p, "w");
    if (!f)
        return 0;
    for (i = 0; i < D->n; i++) {
        fputs(D->l[i] ? D->l[i] : "", f);
        fputc('\n', f);
    }
    fclose(f);
    snprintf(D->path, sizeof D->path, "%s", p);
    D->dirty = 0;
    return 1;
}

int e_save(void)
{
    if (!D->path[0])
        return 0;
    return e_saveas(D->path);
}

const char *e_name_i(int i)
{
    const char *s;
    if (!tabs[i].path[0])
        return "untitled";
    s = strrchr(tabs[i].path, '/');
    return s ? s + 1 : tabs[i].path;
}

const char *e_name(void)
{
    return e_name_i(curtab);
}

char *e_line(int i)
{
    return (i >= 0 && i < D->n && D->l[i]) ? D->l[i] : "";
}

int e_n(void)
{
    return D->n;
}

int e_dirty(void)
{
    return D->dirty;
}

int e_any_dirty(void)
{
    int i;
    for (i = 0; i < NTAB; i++)
        if (tabs[i].dirty)
            return 1;
    return 0;
}

void e_ins(int ch)
{
    char **lp = &D->l[D->cy];
    int n = *lp ? (int)strlen(*lp) : 0;
    if (D->cx > n)
        D->cx = n;
    if (!line_grow(lp, (size_t)n + 2))
        return;
    memmove(*lp + D->cx + 1, *lp + D->cx, (size_t)(n - D->cx + 1));
    (*lp)[D->cx++] = (char)ch;
    D->dirty = 1;
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
    char *cur, *rest;
    int n;
    if (!doc_grow(D, D->n + 1))
        return;
    cur = D->l[D->cy];
    n = cur ? (int)strlen(cur) : 0;
    if (D->cx > n)
        D->cx = n;
    rest = xdup(cur + D->cx);
    cur[D->cx] = 0;
    memmove(&D->l[D->cy + 2], &D->l[D->cy + 1],
            (size_t)(D->n - D->cy - 1) * sizeof(char *));
    D->l[D->cy + 1] = rest ? rest : xdup("");
    D->n++;
    D->cy++;
    D->cx = 0;
    D->dirty = 1;
}

void e_bs(void)
{
    char *cur = D->l[D->cy];
    int n = cur ? (int)strlen(cur) : 0;
    if (D->cx > 0) {
        memmove(cur + D->cx - 1, cur + D->cx, (size_t)(n - D->cx + 1));
        D->cx--;
        D->dirty = 1;
        return;
    }
    if (D->cy == 0)
        return;
    {
        char *prev = D->l[D->cy - 1];
        int pn = prev ? (int)strlen(prev) : 0;
        if (!line_grow(&D->l[D->cy - 1], (size_t)pn + n + 1))
            return;
        memcpy(D->l[D->cy - 1] + pn, cur ? cur : "", (size_t)n + 1);
        free(cur);
        memmove(&D->l[D->cy], &D->l[D->cy + 1],
                (size_t)(D->n - D->cy - 1) * sizeof(char *));
        D->n--;
        D->cy--;
        D->cx = pn;
        D->dirty = 1;
    }
}

void e_left(void)
{
    if (D->cx > 0)
        D->cx--;
    else if (D->cy > 0) {
        D->cy--;
        D->cx = D->l[D->cy] ? (int)strlen(D->l[D->cy]) : 0;
    }
}

void e_right(void)
{
    int n = D->l[D->cy] ? (int)strlen(D->l[D->cy]) : 0;
    if (D->cx < n)
        D->cx++;
    else if (D->cy + 1 < D->n) {
        D->cy++;
        D->cx = 0;
    }
}

void e_up(void)
{
    if (D->cy > 0)
        D->cy--;
    clamp();
}

void e_down(void)
{
    if (D->cy + 1 < D->n)
        D->cy++;
    clamp();
}

void e_home(void)
{
    D->cx = 0;
}

void e_end(void)
{
    D->cx = D->l[D->cy] ? (int)strlen(D->l[D->cy]) : 0;
}

void e_pg(int dir, int pagesize)
{
    D->cy += dir * pagesize;
    clamp();
}

void e_goto(int line)
{
    if (line < 1)
        line = 1;
    D->cy = line - 1;
    clamp();
    D->cx = 0;
}

int e_find(const char *q, int dir)
{
    int i, start, n, qn;
    if (!q || !q[0])
        return 0;
    qn = (int)strlen(q);
    (void)qn;
    n = D->n;
    start = D->cy;
    if (dir >= 0) {
        for (i = 0; i < n; i++) {
            int li = (start + i) % n;
            const char *s = D->l[li] ? D->l[li] : "";
            int sl = (int)strlen(s);
            int col = (i == 0) ? D->cx + 1 : 0;
            const char *hit;
            if (col > sl)
                continue;
            hit = strstr(s + col, q);
            if (hit) {
                D->cy = li;
                D->cx = (int)(hit - s);
                return 1;
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            int li = (start - i + n) % n;
            const char *s = D->l[li] ? D->l[li] : "";
            const char *p = s;
            int last = -1;
            while ((p = strstr(p, q)) != NULL) {
                int at = (int)(p - s);
                if (li != start || at < D->cx)
                    last = at;
                p += 1;
            }
            if (last >= 0) {
                D->cy = li;
                D->cx = last;
                return 1;
            }
        }
    }
    return 0;
}
