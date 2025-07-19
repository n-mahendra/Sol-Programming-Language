/*
** $Id: sol.c $
** Sol stand-alone interpreter
** See Copyright Notice in sol.h
*/

#define sol_c

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>

#include "sol.h"

#include "lauxlib.h"
#include "sollib.h"


#if !defined(SOL_PROGNAME)
#define SOL_PROGNAME		"sol"
#endif

#if !defined(SOL_INIT_VAR)
#define SOL_INIT_VAR		"SOL_INIT"
#endif

#define SOL_INITVARVERSION	SOL_INIT_VAR SOL_VERSUFFIX


static sol_State *globalL = NULL;

static const char *progname = SOL_PROGNAME;


#if defined(SOL_USE_POSIX)   /* { */

/*
** Use 'sigaction' when available.
*/
static void setsignal (int sig, void (*handler)(int)) {
  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);  /* do not mask any signal */
  sigaction(sig, &sa, NULL);
}

#else           /* }{ */

#define setsignal            signal

#endif                               /* } */


/*
** Hook set by signal function to stop the interpreter.
*/
static void lstop (sol_State *L, sol_Debug *ar) {
  (void)ar;  /* unused arg. */
  sol_sethook(L, NULL, 0, 0);  /* reset hook */
  solL_error(L, "interrupted!");
}


/*
** Function to be called at a C signal. Because a C signal cannot
** just change a Sol state (as there is no proper synchronization),
** this function only sets a hook that, when called, will stop the
** interpreter.
*/
static void laction (int i) {
  int flag = SOL_MASKCALL | SOL_MASKRET | SOL_MASKLINE | SOL_MASKCOUNT;
  setsignal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
  sol_sethook(globalL, lstop, flag, 1);
}


static void print_usage (const char *badoption) {
  sol_writestringerror("%s: ", progname);
  if (badoption[1] == 'e' || badoption[1] == 'l')
    sol_writestringerror("'%s' needs argument\n", badoption);
  else
    sol_writestringerror("unrecognized option '%s'\n", badoption);
  sol_writestringerror(
  "usage: %s [options] [script [args]]\n"
  "Available options are:\n"
  "  -e stat   execute string 'stat'\n"
  "  -i        enter interactive mode after executing 'script'\n"
  "  -l mod    require library 'mod' into global 'mod'\n"
  "  -l g=mod  require library 'mod' into global 'g'\n"
  "  -v        show version information\n"
  "  -E        ignore environment variables\n"
  "  -W        turn warnings on\n"
  "  --        stop handling options\n"
  "  -         stop handling options and execute stdin\n"
  ,
  progname);
}


/*
** Prints an error message, adding the program name in front of it
** (if present)
*/
static void l_message (const char *pname, const char *msg) {
  if (pname) sol_writestringerror("%s: ", pname);
  sol_writestringerror("%s\n", msg);
}


/*
** Check whether 'status' is not OK and, if so, prints the error
** message on the top of the stack.
*/
static int report (sol_State *L, int status) {
  if (status != SOL_OK) {
    const char *msg = sol_tostring(L, -1);
    if (msg == NULL)
      msg = "(error message not a string)";
    l_message(progname, msg);
    sol_pop(L, 1);  /* remove message */
  }
  return status;
}


/*
** Message handler used to run all chunks
*/
static int msghandler (sol_State *L) {
  const char *msg = sol_tostring(L, 1);
  if (msg == NULL) {  /* is error object not a string? */
    if (solL_callmeta(L, 1, "__tostring") &&  /* does it have a metamethod */
        sol_type(L, -1) == SOL_TSTRING)  /* that produces a string? */
      return 1;  /* that is the message */
    else
      msg = sol_pushfstring(L, "(error object is a %s value)",
                               solL_typename(L, 1));
  }
  solL_traceback(L, L, msg, 1);  /* append a standard traceback */
  return 1;  /* return the traceback */
}


/*
** Interface to 'sol_pcall', which sets appropriate message function
** and C-signal handler. Used to run all chunks.
*/
static int docall (sol_State *L, int narg, int nres) {
  int status;
  int base = sol_gettop(L) - narg;  /* function index */
  sol_pushcfunction(L, msghandler);  /* push message handler */
  sol_insert(L, base);  /* put it under function and args */
  globalL = L;  /* to be available to 'laction' */
  setsignal(SIGINT, laction);  /* set C-signal handler */
  status = sol_pcall(L, narg, nres, base);
  setsignal(SIGINT, SIG_DFL); /* reset C-signal handler */
  sol_remove(L, base);  /* remove message handler from the stack */
  return status;
}


