#ifndef CCDE_EDIT_H
#define CCDE_EDIT_H

#define NTAB 3

typedef struct {
    char **l;
    int n, cap, cx, cy, dirty, top;
    char path[512];
} Doc;

extern Doc tabs[NTAB];
extern int curtab;
extern Doc *D;

void e_init(void);
void e_select(int i);
void e_next(int dir);
void e_clear(void);
int e_load(const char *p);
int e_save(void);
int e_saveas(const char *p);
void e_ins(int ch);
void e_tab(int w);
void e_nl(void);
void e_bs(void);
void e_left(void);
void e_right(void);
void e_up(void);
void e_down(void);
void e_home(void);
void e_end(void);
void e_pg(int dir, int pagesize);
void e_goto(int line);
const char *e_name(void);
const char *e_name_i(int i);
char *e_line(int i);
int e_n(void);
int e_dirty(void);
int e_find(const char *q, int dir);
int e_any_dirty(void);

#endif
