int echo(comm comm);
void pdstack(void);
int zexit(comm comm);
int zreturn(comm comm);
int logout(comm comm);
int Unset(comm comm);
int set(comm comm);
int setopt(comm comm);
int unsetopt(comm comm);
int csetopt(comm comm,int isun);
void pparm(char *s,struct pmnode *t);
int dirs(comm comm);
void listhtable(htable ht,void (*func)(char *,char *));
void palias(char *s,struct anode *t);
void pshfunc(char *s,list l);
void niceprint(char *s);
void niceprintf(char *s,FILE *f);
char *buildline(table t);
int Alias(comm comm);
int cd(comm comm);
int dot(comm comm);
int Umask(comm comm);
int which(comm comm);
int popd(comm comm);
int pushd(comm comm);
int chcd(char *cnam,char *cd);
int shift(comm comm);
int unhash(comm comm);
int rehash(comm comm);
int hash(comm comm);
int Break(comm comm);
int colon(comm comm);
int Glob(comm comm);
int noglob(comm comm);
int mostglob(comm comm);
int unfunction(comm comm);
int unalias(comm comm);
int prefix(char *s,char *t);
int getjob(char *s,char *prog);
int fg(comm comm);
int bg(comm comm);
int jobs(comm comm);
int Kill(comm comm);
int export(comm comm);
int integer(comm comm);
int limit(comm comm);
int unlimit(comm comm);
void showlimits(int hard,int lim);
int sched(comm comm);
int eval(comm comm);
int Brk(comm comm);
int log(comm comm);
int let(comm comm);
int Read(comm comm);
int fc(comm comm);
int fcgetcomm(char *s);
int fclist(FILE *f,int n,int r,int first,int last,table subs);
int fcsubs(char **sp,table tab);
int fcedit(char *ename,char *fn);
int disown(comm comm);
int function(comm comm);
int builtin(comm comm);
void addintern(htable ht);
void readwtab(void);
void watch(void);