static void print_version (void) {
  sol_writestring(SOL_COPYRIGHT, strlen(SOL_COPYRIGHT));
  sol_writeline();
}


/*
** Create the 'arg' table, which stores all arguments from the
** command line ('argv'). It should be aligned so that, at index 0,
** it has 'argv[script]', which is the script name. The arguments
** to the script (everything after 'script') go to positive indices;
** other arguments (before the script name) go to negative indices.
** If there is no script name, assume interpreter's name as base.
** (If there is no interpreter's name either, 'script' is -1, so
** table sizes are zero.)
*/
static void createargtable (sol_State *L, char **argv, int argc, int script) {
  int i, narg;
  narg = argc - (script + 1);  /* number of positive indices */
  sol_createtable(L, narg, script + 1);
  for (i = 0; i < argc; i++) {
    sol_pushstring(L, argv[i]);
    sol_rawseti(L, -2, i - script);
  }
  sol_setglobal(L, "arg");
}


static int dochunk (sol_State *L, int status) {
  if (status == SOL_OK) status = docall(L, 0, 0);
  return report(L, status);
}


static int dofile (sol_State *L, const char *name) {
  return dochunk(L, solL_loadfile(L, name));
}


static int dostring (sol_State *L, const char *s, const char *name) {
  return dochunk(L, solL_loadbuffer(L, s, strlen(s), name));
}


/*
** Receives 'globname[=modname]' and runs 'globname = require(modname)'.
** If there is no explicit modname and globname contains a '-', cut
** the suffix after '-' (the "version") to make the global name.
*/
static int dolibrary (sol_State *L, char *globname) {
  int status;
  char *suffix = NULL;
  char *modname = strchr(globname, '=');
  if (modname == NULL) {  /* no explicit name? */
    modname = globname;  /* module name is equal to global name */
    suffix = strchr(modname, *SOL_IGMARK);  /* look for a suffix mark */
  }
  else {
    *modname = '\0';  /* global name ends here */
    modname++;  /* module name starts after the '=' */
  }
  sol_getglobal(L, "require");
  sol_pushstring(L, modname);
  status = docall(L, 1, 1);  /* call 'require(modname)' */
  if (status == SOL_OK) {
    if (suffix != NULL)  /* is there a suffix mark? */
      *suffix = '\0';  /* remove suffix from global name */
    sol_setglobal(L, globname);  /* globname = require(modname) */
  }
  return report(L, status);
}


/*
** Push on the stack the contents of table 'arg' from 1 to #arg
*/
static int pushargs (sol_State *L) {
  int i, n;
  if (sol_getglobal(L, "arg") != SOL_TTABLE)
    solL_error(L, "'arg' is not a table");
  n = (int)solL_len(L, -1);
  solL_checkstack(L, n + 3, "too many arguments to script");
  for (i = 1; i <= n; i++)
    sol_rawgeti(L, -i, i);
  sol_remove(L, -i);  /* remove table from the stack */
  return n;
}


static int handle_script (sol_State *L, char **argv) {
  int status;
  const char *fname = argv[0];
  if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
    fname = NULL;  /* stdin */
  status = solL_loadfile(L, fname);
  if (status == SOL_OK) {
    int n = pushargs(L);  /* push arguments to script */
    status = docall(L, n, SOL_MULTRET);
  }
  return report(L, status);
}


/* bits of various argument indicators in 'args' */
#define has_error	1	/* bad option */
#define has_i		2	/* -i */
#define has_v		4	/* -v */
#define has_e		8	/* -e */
#define has_E		16	/* -E */


