table newtable();
htable newhtable(int size);
int hasher(char *s) /* copied from Programming in C++, p14 */;
void addhnode(char *nam,void *dat,htable ht,void (*freefunc)(void *));
void expandhtab(htable ht);
void *gethnode(char *nam,htable ht);
void freehtab(htable ht,void (*freefunc)(void *));
void *remhnode(char *nam,htable ht);
void *zalloc(int l);
void *alloc(int l);
void addnode(table list,void *str);
void insnode(table list,Node last,void *dat);
void *remnode(table list,Node nd);
void chuck(char *str);
void *getnode(table list);
int full(table list);
void freetable(table tab,void (*freefunc)(void *));
char *strdup(char *str);
/*const char *strstr(const char *s,const char *t); */
