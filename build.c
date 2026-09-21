#include "build.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char blog[BMAX][BLEN];
int blogn, bsel, blast_ok = -1;
char proj[400] = ".";

void b_clear(void)
{
    memset(blog, 0, sizeof blog);
    blogn = bsel = 0;
}

void b_setproj(const char *p)
{
    snprintf(proj, sizeof proj, "%s", p && p[0] ? p : ".");
}

void b_add(const char *s)
{
    if (blogn >= BMAX) {
        memmove(blog[0], blog[1], (size_t)(BMAX - 1) * BLEN);
        blogn = BMAX - 1;
    }
    snprintf(blog[blogn++], BLEN, "%s", s);
}

static int has_make(void)
{
    char p[512];
    struct stat st;
    snprintf(p, sizeof p, "%s/Makefile", proj);
    return stat(p, &st) == 0;
}

static char *bin_name(void)
{
    static char n[80];
    const char *s = strrchr(proj, '/');
    s = s ? s + 1 : proj;
    if (!s[0] || !strcmp(s, "."))
        snprintf(n, sizeof n, "a.out");
    else
        snprintf(n, sizeof n, "%s", s);
    return n;
}

int b_make(void)
{
    char cmd[640], line[BLEN];
    FILE *f;
    b_clear();
    if (has_make())
        snprintf(cmd, sizeof cmd, "make -C '%s' 2>&1", proj);
    else
        snprintf(cmd, sizeof cmd,
                 "sh -c 'cd \"%s\" && gcc -O2 -Wall -Wextra -o %s *.c -lm 2>&1'",
                 proj, bin_name());
    b_add(cmd);
    f = popen(cmd, "r");
    if (!f) {
        b_add("popen failed");
        blast_ok = 0;
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        b_add(line);
    }
    blast_ok = (pclose(f) == 0);
    b_add(blast_ok ? "-- ok --" : "-- failed --");
    bsel = blogn - 1;
    return blast_ok;
}

int b_run(void)
{
    char cmd[640], line[BLEN], bin[512];
    FILE *f;
    snprintf(bin, sizeof bin, "%s/%s", proj, bin_name());
    if (access(bin, X_OK) != 0)
        snprintf(bin, sizeof bin, "%s/a.out", proj);
    if (access(bin, X_OK) != 0) {
        b_add("no binary — build first");
        return 0;
    }
    snprintf(cmd, sizeof cmd, "'%s' 2>&1 | head -40", bin);
    b_add(cmd);
    f = popen(cmd, "r");
    if (!f)
        return 0;
    while (fgets(line, sizeof line, f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        b_add(line);
    }
    pclose(f);
    bsel = blogn - 1;
    return 1;
}

int b_parse_jump(int *line, char *file, int nfile)
{
    const char *s;
    int ln = 0;
    if (bsel < 0 || bsel >= blogn)
        return 0;
    s = blog[bsel];
    /* file:line: or file:line:col: */
    {
        const char *col = strchr(s, ':');
        if (!col)
            return 0;
        if (col - s >= nfile)
            return 0;
        memcpy(file, s, (size_t)(col - s));
        file[col - s] = 0;
        ln = atoi(col + 1);
        if (ln <= 0)
            return 0;
        *line = ln;
        return 1;
    }
}