/*
** Traverses all arguments from 'argv', returning a mask with those
** needed before running any Sol code or an error code if it finds any
** invalid argument. In case of error, 'first' is the index of the bad
** argument.  Otherwise, 'first' is -1 if there is no program name,
** 0 if there is no script name, or the index of the script name.
*/
static int collectargs (char **argv, int *first) {
  int args = 0;
  int i;
  if (argv[0] != NULL) {  /* is there a program name? */
    if (argv[0][0])  /* not empty? */
      progname = argv[0];  /* save it */
  }
  else {  /* no program name */
    *first = -1;
    return 0;
  }
  for (i = 1; argv[i] != NULL; i++) {  /* handle arguments */
    *first = i;
    if (argv[i][0] != '-')  /* not an option? */
        return args;  /* stop handling options */
    switch (argv[i][1]) {  /* else check option */
      case '-':  /* '--' */
        if (argv[i][2] != '\0')  /* extra characters after '--'? */
          return has_error;  /* invalid option */
        *first = i + 1;
        return args;
      case '\0':  /* '-' */
        return args;  /* script "name" is '-' */
      case 'E':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_E;
        break;
      case 'W':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        break;
      case 'i':
        args |= has_i;  /* (-i implies -v) *//* FALLTHROUGH */
      case 'v':
        if (argv[i][2] != '\0')  /* extra characters? */
          return has_error;  /* invalid option */
        args |= has_v;
        break;
      case 'e':
        args |= has_e;  /* FALLTHROUGH */
      case 'l':  /* both options need an argument */
        if (argv[i][2] == '\0') {  /* no concatenated argument? */
          i++;  /* try next 'argv' */
          if (argv[i] == NULL || argv[i][0] == '-')
            return has_error;  /* no next argument or it is another option */
        }
        break;
      default:  /* invalid option */
        return has_error;
    }
  }
  *first = 0;  /* no script name */
  return args;
}


/*
** Processes options 'e' and 'l', which involve running Sol code, and
** 'W', which also affects the state.
** Returns 0 if some code raises an error.
*/
static int runargs (sol_State *L, char **argv, int n) {
  int i;
  for (i = 1; i < n; i++) {
    int option = argv[i][1];
    sol_assert(argv[i][0] == '-');  /* already checked */
    switch (option) {
      case 'e':  case 'l': {
        int status;
        char *extra = argv[i] + 2;  /* both options need an argument */
        if (*extra == '\0') extra = argv[++i];
        sol_assert(extra != NULL);
        status = (option == 'e')
                 ? dostring(L, extra, "=(command line)")
                 : dolibrary(L, extra);
        if (status != SOL_OK) return 0;
        break;
      }
      case 'W':
        sol_warning(L, "@on", 0);  /* warnings on */
        break;
    }
  }
  return 1;
}


static int handle_solinit (sol_State *L) {
  const char *name = "=" SOL_INITVARVERSION;
  const char *init = getenv(name + 1);
  if (init == NULL) {
    name = "=" SOL_INIT_VAR;
    init = getenv(name + 1);  /* try alternative name */
  }
  if (init == NULL) return SOL_OK;
  else if (init[0] == '@')
    return dofile(L, init+1);
  else
    return dostring(L, init, name);
}


/*
** {==================================================================
** Read-Eval-Print Loop (REPL)
** ===================================================================
*/

#if !defined(SOL_PROMPT)
#define SOL_PROMPT		"> "
#define SOL_PROMPT2		">> "
#endif

#if !defined(SOL_MAXINPUT)
#define SOL_MAXINPUT		512
#endif


/*
** sol_stdin_is_tty detects whether the standard input is a 'tty' (that
** is, whether we're running sol interactively).
*/
#if !defined(sol_stdin_is_tty)	/* { */

#if defined(SOL_USE_POSIX)	/* { */

#include <unistd.h>
#define sol_stdin_is_tty()	isatty(0)

#elif defined(SOL_USE_WINDOWS)	/* }{ */

#include <io.h>
#include <windows.h>

#define sol_stdin_is_tty()	_isatty(_fileno(stdin))

#else				/* }{ */

/* ISO C definition */
#define sol_stdin_is_tty()	1  /* assume stdin is a tty */

#endif				/* } */

#endif				/* } */


/*
** sol_readline defines how to show a prompt and then read a line from
** the standard input.
** sol_saveline defines how to "save" a read line in a "history".
** sol_freeline defines how to free a line read by sol_readline.
*/
#if !defined(sol_readline)	/* { */

#if defined(SOL_USE_READLINE)	/* { */

#include <readline/readline.h>
#include <readline/history.h>
#define sol_initreadline(L)	((void)L, rl_readline_name="sol")
#define sol_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define sol_saveline(L,line)	((void)L, add_history(line))
#define sol_freeline(L,b)	((void)L, free(b))

