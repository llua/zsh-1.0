void glob(table list,Node *np);
int notstrcmp(char **a,char **b);
void insert(char *s);
int haswilds(char *str);
int hasbraces(char *str);
int xpandredir(struct fnode *fn,table tab);
char *dyncat(char *s1,char *s2);
char *tricat(char *s1,char *s2,char *s3);
void xpandbraces(table list,Node *np);
char *getparen(char *str);
int matchpat(char *a,char *b);
void getmatch(char **sp,char *pat,int dd);
void addpath(char *s);
void scanner(qath q);
int minimatch(char **pat,char **str);
int doesmatch(char *str,comp c,int first);
qath parsepat(char *str);
comp parsereg(char *str);
qath parseqath(void);
comp parsecomp(void);
comp parsecompsw(void);
void adjustcomp(comp c1,comp c2,comp c3);
void freepath(qath p);
void freecomp(comp c);
int patmatch(char *ss,char *tt);
void remnulargs(char *s);
