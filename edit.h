#ifndef CCDE_EDIT_H
#define CCDE_EDIT_H
#define ELMAX 800
#define ECMAX 240
extern char eline[ELMAX][ECMAX];
extern int elines, ecx, ecy, edirty;
extern char epath[512];
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
#endif