#else				/* }{ */

#define sol_initreadline(L)  ((void)L)
#define sol_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, SOL_MAXINPUT, stdin) != NULL)  /* get line */
#define sol_saveline(L,line)	{ (void)L; (void)line; }
#define sol_freeline(L,b)	{ (void)L; (void)b; }

#endif				/* } */

#endif				/* } */


/*
** Return the string to be used as a prompt by the interpreter. Leave
** the string (or nil, if using the default value) on the stack, to keep
** it anchored.
*/
static const char *get_prompt (sol_State *L, int firstline) {
  if (sol_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2") == SOL_TNIL)
    return (firstline ? SOL_PROMPT : SOL_PROMPT2);  /* use the default */
  else {  /* apply 'tostring' over the value */
    const char *p = solL_tolstring(L, -1, NULL);
    sol_remove(L, -2);  /* remove original value */
    return p;
  }
}

/* mark in error messages for incomplete statements */
#define EOFMARK		"<eof>"
#define marklen		(sizeof(EOFMARK)/sizeof(char) - 1)


/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete (sol_State *L, int status) {
  if (status == SOL_ERRSYNTAX) {
    size_t lmsg;
    const char *msg = sol_tolstring(L, -1, &lmsg);
    if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0)
      return 1;
  }
  return 0;  /* else... */
}


/*
** Prompt the user, read a line, and push it into the Sol stack.
*/
static int pushline (sol_State *L, int firstline) {
  char buffer[SOL_MAXINPUT];
  char *b = buffer;
  size_t l;
  const char *prmt = get_prompt(L, firstline);
  int readstatus = sol_readline(L, b, prmt);
  sol_pop(L, 1);  /* remove prompt */
  if (readstatus == 0)
    return 0;  /* no input */
  l = strlen(b);
  if (l > 0 && b[l-1] == '\n')  /* line ends with newline? */
    b[--l] = '\0';  /* remove it */
  if (firstline && b[0] == '=')  /* for compatibility with 5.2, ... */
    sol_pushfstring(L, "return %s", b + 1);  /* change '=' to 'return' */
  else
    sol_pushlstring(L, b, l);
  sol_freeline(L, b);
  return 1;
}


/*
** Try to compile line on the stack as 'return <line>;'; on return, stack
** has either compiled chunk or original line (if compilation failed).
*/
static int addreturn (sol_State *L) {
  const char *line = sol_tostring(L, -1);  /* original line */
  const char *retline = sol_pushfstring(L, "return %s;", line);
  int status = solL_loadbuffer(L, retline, strlen(retline), "=stdin");
  if (status == SOL_OK) {
    sol_remove(L, -2);  /* remove modified line */
    if (line[0] != '\0')  /* non empty? */
      sol_saveline(L, line);  /* keep history */
  }
  else
    sol_pop(L, 2);  /* pop result from 'solL_loadbuffer' and modified line */
  return status;
}


/*
** Read multiple lines until a complete Sol statement
*/
static int multiline (sol_State *L) {
  for (;;) {  /* repeat until gets a complete statement */
    size_t len;
    const char *line = sol_tolstring(L, 1, &len);  /* get what it has */
    int status = solL_loadbuffer(L, line, len, "=stdin");  /* try it */
    if (!incomplete(L, status) || !pushline(L, 0)) {
      sol_saveline(L, line);  /* keep history */
      return status;  /* should not or cannot try to add continuation line */
    }
    sol_remove(L, -2);  /* remove error message (from incomplete line) */
    sol_pushliteral(L, "\n");  /* add newline... */
    sol_insert(L, -2);  /* ...between the two lines */
    sol_concat(L, 3);  /* join them */
  }
}


/*
** Read a line and try to load (compile) it first as an expression (by
** adding "return " in front of it) and second as a statement. Return
** the final status of load/call with the resulting function (if any)
** in the top of the stack.
*/
static int loadline (sol_State *L) {
  int status;
  sol_settop(L, 0);
  if (!pushline(L, 1))
    return -1;  /* no input */
  if ((status = addreturn(L)) != SOL_OK)  /* 'return ...' did not work? */
    status = multiline(L);  /* try as command, maybe with continuation lines */
  sol_remove(L, 1);  /* remove line from the stack */
  sol_assert(sol_gettop(L) == 1);
  return status;
}


