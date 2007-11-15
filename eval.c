/* eval.c -- reading and evaluating commands. */

/* Copyright (C) 1996 Free Software Foundation, Inc.

   This file is part of GNU Bash, the Bourne Again SHell.

   Bash is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Bash is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with Bash; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place, Suite 330, Boston, MA 02111 USA. */

#include "config.h"

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include "bashansi.h"
#include <stdio.h>
#include <dirent.h>

#include "bashintl.h"

#include "shell.h"
#include "flags.h"
#include "trap.h"

#include "builtins/common.h"

#include "input.h"
#include "execute_cmd.h"

#if defined (HISTORY)
#  include "bashhist.h"
#endif

extern int EOF_reached;
extern int indirection_level;
extern int posixly_correct;
extern int subshell_environment, running_under_emacs;
extern int last_command_exit_value, stdin_redir;
extern int need_here_doc;
extern int current_command_number, current_command_line_count, line_number;
extern int expand_aliases;

static void send_pwd_to_eterm __P((void));
static sighandler alrm_catcher __P((int));

#if defined (READLINE)
extern char *current_readline_line;
extern int current_readline_line_index;
#endif

/* Read and execute commands until EOF is reached.  This assumes that
   the input source has already been initialized. */
int
reader_loop ()
{
  int our_indirection_level;
  COMMAND * volatile current_command;

  current_command = (COMMAND *)NULL;
  USE_VAR(current_command);

  our_indirection_level = ++indirection_level;

  while (EOF_Reached == 0)
    {
      int code;

      code = setjmp (top_level);

#if defined (PROCESS_SUBSTITUTION)
      unlink_fifo_list ();
#endif /* PROCESS_SUBSTITUTION */

      if (interactive_shell && signal_is_ignored (SIGINT) == 0)
	set_signal_handler (SIGINT, sigint_sighandler);

      if (code != NOT_JUMPED)
	{
	  indirection_level = our_indirection_level;

	  switch (code)
	    {
	      /* Some kind of throw to top_level has occured. */
	    case FORCE_EOF:
	    case ERREXIT:
	    case EXITPROG:
	      current_command = (COMMAND *)NULL;
	      if (exit_immediately_on_error)
		variable_context = 0;	/* not in a function */
	      EOF_Reached = EOF;
	      goto exec_done;

	    case DISCARD:
	      last_command_exit_value = 1;
	      if (subshell_environment)
		{
		  current_command = (COMMAND *)NULL;
		  EOF_Reached = EOF;
		  goto exec_done;
		}
	      /* Obstack free command elements, etc. */
	      if (current_command)
		{
		  dispose_command (current_command);
		  current_command = (COMMAND *)NULL;
		}
	      break;

	    default:
	      command_error ("reader_loop", CMDERR_BADJUMP, code, 0);
	    }
	}

      executing = 0;
      if (temporary_env)
	dispose_used_env_vars ();

#if (defined (ultrix) && defined (mips)) || defined (C_ALLOCA)
      /* Attempt to reclaim memory allocated with alloca (). */
      (void) alloca (0);
#endif

      if (read_command () == 0)
	{
	  if (interactive_shell == 0 && read_but_dont_execute)
	    {
	      last_command_exit_value = EXECUTION_SUCCESS;
	      dispose_command (global_command);
	      global_command = (COMMAND *)NULL;
	    }
	  else if (current_command = global_command)
	    {
	      global_command = (COMMAND *)NULL;
	      current_command_number++;

	      executing = 1;
	      stdin_redir = 0;
	      execute_command (current_command);

	    exec_done:
	      QUIT;

	      if (current_command)
		{
		  dispose_command (current_command);
		  current_command = (COMMAND *)NULL;
		}
	    }
	}
      else
	{
	  /* Parse error, maybe discard rest of stream if not interactive. */
	  if (interactive == 0)
	    EOF_Reached = EOF;
	}
      if (just_one_command)
	EOF_Reached = EOF;
    }
  indirection_level--;
  return (last_command_exit_value);
}

static sighandler
alrm_catcher(i)
     int i;
{
  printf (_("\007timed out waiting for input: auto-logout\n"));
  bash_logout ();	/* run ~/.bash_logout if this is a login shell */
  jump_to_top_level (EXITPROG);
  SIGRETURN (0);
}

/* Send an escape sequence to emacs term mode to tell it the
   current working directory. */
static void
send_pwd_to_eterm ()
{
  char *pwd;

  pwd = get_string_value ("PWD");
  if (pwd == 0)
    pwd = get_working_directory ("eterm");
  fprintf (stderr, "\032/%s\n", pwd);
}

