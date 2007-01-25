/*
 * viconf.c
 *
 * $Id: viconf.c 6 2005-09-10 01:02:21Z nenolod $
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include "config.h"


/* wait.h is in /include on solaris, likely on other SYSV machines as well
 * but wait.h is normally in /include/sys on BSD boxen,
 * probably we should have an #ifdef SYSV?
 * -Dianora
 */

#ifdef SOL20
#include <wait.h>
#else
#include <sys/wait.h>
#endif

static int LockedFile(const char *filename);
static char lockpath[PATH_MAX + 1];


int main(int argc, char *argv[])
{
  const char *ed, *p, *filename = CPATH;

  if( chdir(DPATH) < 0 )
    {
      fprintf(stderr,"Cannot chdir to %s\n", DPATH);
      exit(errno);
    }

  if((p = strrchr(argv[0], '/')) == NULL)
    p = argv[0];
  else
    p++;
#ifdef KPATH
  if(strcmp(p, "viklines") == 0)
    filename = KPATH;
#endif /* KPATH */

  if(strcmp(p, "vimotd") == 0)
    filename = MPATH;

  if(LockedFile(filename))
    {
      fprintf(stderr,"Can't lock %s\n", filename);
      exit(errno);
    }

  /* ed config file */
  switch(fork())
    {
    case -1:
      fprintf(stderr, "error forking, %d\n", errno);
      exit(errno);
    case 0:		/* Child */
      if((ed = getenv("EDITOR")) == NULL)
	ed = "vi";
      execlp(ed, ed, filename, NULL);
      fprintf(stderr, "error running editor, %d\n", errno);
      exit(errno);
    default:
      wait(0);
    }

  unlink(lockpath);
  return 0;
}

/*
 * LockedFile() (copied from m_kline.c in ircd)
 * Determine if 'filename' is currently locked. If it is locked,
 * there should be a filename.lock file which contains the current
 * pid of the editing process. Make sure the pid is valid before
 * giving up.
 *
 * Return: 1 if locked
 *         -1 if couldn't unlock
 *         0 if was able to lock
 */



static int
LockedFile(const char *filename)

{

  char buffer[1024];
  FILE *fileptr;
  int killret;
  int fd;

  if (!filename)
    return (0);
  
  sprintf(lockpath, "%s.lock", filename);
  
  if ((fileptr = fopen(lockpath, "r")) != NULL)
    {
      if (fgets(buffer, sizeof(buffer) - 1, fileptr))
	{
	  /*
	   * If it is a valid lockfile, 'buffer' should now
	   * contain the pid number of the editing process.
	   * Send the pid a SIGCHLD to see if it is a valid
	   * pid - it could be a remnant left over from a
	   * crashed editor or system reboot etc.
	   */
      
	  killret = kill(atoi(buffer), SIGCHLD);
	  if (killret == 0)
	    {
	      fclose(fileptr);
	      return (1);
	    }

	  /*
	   * killret must be -1, which indicates an error (most
	   * likely ESRCH - No such process), so it is ok to
	   * proceed writing klines.
	   */
	}
      fclose(fileptr);
    }

  /*
   * Delete the outdated lock file
   */
  unlink(lockpath);

  /* create exclusive lock */
  if((fd = open(lockpath, O_WRONLY|O_CREAT|O_EXCL, 0666)) < 0)
    {
      fprintf(stderr, "ircd config file locked\n");
      return (-1);
    }

  fileptr = fdopen(fd,"w");
  fprintf(fileptr,"%d\n",(int)getpid());
  fclose(fileptr);
  return (0);
} /* LockedFile() */
