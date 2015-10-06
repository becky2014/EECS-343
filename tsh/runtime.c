/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2006/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:25:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"
#include "interpreter.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

static char* BuiltInCommands[] = {
  "alias", "bg", "cd", "fg", "jobs", "unalias"
};
static char* TSHRC = ".tshrc";

typedef struct alias_l {
  char* name;
  char* cmd;
  struct alias_l* parent;
  struct alias_l* child;
} aliasL;

typedef struct bgjob_l {
  pid_t pid;
//  int gid;
  int jid;
  char* status;
  char* cmd;
  bool isBack;
  struct bgjob_l* parent;
  struct bgjob_l* child;
} bgjobL;



/* the pids of the background processes */
bgjobL *bgjobs = NULL;
/* the alias bilink list */
aliasL *alist = NULL;


/************Function Prototypes******************************************/
/* run command */
//static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/* initialize alias list */
void InitAlias();
/* finalize aliae list */
void FinAlias();
/* read in alias from file */
aliasL* ReadInAliasList(char*);
/* write alias list to file */
bool WriteAliasList(aliasL*, char*);
/* release alias list*/
void ReleaseAliasList(aliasL*);
/* release alias*/
void ReleaseAlias(aliasL*);
/* alias to string */
void DisplayAlias(aliasL*);
/* add alias */
void AddAlias(aliasL**, char*);
/* delete an alias */
void DelAlias(aliasL**, char*);
/* get real command of alias */
char* InterpretAlias(aliasL*, char*);
/* display single job */
void DisplayJob(bgjobL*);
/* display job by pid */
void DisplayPidJob(bgjobL*, pid_t);
/* display background jobs */
void DisplayJobs(bgjobL*);
/* start suspend job in backgroud */
void BGJob(bgjobL*, int);
/* bring background in foreground */
void FGJob(bgjobL*, int);
/* kill foreground job */
void KillFGJob();
/* suspend forground job */
void SuspendFGJob();
/* kill all jobs */
void KillAllJobs();
/* add new job to job list */
void AddJob(bgjobL**, pid_t, char*, bool);
/* release job */
void ReleaseJob(bgjobL*);
/* delete job from bgjobs */
void DelJob(bgjobL**, pid_t);
/* change job status */
void ChangeJobStatus(bgjobL*, pid_t, char*, bool);
/* suspend job */
void SuspendJob(bgjobL*, pid_t);
/* resume job */
void ResumeJob(bgjobL*, pid_t);
/* strip '&' sign in cmd */
char* CmdBackSignStrip(char*);

/************External Declaration*****************************************/

/**************Implementation***********************************************/
void RunCmd(commandT** cmd, int n, int fd_in, int fd_out) {
  int i;
  int fd[2];
  fd[0] = fd_in;
  for (i = 0; i < n; i++) {
    cmd[i]->fd_in = fd[0];
    if (i != n-1) {
      pipe(fd);
      cmd[i]->fd_out = fd[1];
    } else {
      cmd[i]->fd_out = fd_out;
    }
    if (cmd[i]->argc > 0) {
      char* realcmd = InterpretAlias(alist, cmd[i]->argv[0]);
      if (realcmd) {
        int taskNum = 0;
        commandT** aliasCmd = Interpret(realcmd, &taskNum);
        RunCmd(aliasCmd, taskNum, cmd[i]->fd_in, cmd[i]->fd_out);
      } else if (IsBuiltIn(cmd[i]->argv[0])) {
        RunBuiltInCmd(cmd[i]);
      } else {
        RunExternalCmd(cmd[i], TRUE);
      }
    }
    ReleaseCmdT(&cmd[i]);
  }
  free(cmd);
}

//void RunCmdFork(commandT* cmd, bool fork) {
//}

//void RunCmdBg(commandT* cmd) {
//}

//void RunCmdPipe(commandT** cmd, int n) {
//}

//void RunCmdRedirOut(commandT* cmd, char* file) {
//}