static int
is_in_command_list(const char *cmd, const char *cmds[])
{
  int idx = 0;
  for (idx = 0; cmds[idx]; idx++) {
    if (strcmp(cmd, cmds[idx]) == 0) {
      return 1;
    }
  }
  return 0;
}

static int
is_vyatta_cfg_command(const char *cmd)
{
  char *valid_commands[] = { "set", "delete", "commit", "save", "load",
                             "show", "exit", "edit", "run", NULL };
  return is_in_command_list(cmd, valid_commands);
}

static int
is_vyatta_op_command(const char *cmd)
{
  char *dir = getenv("vyatta_op_templates");
  DIR *dp = NULL;
  struct dirent *dent = NULL;
  char *other_commands[] = { "exit", NULL };
  int ret = 0;

  if (dir == NULL || (dp = opendir(dir)) == NULL) {
    return 0;
  }
  while (dent = readdir(dp)) {
    if (strncmp(dent->d_name, ".", 1) == 0) {
      continue;
    }
    if (strcmp(dent->d_name, cmd) == 0) {
      ret = 1;
      break;
    }
  }
  closedir(dp);
  return (ret) ? 1 : is_in_command_list(cmd, other_commands);
}

static int
is_vyatta_command(char *cmdline)
{
  char *cfg = getenv("_OFR_CONFIGURE");
  int in_cfg = (cfg) ? (strcmp(cfg, "ok") == 0) : 0;
  char *start = cmdline;
  char *end = NULL;
  char save = 0;
  int ret = 0;
  while (*start && (whitespace(*start) || *start == '\n')) {
    start++;
  }
  if (*start == 0) {
    /* empty command line */
    return 1;
  }
  end = start;
  while (*end && (!whitespace(*end) && *end != '\n')) {
    end++;
  }
  save = *end;
  *end = 0;

  if (in_cfg) {
    ret = is_vyatta_cfg_command(start);
  } else {
    ret = is_vyatta_op_command(start);
  }
  *end = save;
  return ret;
}

/* Call the YACC-generated parser and return the status of the parse.
   Input is read from the current input stream (bash_input).  yyparse
   leaves the parsed command in the global variable GLOBAL_COMMAND.
   This is where PROMPT_COMMAND is executed. */
int
parse_command ()
{
  int r;
  char *command_to_execute;

  need_here_doc = 0;
  run_pending_traps ();

  /* Allow the execution of a random command just before the printing
     of each primary prompt.  If the shell variable PROMPT_COMMAND
     is set then the value of it is the command to execute. */
  if (interactive && bash_input.type != st_string)
    {
      command_to_execute = get_string_value ("PROMPT_COMMAND");
      if (command_to_execute)
	execute_prompt_command (command_to_execute);

      if (running_under_emacs == 2)
	send_pwd_to_eterm ();	/* Yuck */
    }

  current_command_line_count = 0;
  r = yyparse ();

#if defined (READLINE)
  if (in_vyatta_restricted_mode(FULL) && current_readline_line) {
    if (!is_vyatta_command(current_readline_line)) {
      printf("Invalid command\n");
      current_readline_line_index = 0;
      current_readline_line[0] = '\n';
      current_readline_line[1] = '\0';
      return 1;
    }
  }
#endif

  if (need_here_doc)
    gather_here_documents ();

  return (r);
}

/* Read and parse a command, returning the status of the parse.  The command
   is left in the globval variable GLOBAL_COMMAND for use by reader_loop.
   This is where the shell timeout code is executed. */
int
read_command ()
{
  SHELL_VAR *tmout_var;
  int tmout_len, result;
  SigHandler *old_alrm;

  set_current_prompt_level (1);
  global_command = (COMMAND *)NULL;

  /* Only do timeouts if interactive. */
  tmout_var = (SHELL_VAR *)NULL;
  tmout_len = 0;
  old_alrm = (SigHandler *)NULL;

  if (interactive)
    {
      tmout_var = find_variable ("TMOUT");

      if (tmout_var && var_isset (tmout_var))
	{
	  tmout_len = atoi (value_cell (tmout_var));
	  if (tmout_len > 0)
	    {
	      old_alrm = set_signal_handler (SIGALRM, alrm_catcher);
	      alarm (tmout_len);
	    }
	}
    }

  QUIT;

  current_command_line_count = 0;
  result = parse_command ();

  if (interactive && tmout_var && (tmout_len > 0))
    {
      alarm(0);
      set_signal_handler (SIGALRM, old_alrm);
    }

  return (result);
}
