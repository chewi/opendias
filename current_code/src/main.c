/*
 * main.c
 * Copyright (C) Clearscene Ltd 2008 <wbooth@essentialcollections.co.uk>
 * 
 * main.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sane/sane.h>

#include "main.h"
#include "db.h" 
#include "utils.h"
#include "debug.h"
#include "web_handler.h"
#include "dirconfig.h"

struct MHD_Daemon *httpdaemon;
int pidFilehandle;
int turnToDaemon=1;

int setup (char *configFile) {

  struct simpleLinkedList *rSet;
  char *location, *conf, *sql, *config_option, *config_value;

	o_log(DEBUGM,"setup launched\n");

  // Defaults
  VERBOSITY = DEBUGM;
  DB_VERSION = 6;
  PORT = 8988;
  LOG_DIR = o_printf("%s",LOG_LOCATION);


  // Get 'DB' location
  if (configFile != NULL) {
    conf = configFile;
  } else {
    conf = DEFAULT_CONF_FILE;
  }


  o_log(INFORMATION, "Using config file: %s", conf);
  if( 0 == load_file_to_memory(conf, &location) ) {
    o_log(ERROR, "Cannot find main config file: %s", conf);
    free(location);
    return 1;
  }

  chop(location);
  BASE_DIR = o_strdup(location);
  o_log(INFORMATION, "Which says the database is at: %s", BASE_DIR);

  // Open (& maybe update) the database.
  if(1 == connect_db(1)) { // 1 = create if required
    free(BASE_DIR);
    free(location);
    return 1;
  }

  	o_log(INFORMATION, "database opened");
  sql = o_strdup("SELECT config_option, config_value FROM config");
  rSet = runquery_db(sql);
  if( rSet != NULL ) {
    do {
      config_option = o_strdup(readData_db(rSet, "config_option"));
      config_value = o_strdup(readData_db(rSet, "config_value"));
      o_log(INFORMATION, "Config setting: %s = %s", config_option, config_value);
      if( 0 == strcmp(config_option, "log_verbosity") ) {
        VERBOSITY = atoi(config_value);
      }
      else if ( 0 == strcmp(config_option, "scan_driectory") ) {
        free(location);
        free(BASE_DIR);
        BASE_DIR = o_strdup(config_value);
        location = o_strdup(BASE_DIR);
      }
      else if ( 0 == strcmp(config_option, "port") ) {
        PORT = (unsigned short) atoi(config_value);
      }
      //I don't see the rationale behind as this is the only location 
      // in the source in which the log_directory row of the table config is 
      // used. The database shall not change the log_directory location,
      // as long as the log location is defined at built time.
      // remove it from the database initialization in openDIAS.sqlite3.dmp.v4.sql
      //else if ( 0 == strcmp(config_option, "log_directory") ) {
      //  free(LOG_DIR);
      //  LOG_DIR = o_strdup(config_value);
      //  createDir_ifRequired(LOG_DIR);
      //}
      free(config_option);
      free(config_value);
    } while ( nextRow( rSet ) );
  }
  free_recordset( rSet );
  free(sql);

  createDir_ifRequired(BASE_DIR);
  conCat(&location, "/scans");
  createDir_ifRequired(location);
  free(location);

  return 0;

}

extern void server_shutdown() {
  o_log(INFORMATION, "openDias service is shutting down....");

  o_log(DEBUGM, "cleanup sane");
  sane_exit();

  o_log(DEBUGM, "httpd stop");
	if ( turnToDaemon == 1 ) {
	  MHD_stop_daemon (httpdaemon);
	}

  o_log(DEBUGM, "database close");
  close_db();

  o_log(INFORMATION, "....openDias service has shutdown");
  close(pidFilehandle); 
  o_log(DEBUGM, "pidfile closed");
  free(LOG_DIR); // Cannot log anymore
  free(BASE_DIR);
}

void signal_handler(int sig) {
    char *signame;
    switch(sig) {
        case SIGUSR1:
            o_log(INFORMATION, "Received SIGUSR1 signal.");
            server_shutdown();
            exit(EXIT_SUCCESS);
            break;
        default:
            signame = strsignal(sig);
            o_log(INFORMATION, "Received signal %s. IGNORING. Try SIGUSR1 to stop the service.", signame );
            break;
    }
}
 
void daemonize(char *rundir, char *pidfile) {
    int pid, sid, i;
    char *str;
    size_t size;
    struct sigaction newSigAction;
    sigset_t newSigSet;
 
    /* Check if parent process id is set */
    if (getppid() == 1) {
        /* PPID exists, therefore we are already a daemon */
        o_log(ERROR, "Code called to make this process a daemon, but we are already such.");
        return;
    }
 
    /* Set signal mask - signals we want to block */
    sigemptyset(&newSigSet);
    sigaddset(&newSigSet, SIGCHLD);  /* ignore child - i.e. we don't need to wait for it */
    sigaddset(&newSigSet, SIGTSTP);  /* ignore Tty stop signals */
    sigaddset(&newSigSet, SIGTTOU);  /* ignore Tty background writes */
    sigaddset(&newSigSet, SIGTTIN);  /* ignore Tty background reads */
    sigprocmask(SIG_BLOCK, &newSigSet, NULL);   /* Block the above specified signals */
 
    /* Set up a signal handler */
    newSigAction.sa_handler = signal_handler;
    sigemptyset(&newSigAction.sa_mask);
    newSigAction.sa_flags = 0;
 
    /* Signals to handle */
    sigaction(SIGTERM, &newSigAction, NULL);    /* catch term signal - shutdown the service*/
    sigaction(SIGHUP, &newSigAction, NULL);     /* catch hangup signal */
    sigaction(SIGINT, &newSigAction, NULL);     /* catch interrupt signal */
    sigaction(SIGUSR1, &newSigAction, NULL);    /* catch user 1 signal */
    sigaction(SIGUSR2, &newSigAction, NULL);    /* catch user 2 signal */
 
    /* Fork*/
    pid = fork();
 
    if (pid < 0) {
        /* Could not fork */
        o_log(ERROR, "Could not fork.");
        printf("Could not daemonise [1]. Try running with the -d option or as super user\n");
        exit(EXIT_FAILURE);
    }
 
    if (pid > 0) {
        /* Child created ok, so exit parent process */
        o_log(INFORMATION, "Child process created %d", pid);
        exit(EXIT_SUCCESS);
    }
 
    /* Child continues */

    (void)umask(027); /* Set file permissions 750 ? that's 640 but that's fine */
 
    /* Get a new process group */
    sid = setsid();
    if (sid < 0) {
        o_log(ERROR, "Could not get new process group.");
        printf("Could not daemonise [2]. Try running with the -d option or as super user\n");
        exit(EXIT_FAILURE);
    }
 
    /* Ensure only one copy */
    pidFilehandle = open(pidfile, O_RDWR|O_CREAT, 0600);
 
    if (pidFilehandle == -1 ) {
        /* Couldn't open lock file */
        printf("Could not daemonise [3] pidfile %s. Try running with the -d option or as super user\n",pidfile);
        o_log(ERROR, "Could not open PID lock file. Exiting");
        exit(EXIT_FAILURE);
    }
 
    /* Try to lock file */
    if (lockf(pidFilehandle,F_TLOCK,0) == -1) {
        /* Couldn't get lock on lock file */
        printf("Could not daemonise [4]. Try running with the -d option or as super user\n");
        o_log(ERROR, "Could not lock PID lock file. Exiting");
        exit(EXIT_FAILURE);
    }
 
    /* Get and format PID */
    str = o_printf("%d\n",getpid());
 
    /* write pid to lockfile */
    size = strlen(str);
    if(size != (size_t)write(pidFilehandle, str, size) )
      o_log(ERROR, "Could not write entire data.");

    /* close all descriptors */
    free(str);
    for (i = getdtablesize(); i > 2; --i) {
        close(i);
    }
 
    /* Route I/O connections */
    close(STDIN_FILENO);
    //close(STDOUT_FILENO);
    //close(STDERR_FILENO);

	//open STDOUT and STDOUT again and bind them to null device. (ensure potentially outputted data 
	//can be written properly.
		//if((pid=fork()) == -1)
	
	int devnull;	
	if ( (devnull=open("/dev/null",O_APPEND)) == -1 ) {
		o_log(ERROR,"cannot open /dev/null");
		exit(1);
	}

	dup2(devnull,STDOUT_FILENO);
	dup2(devnull,STDERR_FILENO);

 
    i = chdir(rundir); /* change running directory */
}