//void RunCmdRedirIn(commandT* cmd, char* file) {
//}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork) {
  if (ResolveExternalCmd(cmd)) {
    Exec(cmd, fork);
  } else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd) {
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL) {
    if(stat(cmd->argv[0], &fs) >= 0) {
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0) {
          /*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)) {
    c = strchr(&(pathlist[i]),':');
    if(c != NULL) {
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    } else {
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0) {
          /*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

static void Exec(commandT* cmd, bool forceFork) {
  int fd;
  int pid = fork();
  int status;
  if (pid == 0) { // child prcess
    if (cmd->fd_in != STDIN_FILENO) {
      dup2(cmd->fd_in, STDIN_FILENO);
      close(cmd->fd_in);
    }
    if (cmd->fd_out != STDOUT_FILENO) {
      dup2(cmd->fd_out, STDOUT_FILENO);
      close(cmd->fd_out);
    }
    if (cmd->is_redirect_in) {
      fd = open(cmd->redirect_in, O_RDONLY);
      dup2(fd, STDIN_FILENO);
      close(fd);
    }
    if (cmd->is_redirect_out) {
      fd = open(cmd->redirect_out, O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    setpgid(0, 0);
    execv(cmd->name, cmd->argv);
  } else { // parent process
    if (cmd->fd_in != STDIN_FILENO) close(cmd->fd_in);
    if (cmd->fd_out != STDOUT_FILENO) close(cmd->fd_out);
    AddJob(&bgjobs, pid, cmd->cmdline, (bool)cmd->bg);
    if (cmd->bg != 1) { // foreground job
      tcsetpgrp(STDIN_FILENO, pid);
      waitpid(pid, &status, WUNTRACED);
      tcsetpgrp(STDIN_FILENO, getpgid(0));
      if (WIFSTOPPED(status)) { // job suspended by SIGTSTP
        SuspendJob(bgjobs, pid);
      } else { // job terminated
        //if (kill(pid, 0)) kill(pid, SIGINT);
        DelJob(&bgjobs, pid);
      }
    }
  }
}

static bool IsBuiltIn(char* cmd) {
  int i;
  for (i = 0; i < NBUILTINCOMMANDS; i++) {
    if (!strcmp(cmd, BuiltInCommands[i])) {
      return TRUE;
    }
  }
  return FALSE;
}


static void RunBuiltInCmd(commandT* cmd) {
  if (cmd->fd_in != STDIN_FILENO) {
    dup2(cmd->fd_in, STDIN_FILENO);
    close(cmd->fd_in);
  }
  if (cmd->fd_out != STDOUT_FILENO) {
    dup2(cmd->fd_out, STDOUT_FILENO);
    close(cmd->fd_out);
  }
  if (!strcmp(cmd->argv[0], "alias")) {
    if (cmd->argc == 1) {
      DisplayAlias(alist);
    } else {
      char* cmdline = strdup(cmd->cmdline);
      strsep(&cmdline, " ");
      AddAlias(&alist, cmdline);
    }
  } else if (!strcmp(cmd->argv[0], "cd")) {
    char* dir = NULL;
    char err[256];
    if (cmd->argc == 1) {
      dir = getenv("HOME");
    } else {
      dir = cmd->argv[1];
    }
    sprintf(err, "cd: fail to change directory: %s\n", dir);
    if (chdir(dir)) {
      perror(err);
    }
  } else if (!strcmp(cmd->argv[0], "bg")) {
    if (cmd->argc == 1) BGJob(bgjobs, -1);
    else BGJob(bgjobs, atoi(cmd->argv[1]));
  } else if (!strcmp(cmd->argv[0], "fg")) {
    if (cmd->argc == 1) FGJob(bgjobs, -1);
    else FGJob(bgjobs, atoi(cmd->argv[1]));
  } else if (!strcmp(cmd->argv[0], "jobs")) {
    DisplayJobs(bgjobs);
  } else if (!strcmp(cmd->argv[0], "unalias")) {
    if (cmd->argc == 1) {
      printf("unalias: not enough arguments\n");
    } else {
      DelAlias(&alist, cmd->argv[1]);
    }
  }
  if (cmd->fd_in != STDIN_FILENO) {
    close(cmd->fd_in);
  }
  if (cmd->fd_out != STDOUT_FILENO) {
    close(cmd->fd_out);
  }
}

void CheckJobs() {
  char str_out[100];
  bgjobL* node = bgjobs;
  bgjobL* next;
  if (node) {
    node = node->child;
    while (node != bgjobs) {
      next = node->child;
      if (kill(node->pid, 0)) {
        sprintf(str_out,"[%d]   Done                   %s\n", node->jid, CmdBackSignStrip(node->cmd));
        write(STDOUT_FILENO, str_out, strlen(str_out));
        node->parent->child = node->child;
        node->child->parent = node->parent;
        ReleaseJob(node);
      }
      node = next;
    }
    if (kill(bgjobs->pid, 0)) {
      sprintf(str_out,"[%d]   Done                   %s\n", bgjobs->jid, CmdBackSignStrip(bgjobs->cmd));
      write(STDOUT_FILENO, str_out, strlen(str_out));
      if (bgjobs->child == bgjobs) {
        ReleaseJob(bgjobs);
        bgjobs = NULL;
      } else {
        node = bgjobs;
        bgjobs = bgjobs->child;
        node->parent->child = node->child;
        node->child->parent = node->parent;
        ReleaseJob(node);
      }
    }
  }
}


commandT* CreateCmdT(int n) {
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd) {
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}

aliasL* ReadInAliasList(char* file) {
  char* line = NULL;
  size_t len = 0;
  aliasL* aliasHead = NULL;
  FILE* fp = fopen(file, "r");
  if (fp) {
    while (getline(&line, &len, fp) != -1) {
      line[strlen(line)-1] = 0;
      AddAlias(&aliasHead, line);
    }
    fclose(fp);
    if (line) {
      free(line);
    }
  }
  return aliasHead;
}

bool WriteAliasList(aliasL* head, char* file) {
  FILE* fp;
  aliasL* ptr = head;
  if (!file) {
    return FALSE;
  }
  fp = fopen(file, "w");
  if (fp && head) {
    do {
      fprintf(fp, "%s='%s'\n", ptr->name, ptr->cmd);
      ptr = ptr->child;
    } while (ptr != head);
    fclose(fp);
  }
  return TRUE;
}

void ReleaseAliasList(aliasL* head) {
  if (head) {
    aliasL *a, *b;
    a = head->child;
    if (a == head) {
      ReleaseAlias(head);
    } else {
      do {
        b = a->child;
        ReleaseAlias(a);
        a = b;
      } while (a != head);
      ReleaseAlias(head);
    }
  }
}

void ReleaseAlias(aliasL* node) {
  if (node) {
    if (node->name) free(node->name);
    if (node->cmd) free(node->cmd);
    free(node);
  }
}


void DisplayAlias(aliasL* head) {
  aliasL *ptr = head;
  if (ptr) {
    do {
      printf("alias %s='%s'\n", ptr->name, ptr->cmd);
      ptr = ptr->child;
    } while (ptr != head);
  } else {
    printf("alias: no alias available\n");
  }
}


void AddAlias(aliasL** aliasHeadPtr, char* cmdline) {
  aliasL *aliasNode = (aliasL*)malloc(sizeof(aliasL));
  char* name = NULL;
  char* cmd = strdup(cmdline);
  size_t len = 0;
  name = strsep(&cmd, "=");
  len = strlen(cmd);
  if (name && cmd) {
    if (cmd[0] == '\'') {
      cmd++;
      cmd[len-2] = 0;
    }
    aliasNode = (aliasL*)malloc(sizeof(aliasL));
    aliasNode->name = name;
    aliasNode->cmd = strdup(cmd);
    if (!(*aliasHeadPtr)) {
      aliasNode->child = aliasNode;
      aliasNode->parent = aliasNode;
      *aliasHeadPtr = aliasNode;
    } else {
      aliasNode->child = *aliasHeadPtr;
      aliasNode->parent = (*aliasHeadPtr)->parent;
      (*aliasHeadPtr)->parent->child = aliasNode;
      (*aliasHeadPtr)->parent = aliasNode;
    }
  }
}

void DelAlias(aliasL** aliasHeadPtr, char* cmd) {
  aliasL *ptr = *aliasHeadPtr;
  if (ptr) {
    do {
      if (!strcmp(ptr->name, cmd)) {
        if (ptr == *aliasHeadPtr) {
          if (ptr->child == ptr) {
            *aliasHeadPtr = NULL;
          } else {
            *aliasHeadPtr = ptr->child;
          }
        }
        ptr->parent->child = ptr->child;
        ptr->child->parent = ptr->parent;
        ReleaseAlias(ptr);
        return;
      }
      ptr = ptr->child;
    } while (ptr != *aliasHeadPtr);
  }
  printf("unalias: no such hash table element %s\n", cmd);
}

char* InterpretAlias(aliasL* head, char* cmd) {
  aliasL* a;
  if (head) {
    a = head;
    do {
      if (!strcmp(a->name, cmd)) {
        return strdup(a->cmd);
      }
      a = a->child;
    } while(a != head);
  }
  return NULL;
}

void InitAlias() {
  //alist = ReadInAliasList(TSHRC);
}

void FinAlias() {
  //WriteAliasList(alist, TSHRC);
  ReleaseAliasList(alist);
}

void DisplayJob(bgjobL* jb) {
  char str_out[100];
  sprintf(str_out, "[%d]   %s                %s\n", jb->jid, jb->status, jb->cmd);
  write(STDOUT_FILENO, str_out, strlen(str_out));
}

void DisplayPidJob(bgjobL* bgjobHead, pid_t pid) {
  bgjobL* a = bgjobHead;
  if (a) {
    do {
      if (a->pid == pid) {
        DisplayJob(a);
        return;
      }
      a = a->child;
    } while (a != bgjobHead);
  }
}

void DisplayJobs(bgjobL* bgjobHead) {
  bgjobL* a = bgjobHead;
  if (a) {
    do {
      DisplayJob(a);
      a = a->child;
    } while (a != bgjobHead);
  }
}

void BGJob(bgjobL* bgjobHead, int jid) {
  bgjobL* node = bgjobHead;
  if (!node) {
    printf("bg: no current job\n");
  } else {
    if (jid > 0) {
      do {
        if (node->jid == jid) break;
        node = node->child;
      } while (node != bgjobHead);
      if (node->jid != jid) {
        printf("bg: %d: no such job\n", jid);
        return;
      }
    }
    if (!strcmp(node->status, "Running")) {
      printf("bg: job already in background!\n");
      return;
    }
    kill(node->pid, SIGCONT);
    ResumeJob(bgjobHead, node->pid);
    DisplayJob(node);
  }
}

void FGJob(bgjobL* bgjobHead, int jid) {
  bgjobL* node = bgjobHead;
  int status;
  if (!node) {
    printf("fg: no current job\n");
  } else {
    if (jid > 0) {
      do {
        if (node->jid == jid) break;
        node = node->child;
      } while (node != bgjobHead);
      if (node->jid != jid) {
        printf("fg: %d: no such job\n", jid);
        return;
      }
    }
    if (!strcmp(node->status, "Stopped")) {
      kill(node->pid, SIGCONT);
      ResumeJob(bgjobHead, node->pid);
    }
    DisplayJob(node);
    tcsetpgrp(STDIN_FILENO, node->pid);
    waitpid(node->pid, &status, WUNTRACED);
    tcsetpgrp(STDIN_FILENO, getpgid(0));
    if (WIFSTOPPED(status)) { // job suspended by SIGTSTP
      SuspendJob(bgjobs, node->pid);
    } else { // job terminated
      //if (kill(node->pid, 0)) kill(node->pid, SIGINT);
      DelJob(&bgjobs, node->pid);
    }
  }
}

void KillFGJob() {
  bgjobL* node = bgjobs;
  if (node) {
    do {
      if (!node->isBack) {
        kill(node->pid, SIGINT);
        printf("\n");
        DelJob(&bgjobs, node->pid);
        return;
      }
      node = node->child;
    } while (node != bgjobs);
  }
}

void SuspendFGJob() {
  bgjobL* node = bgjobs;
  if (node) {
    do {
      if (!node->isBack) {
        kill(node->pid, SIGTSTP);
        SuspendJob(bgjobs, node->pid);
        printf("\n");
        DisplayPidJob(bgjobs, node->pid);
        return;
      }
      node = node->child;
    } while (node != bgjobs);
  }
}


void KillAllJobs() {
  bgjobL* head = bgjobs;
  if (head) {
    bgjobL *a, *b;
    a = head;
    do {
      b = a->child;
      kill(a->pid, SIGINT);
      ReleaseJob(a);
      a = b;
    } while (a != head);
  }
}

void AddJob(bgjobL** bgjobHeadPtr, pid_t pid, char* cmd, bool back) {
  bgjobL* node = (bgjobL*)malloc(sizeof(bgjobL));
  node->pid = pid;
  node->status = strdup("Running");
  if (back) {
    char cmd_cpy[100];
    strcpy(cmd_cpy, cmd);
    int len = strlen(cmd_cpy);
    cmd_cpy[len - 1] = ' ';
    cmd_cpy[len] = '&';
    cmd_cpy[len + 1] = 0;
    node->cmd = strdup(cmd_cpy);
  } else {
    node->cmd = strdup(cmd);
  }
  node->isBack = back;
  if (!(*bgjobHeadPtr)) {
    node->child = node;
    node->parent = node;
    node->jid = 1;
    *bgjobHeadPtr = node;
  } else {
    node->child = *bgjobHeadPtr;
    node->parent = (*bgjobHeadPtr)->parent;
    node->jid = (*bgjobHeadPtr)->parent->jid + 1;
    (*bgjobHeadPtr)->parent->child = node;
    (*bgjobHeadPtr)->parent = node;
  }
}

void ReleaseJob(bgjobL* p) {
  if (p) {
    if (p->status) free(p->status);
    if (p->cmd) free(p->cmd);
    free(p);
  }
}

void DelJob(bgjobL** bgjobHeadPtr, pid_t pid) {
  bgjobL *ptr = *bgjobHeadPtr;
  if (ptr) {
    do {
      if (ptr->pid == pid) {
        if (ptr == *bgjobHeadPtr) {
          if (ptr->child == ptr) {
            *bgjobHeadPtr = NULL;
          } else {
            *bgjobHeadPtr = ptr->child;
          }
        }
        ptr->parent->child = ptr->child;
        ptr->child->parent = ptr->parent;
        ReleaseJob(ptr);
        return;
      }
      ptr = ptr->child;
    } while (ptr != *bgjobHeadPtr);
  }
}


void ChangeJobStatus(bgjobL* bgjobHead, pid_t pid, char* s, bool back) {
  bgjobL *ptr = bgjobHead;
  if (ptr) {
    do {
      if (ptr->pid == pid) {
        ptr->isBack = back;
        free(ptr->status);
        ptr->status = strdup(s);
        return;
      }
      ptr = ptr->child;
    } while (ptr != bgjobHead);
  }
}

void SuspendJob(bgjobL* bgjobHead, pid_t pid) {
  ChangeJobStatus(bgjobHead, pid, "Stopped", TRUE);
}


void ResumeJob(bgjobL* bgjobHead, pid_t pid) {
  ChangeJobStatus(bgjobHead, pid, "Running", FALSE);
}

char* CmdBackSignStrip(char* cmd) {
  int len = strlen(cmd);
  if (cmd[len-1] == '&') {
    cmd[len-1] = 0;
  }
  return cmd;
}



