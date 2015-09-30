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

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

static char* BuiltInCommands[] = {
  "alias", "bg", "bind", "builtin", "caller", "cd", "command",
  "declare", "echo", "enable", "fg", "help", "jobs", "let",
  "local", "logout", "mapfile", "printf", "read", "readarray",
  "source", "type", "typeset", "ulimit", "unalias"
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
  struct bgjob_l* next;
} bgjobL;

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
/* the alias bilink list */
aliasL *alist = NULL;


/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
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
aliasL* ReadInAlias(char*);
/* write alias to file */
bool WriteAlias(aliasL*, char*);
/* release alias */
bool ReleaseAlias(aliasL*);
/* alias to string */
void DisplayAlias(aliasL*);
/* add alias */
void AddAlias(aliasL**, char*);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
void RunCmd(commandT** cmd, int n) {
  int i;
  int fd[2] = {0, 1};
  for (i = 0; i < n; i++) {
    cmd[i]->fd_in = fd[0];
    if (i == n-1) {
      cmd[i]->fd_out = STDOUT_FILENO;
    } else {
      pipe(fd);
      cmd[i]->fd_out = fd[1];
    }
    RunCmdFork(cmd[i], TRUE);
    ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork) {
  if (cmd->argc<=0) {
    return;
  }
  if (IsBuiltIn(cmd->argv[0])) {
    RunBuiltInCmd(cmd);
  } else {
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd) {
  // TODO
}

void RunCmdPipe(commandT** cmd, int n) {
}

void RunCmdRedirOut(commandT* cmd, char* file) {
}

void RunCmdRedirIn(commandT* cmd, char* file) {
}


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
      fd = open(cmd->redirect_out, O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
      dup2(fd, STDOUT_FILENO);
      close(fd);
    }
    if (cmd->bg != 1) {
      tcsetpgrp(STDIN_FILENO, getpid());
    }
    setpgid(0, 0);
    execv(cmd->name, cmd->argv);
  } else { // parent process
    if (cmd->fd_in != STDIN_FILENO) {
      close(cmd->fd_in);
    }
    if (cmd->fd_out != STDOUT_FILENO) {
      close(cmd->fd_out);
    }
    if (cmd->bg == 1) { // background job
    } else { // foreground job
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

  } else if (!strcmp(cmd->argv[0], "fg")) {

  } else if (!strcmp(cmd->argv[0], "jobs")) {

  } else if (!strcmp(cmd->argv[0], "unalias")) {
    if (cmd->argc == 1) {
      printf("unalias: not enough arguments\n");
    } else {

    }
  }
}

void CheckJobs() {
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

aliasL* ReadInAlias(char* file) {
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

bool WriteAlias(aliasL* head, char* file) {
  FILE* fp;
  aliasL* ptr = head;
  if (!head || !file) {
    return FALSE;
  }
  fp = fopen(file, "w");
  if (fp) {
    do {
      fprintf(fp, "%s='%s'\n", ptr->name, ptr->cmd);
      ptr = ptr->child;
    } while (ptr != head);
    fclose(fp);
  }
  return TRUE;
}

bool ReleaseAlias(aliasL* head) {
  if (!head) {
    return FALSE;
  }
  aliasL *a;
  a = head->child;
  while (a != head) {
    free(a->parent);
    a = a->child;
  }
  free(head);
  return TRUE;
}


void DisplayAlias(aliasL* head) {
  aliasL *ptr = head;
  if (ptr) {
    do {
      printf("%s='%s'\n", ptr->name, ptr->cmd);
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
    aliasNode->cmd = cmd;
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

void InitAlias() {
  alist = ReadInAlias(TSHRC);
}

void FinAlias() {
  WriteAlias(alist, TSHRC);
  ReleaseAlias(alist);
}