void usage(void) {
    fprintf(stderr,"openDIAS. v%s\n", PACKAGE_VERSION);
    fprintf(stderr,"usage: opendias <options>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Where:\n");
    fprintf(stderr,"          -d = don't turn into a daemon once started\n");
    fprintf(stderr,"          -c = specify config \"file\"\n");
    fprintf(stderr,"          -h = show this page\n");
}

int main (int argc, char **argv) {

  char *configFile = NULL;
	
  	//declare globally to ensure graceful shutdown if the server is not running as daemon     int turnToDaemon = 1;
  int c;

  while ((c = getopt(argc, argv, "dc:ih")) != EOF) {
    switch (c) {
      case 'c':
        configFile = optarg;
        break;
      case 'd': 
        turnToDaemon = 0;
        break;
      case 'h': 
        usage();
        return 0;
        break;
      default: usage();
    }
  }

  if(turnToDaemon==1) {
    // Turn into a meamon and write the pid file.
    o_log(INFORMATION, "Running in daemon mode.");
    daemonize("/tmp/", PIDFILE);
  }
  else {
    o_log(INFORMATION, "Running in interactive mode.");
  }


  if(setup (configFile))
    return 1;

	o_log(DEBUGM,"config retrieved");

  o_log(INFORMATION, "... Starting up the openDias service.");
  httpdaemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, 
    NULL, NULL, 
    &answer_to_connection, NULL, 
    MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL, 
    MHD_OPTION_END);
  if (NULL == httpdaemon) {
    o_log(INFORMATION, "Could not create an http service");
    if(turnToDaemon!=1) 
      printf("Could not create an http service. Port is already in use?.\n");
    server_shutdown();
    exit(EXIT_FAILURE);
  }
  o_log(INFORMATION, "ready to accept connectons");

  
  if(turnToDaemon==1) {
    while(1) {
      sleep(500);
    }
    o_log(ERROR, "done waiting - should never get here");
  }
  else {
    printf("Hit [enter] to close the service.\n");
    getchar();
    server_shutdown();
    exit(EXIT_SUCCESS);
  }

}

