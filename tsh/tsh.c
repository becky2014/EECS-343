// Net IDs: 
//
//
/***************************************************************************
 *  Title: MySimpleShell 
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2006/10/13 05:25:59 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: tsh.c,v $
 *    Revision 1.1  2005/10/13 05:25:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.3  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */	
static void sig_handler(int);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

int main (int argc, char *argv[]) {
  /* Initialize command buffer */
  char* cmdLine = malloc(sizeof(char*)*BUFSIZE);
  int taskNum;
  commandT** cmd;

  /* shell initialization */
  if (signal(SIGINT, sig_handler) == SIG_ERR) PrintPError("SIGINT");
  if (signal(SIGTSTP, sig_handler) == SIG_ERR) PrintPError("SIGTSTP");
  if (signal(SIGTTIN, SIG_IGN) == SIG_ERR) PrintPError("SIGTTIN");
  if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) PrintPError("SIGTTOU");
  if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) PrintPError("SIGCHLD");
  InitAlias();
  while (!forceExit) { /* repeat forever */
    /* read command line */
    getCommandLine(&cmdLine, BUFSIZE);
    if(strcmp(cmdLine, "exit") == 0) {
      forceExit=TRUE;
      continue;
    }
    /* checks the status of background jobs */
    CheckJobs();
    /* interpret command and line
     * includes executing of commands */
    cmd = Interpret(cmdLine, &taskNum);
    RunCmd(cmd, taskNum, STDIN_FILENO, STDOUT_FILENO);
  }
  KillAllJobs();
  FinAlias();
  /* shell termination */
  free(cmdLine);
  return 0;
} /* end main */

static void sig_handler(int signo) {
  if (signo == SIGINT) {
    KillFGJob();
  } else if (signo == SIGTSTP) {
    SuspendFGJob();
  }
}