/*
** Prints (calling the Sol 'print' function) any values on the stack
*/
static void l_print (sol_State *L) {
  int n = sol_gettop(L);
  if (n > 0) {  /* any result to be printed? */
    solL_checkstack(L, SOL_MINSTACK, "too many results to print");
    sol_getglobal(L, "print");
    sol_insert(L, 1);
    if (sol_pcall(L, n, 0, 0) != SOL_OK)
      l_message(progname, sol_pushfstring(L, "error calling 'print' (%s)",
                                             sol_tostring(L, -1)));
  }
}


/*
** Do the REPL: repeatedly read (load) a line, evasolte (call) it, and
** print any results.
*/
static void doREPL (sol_State *L) {
  int status;
  const char *oldprogname = progname;
  progname = NULL;  /* no 'progname' on errors in interactive mode */
  sol_initreadline(L);
  while ((status = loadline(L)) != -1) {
    if (status == SOL_OK)
      status = docall(L, 0, SOL_MULTRET);
    if (status == SOL_OK) l_print(L);
    else report(L, status);
  }
  sol_settop(L, 0);  /* clear stack */
  sol_writeline();
  progname = oldprogname;
}

/* }================================================================== */


/*
** Main body of stand-alone interpreter (to be called in protected mode).
** Reads the options and handles them all.
*/
static int pmain (sol_State *L) {
  int argc = (int)sol_tointeger(L, 1);
  char **argv = (char **)sol_touserdata(L, 2);
  int script;
  int args = collectargs(argv, &script);
  int optlim = (script > 0) ? script : argc; /* first argv not an option */
  solL_checkversion(L);  /* check that interpreter has correct version */
  if (args == has_error) {  /* bad arg? */
    print_usage(argv[script]);  /* 'script' has index of bad arg. */
    return 0;
  }
  if (args & has_v)  /* option '-v'? */
    print_version();
  if (args & has_E) {  /* option '-E'? */
    sol_pushboolean(L, 1);  /* signal for libraries to ignore env. vars. */
    sol_setfield(L, SOL_REGISTRYINDEX, "SOL_NOENV");
  }
  solL_openlibs(L);  /* open standard libraries */
  createargtable(L, argv, argc, script);  /* create table 'arg' */
  sol_gc(L, SOL_GCRESTART);  /* start GC... */
  sol_gc(L, SOL_GCGEN, 0, 0);  /* ...in generational mode */
  if (!(args & has_E)) {  /* no option '-E'? */
    if (handle_solinit(L) != SOL_OK)  /* run SOL_INIT */
      return 0;  /* error running SOL_INIT */
  }
  if (!runargs(L, argv, optlim))  /* execute arguments -e and -l */
    return 0;  /* something failed */
  if (script > 0) {  /* execute main script (if there is one) */
    if (handle_script(L, argv + script) != SOL_OK)
      return 0;  /* interrupt in case of error */
  }
  if (args & has_i)  /* -i option? */
    doREPL(L);  /* do read-eval-print loop */
  else if (script < 1 && !(args & (has_e | has_v))) { /* no active option? */
    if (sol_stdin_is_tty()) {  /* running in interactive mode? */
      print_version();
      doREPL(L);  /* do read-eval-print loop */
    }
    else dofile(L, NULL);  /* executes stdin as a file */
  }
  sol_pushboolean(L, 1);  /* signal no errors */
  return 1;
}


int main (int argc, char **argv) {
  int status, result;
  sol_State *L = solL_newstate();  /* create state */
  if (L == NULL) {
    l_message(argv[0], "cannot create state: not enough memory");
    return EXIT_FAILURE;
  }
  sol_gc(L, SOL_GCSTOP);  /* stop GC while building state */
  sol_pushcfunction(L, &pmain);  /* to call 'pmain' in protected mode */
  sol_pushinteger(L, argc);  /* 1st argument */
  sol_pushlightuserdata(L, argv); /* 2nd argument */
  status = sol_pcall(L, 2, 1, 0);  /* do the call */
  result = sol_toboolean(L, -1);  /* get result */
  report(L, status);
  sol_close(L);
  return (result && status == SOL_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}

