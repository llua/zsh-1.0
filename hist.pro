void hwaddc(int c);
int hgetc(void);
void strinbeg(void);
void strinend(void);
int stuff(char *fn);
int hgetch(void);
void hungetch(int c);
void hungetc(int c);
void hflush(void);
void hungets(char *str);
void hbegin(void);
void inittty(void);
int hend(void);
void remhist(void);
void hwbegin(void);
char *hwadd(void);
int getargspec(int argc,int marg);
int hconsearch(char *str,int *marg);
int hcomsearch(char *str);
int apply1(int gflag,int (*func)(void **),table list);
int remtpath(void **junkptr);
int remtext(void **junkptr);
int rembutext(void **junkptr);
int remlpaths(void **junkptr);
int subst(int gbal,table slist,char *ptr1,char *ptr2);
int subststr(void **strptr,char *in,char *out,int gbal);
char *convamps(char *out,char *in);
char *makehlist(table tab,int freeit);
table quietgetevent(int ev);
table getevent(int ev);
int getargc(table tab);
table getargs(table elist,int arg1,int arg2);
int quote(void **tr);
int quotebreak(void **tr);
void stradd(char **p,char *d);
char *putprompt(char *fm);
void herrflush(void);
char *hdynread(char stop);
char *hdynread2(char stop);
void setcbreak(void);
int getlineleng(void);
void unsetcbreak(void);
void attachtty(long pgrp);
