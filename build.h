#ifndef CCDE_BUILD_H
#define CCDE_BUILD_H
#define BMAX 120
#define BLEN 200
extern char blog[BMAX][BLEN];
extern int blogn, bsel, blast_ok;
extern char proj[400];
void b_clear(void);
void b_add(const char *s);
void b_setproj(const char *p);
int b_make(void);
int b_run(void);
int b_parse_jump(int *line, char *file, int nfile);
#endif
