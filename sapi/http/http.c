/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Rasmus Lerdorf <rasmus@lerdorf.on.ca>                       |
   |          Stig Bakken <ssb@php.net>                                   |
   |          Zeev Suraski <zeev@php.net>                                 |
   | SAPI: Michał Brzuchalski <brzuchal@php.net>                          |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "php_globals.h"
#include "php_variables.h"
#include "zend_modules.h"
#include "php.h"
#include "zend_ini_scanner.h"
#include "zend_globals.h"
#include "zend_stream.h"

#include "SAPI.h"

#include <stdio.h>
#include "php.h"

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <signal.h>

#include <locale.h>

#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#if HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include "zend.h"
#include "zend_extensions.h"
#include "php_ini.h"
#include "php_globals.h"
#include "php_main.h"
#include "fopen_wrappers.h"
#include "ext/standard/php_standard.h"

#ifdef __riscos__
# include <unixlib/local.h>
int __riscosify_control = __RISCOSIFY_STRICT_UNIX_SPECS;
#endif

#include "zend_compile.h"
#include "zend_execute.h"
#include "zend_highlight.h"

#include "php_getopt.h"

#include "http_status_codes.h"

// #include "fastcgi.h"

#include <php_config.h>
#include <microhttpd.h>
#include "http.h"

// #include "main.c"
// PHPAPI int php_execute_simple_script(zend_file_handle *primary_file, zval *ret)

static const opt_struct OPTIONS[] = {
	{'p', 1, "port"},
    {'f', 1, "file"},
	{'c', 1, "php-ini"},
	{'d', 1, "define"},
	{'h', 0, "help"},
	{'i', 0, "info"},
	{'m', 0, "modules"},
	{'n', 0, "no-php-ini"},
	{'?', 0, "usage"},/* help alias (both '?' and 'usage') */
	{'v', 0, "version"},
	{'g', 1, "pid"},
	{'R', 0, "allow-to-run-as-root"},
    {'t', 0, "test"},
	{'D', 0, "daemonize"},
	{'F', 0, "nodaemonize"},
	{'O', 0, "force-stderr"},
	{'-', 0, NULL} /* end of args */
};

/* {{{ php_cgi_usage
 */
static void php_http_usage(char *argv0)
{
	char *prog;

	prog = strrchr(argv0, '/');
	if (prog) {
		prog++;
	} else {
		prog = "php";
	}

	php_printf(	"Usage: %s [-n] [-h] [-i] [-m] [-v] [-t] [-p <prefix>] [-g <pid>] [-c <file>] [-d foo[=bar]] [-y <file>] [-D] [-F [-O]]\n"
				"  -c <path>|<file> Look for php.ini file in this directory\n"
				"  -n               No php.ini file will be used\n"
				"  -d foo[=bar]     Define INI entry foo with value 'bar'\n"
				"  -h               This help\n"
				"  -i               PHP information\n"
				"  -m               Show compiled in modules\n"
				"  -v               Version number\n"
				"  -g, --pid <file>\n"
				"                   Specify the PID file location.\n"
				"  -t, --test       Test FPM configuration and exit\n"
				"  -D, --daemonize  force to run in background, and ignore daemonize option from config file\n"
				"  -F, --nodaemonize\n"
				"                   force to stay in foreground, and ignore daemonize option from config file\n"
                                "  -O, --force-stderr\n"
                                "                   force output to stderr in nodaemonize even if stderr is not a TTY\n"
				"  -R, --allow-to-run-as-root\n"
				"                   Allow pool to run as root (disabled by default)\n",
				prog, PHP_PREFIX);
}
/* }}} */

static char *php_self = "";
static char *script_filename = "";

static void sapi_http_register_variables(zval *track_vars_array) /* {{{ */
{
	size_t len;
	char   *docroot = "";

	/* In CGI mode, we consider the environment to be a part of the server
	 * variables
	 */
	php_import_environment_variables(track_vars_array);

	/* Build the special-case PHP_SELF variable for the CLI version */
	len = strlen(php_self);
	if (sapi_module.input_filter(PARSE_SERVER, "PHP_SELF", &php_self, len, &len)) {
		php_register_variable("PHP_SELF", php_self, track_vars_array);
	}
	if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_NAME", &php_self, len, &len)) {
		php_register_variable("SCRIPT_NAME", php_self, track_vars_array);
	}
	/* filenames are empty for stdin */
	len = strlen(script_filename);
	if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_FILENAME", &script_filename, len, &len)) {
		php_register_variable("SCRIPT_FILENAME", script_filename, track_vars_array);
	}
	if (sapi_module.input_filter(PARSE_SERVER, "PATH_TRANSLATED", &script_filename, len, &len)) {
		php_register_variable("PATH_TRANSLATED", script_filename, track_vars_array);
	}
	/* just make it available */
	len = 0U;
	if (sapi_module.input_filter(PARSE_SERVER, "DOCUMENT_ROOT", &docroot, len, &len)) {
		php_register_variable("DOCUMENT_ROOT", docroot, track_vars_array);
	}
}
/* }}} */

static void sapi_http_log_message(char *message, int syslog_type_int) /* {{{ */
{
	fprintf(stderr, "%s\n", message);
#ifdef PHP_WIN32
	fflush(stderr);
#endif
}
/* }}} */

static int sapi_cli_deactivate(void) /* {{{ */
{
	fflush(stdout);
	if(SG(request_info).argv0) {
		free(SG(request_info).argv0);
		SG(request_info).argv0 = NULL;
	}
	return SUCCESS;
}
/* }}} */

static char* sapi_cli_read_cookies(void) /* {{{ */
{
	return NULL;
}
/* }}} */

static int sapi_cli_header_handler(sapi_header_struct *h, sapi_header_op_enum op, sapi_headers_struct *s) /* {{{ */
{
	return 0;
}
/* }}} */

static int sapi_http_send_headers(sapi_headers_struct *sapi_headers) /* {{{ */
{
	/* We do nothing here, this function is needed to prevent that the fallback
	 * header handling is called. */
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}
/* }}} */

static int php_http_startup(sapi_module_struct *sapi_module)
{
	// if (php_module_startup(&http_sapi_module, &http_module_entry, 1) == FAILURE) {
	// 	return FAILURE;
	// } else {
	// 	return SUCCESS;
	// }
    if (php_module_startup(sapi_module, NULL, 0)==FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static sapi_module_struct http_sapi_module = {
	"http",
	"HTTP/php",
	
	php_http_startup,
	php_module_shutdown_wrapper,
	
	NULL,									/* activate */
	NULL,									/* deactivate */

	NULL,                                   /* unbuffered write */
	NULL,
	NULL,									/* get uid */
	NULL,									/* getenv */

	php_error,
	
	NULL,
	sapi_http_send_headers,
	NULL,
	NULL, /*sapi_uwsgi_read_post,*/
	NULL, /*sapi_uwsgi_read_cookies,*/

	sapi_http_register_variables,
	sapi_http_log_message,					/* Log message */
	NULL,									/* Get request time */
	NULL,									/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};


#define PAGE "<html><head><title>php-http</title>"\
             "</head><body><p>PHP File: %s</p></body></html>\n"

const char * get_connection_header(struct MHD_Connection * connection, const char * key)
{
    const char * header;
    
    header = MHD_lookup_connection_value(
        connection,
        MHD_HEADER_KIND,
        key
    );

    return header;
}

static int ahc_echo(void * php_self, struct MHD_Connection * connection, const char * url, const char * method, const char * version, const char * upload_data, size_t * upload_data_size, void ** ptr) {
  static int dummy;
  char * page;
  char * script_file = (char*) php_self;
  struct MHD_Response * response;
  int ret;

    sprintf(page, PAGE, script_file);
  if (0 != strcmp(method, "GET"))
    return MHD_NO; /* unexpected method */
  if (&dummy != *ptr) {
    /* The first time only the headers are valid,
        do not respond in the first round... */
    printf("Url: %s\nMethod: %s\nVersion: %s\n", url, method, version);
    printf("Header: %s=%s\n", "Host", get_connection_header(connection, "Host"));
    printf("Header: %s=%s\n", "Accept", get_connection_header(connection, "Accept"));
    printf("Header: %s=%s\n", "User-Agent", get_connection_header(connection, "User-Agent"));
    *ptr = &dummy;
    return MHD_YES;
  }
  if (0 != *upload_data_size)
    return MHD_NO; /* upload data in a GET!? */
  printf("Url: %s\nMethod: %s\nVersion: %s\n", url, method, version);
  *ptr = NULL; /* clear context pointer */
  response = MHD_create_response_from_buffer(strlen(page), (void*) page, MHD_RESPMEM_PERSISTENT);
  ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return ret;
}

static int module_name_cmp(Bucket *f, Bucket *s) /* {{{ */
{
	return strcasecmp(((zend_module_entry *)Z_PTR(f->val))->name,
				  ((zend_module_entry *)Z_PTR(s->val))->name);
}
/* }}} */

static void print_modules(void) /* {{{ */
{
	HashTable sorted_registry;
	zend_module_entry *module;

	zend_hash_init(&sorted_registry, 50, NULL, NULL, 0);
	zend_hash_copy(&sorted_registry, &module_registry, NULL);
	zend_hash_sort(&sorted_registry, module_name_cmp, 0);
	ZEND_HASH_FOREACH_PTR(&sorted_registry, module) {
		php_printf("%s\n", module->name);
	} ZEND_HASH_FOREACH_END();
	zend_hash_destroy(&sorted_registry);
}
/* }}} */

static int print_extension_info(zend_extension *ext, void *arg) /* {{{ */
{
	php_printf("%s\n", ext->name);
	return ZEND_HASH_APPLY_KEEP;
}
/* }}} */

static int extension_name_cmp(const zend_llist_element **f, const zend_llist_element **s) /* {{{ */
{
	zend_extension *fe = (zend_extension*)(*f)->data;
	zend_extension *se = (zend_extension*)(*s)->data;
	return strcmp(fe->name, se->name);
}
/* }}} */

static void print_extensions(void) /* {{{ */
{
	zend_llist sorted_exts;

	zend_llist_copy(&sorted_exts, &zend_extensions);
	sorted_exts.dtor = NULL;
	zend_llist_sort(&sorted_exts, extension_name_cmp);
	zend_llist_apply(&sorted_exts, (llist_apply_func_t) print_extension_info);
	zend_llist_destroy(&sorted_exts);
}
/* }}} */

/* {{{ cli_seek_file_begin
 */
static int http_seek_file_begin(zend_file_handle *file_handle, char *script_file)
{
printf("Trying to open: %s\n", script_file);
	FILE *fp = VCWD_FOPEN(script_file, "rb");
printf("Opened file ??: %d\n", fp);
	if (!fp) {
		php_printf("Could not open input file: %s\n", script_file);
		return FAILURE;
	}

	zend_stream_init_fp(file_handle, fp, script_file);
	return SUCCESS;
}
/* }}} */

int main(int argc, char ** argv) {
    struct MHD_Daemon *d;
	int c, port = -1;
	zend_file_handle file_handle;
	int behavior = PHP_MODE_STANDARD;
	volatile int request_started = 0;
	volatile int exit_status = 0;
	char *php_optarg = NULL, *orig_optarg = NULL;
	int php_optind = 1, orig_optind = 1;
	// char *arg_free=NULL, **arg_excp=&arg_free;
	char *script_file=NULL, *translated_path = NULL;
	// int interactive=0;
	const char *param_error=NULL;
	int hide_argv = 0;
    int show_error = 1;

	zend_try {

		CG(in_compilation) = 0; /* not initialized but needed for several options */
printf("Pre while argc+argv...\n");
		while ((c = php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, show_error, 2)) != -1) {
printf("Entering switch\n");
printf("Switch c: %c\n", c);
			switch (c) {

			case 'h': /* help & quit */
			case '?':
				php_http_usage(argv[0]);
				goto out;

			case 'i': /* php info & quit */
				if (php_request_startup()==FAILURE) {
					goto err;
				}
				request_started = 1;
				php_print_info(0xFFFFFFFF);
				php_output_end_all();
				exit_status = (c == '?' && argc > 1 && !strchr(argv[1],  c));
				goto out;

			case 'v': /* show php version & quit */
				php_printf("PHP %s (%s) (built: %s %s) ( %s)\nCopyright (c) The PHP Group\n%s",
					PHP_VERSION, http_sapi_module.name, __DATE__, __TIME__,
#if ZTS
					"ZTS "
#else
					"NTS "
#endif
#ifdef COMPILER
					COMPILER
					" "
#endif
#ifdef ARCHITECTURE
					ARCHITECTURE
					" "
#endif
#if ZEND_DEBUG
					"DEBUG "
#endif
#ifdef HAVE_GCOV
					"GCOV "
#endif
					,
					get_zend_version()
				);
				sapi_deactivate();
				goto out;

			case 'm': /* list compiled in modules */
				if (php_request_startup()==FAILURE) {
					goto err;
				}
				request_started = 1;
				php_printf("[PHP Modules]\n");
				print_modules();
				php_printf("\n[Zend Modules]\n");
				print_extensions();
				php_printf("\n");
				php_output_end_all();
				exit_status=0;
				goto out;

			default:
				break;
			}
		}

		/* Set some CLI defaults */
		SG(options) |= SAPI_OPTION_NO_CHDIR;
printf("Pre second while argc+argv...\n");
		php_optind = orig_optind;
		php_optarg = orig_optarg;

		while ((c = php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, show_error, 2)) != -1) {
printf("Entering switch\n");
printf("Switch c: %c\n", c);
			switch (c) {

			case 'f': /* parse file */
                if (script_file) {
					param_error = "You can use -f only once.\n";
					break;
				}
                printf("Arg -f: %s\n", php_optarg);
				script_file = php_optarg;
                printf("Arg -f set...\n");
				break;

			case 'z': /* load extension file */
				zend_load_extension(php_optarg);
				break;
			case 'H':
				hide_argv = 1;
				break;
            case 'p':
                if (port > 0) {
					param_error = "You can use -p only once.\n";
					break;
				}
                port = atoi(php_optarg);
                break;
			case 15:
				behavior = PHP_MODE_SHOW_INI_CONFIG;
				break;
			default:
				break;
			}
		}
printf("Before param_error...\n");
		if (param_error) {
			PUTS(param_error);
			exit_status=1;
			goto err;
		}
printf("After param_error...\n");
printf("argc > php_optind: ");printf("%d\n", argc > php_optind);
printf("!script_file: ");printf("%d\n", !script_file);
printf("behavior!=PHP_MODE_PROCESS_STDIN: ");printf("%d\n", behavior!=PHP_MODE_PROCESS_STDIN);
printf("strcmp(argv[php_optind-1],\'--\')): ");printf("%d\n", strcmp(argv[php_optind-1],"--"));
		/* only set script_file if not set already and not in direct mode and not at end of parameter list */
		if (argc > php_optind
		  && !script_file
		  && behavior!=PHP_MODE_PROCESS_STDIN
		  && strcmp(argv[php_optind-1],"--"))
		{
			script_file=argv[php_optind];
			php_optind++;
		}
printf("script_file: ");printf("%d\n", script_file);
		if (script_file) {
printf("Line 523...\n");
			if (http_seek_file_begin(&file_handle, script_file) != SUCCESS) {
printf("Line 525...\n");
				goto err;
			} else {
printf("Line 528...\n");
				char real_path[MAXPATHLEN];
printf("Line 530...\n");
				if (VCWD_REALPATH(script_file, real_path)) {
printf("Line 532...\n");
					translated_path = strdup(real_path);
				}
printf("Line 535...\n");
				script_filename = script_file;
			}
		} else {
			/* We could handle PHP_MODE_PROCESS_STDIN in a different manner  */
			/* here but this would make things only more complicated. And it */
			/* is consistent with the way -R works where the stdin file handle*/
			/* is also accessible. */
printf("Line 543...\n");
			zend_stream_init_fp(&file_handle, stdin, "Standard input code");
		}
printf("Line 546...\n");
		php_self = (char*)file_handle.filename;

    } zend_end_try();

    printf("PHP File: %s\n", php_self);

    printf("Trying to run http daemon on port=%d\n", port);
    d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, port, NULL, NULL, &ahc_echo, php_self, MHD_OPTION_END);
    if (d == NULL)
        return 1;
    
    (void) getc (stdin);
    printf("Terminating http daemon...\n");
    MHD_stop_daemon(d);
out:
	if (request_started) {
		php_request_shutdown((void *) 0);
	}
	if (translated_path) {
		free(translated_path);
	}
	if (exit_status == 0) {
		exit_status = EG(exit_status);
	}
	return exit_status;
err:
	sapi_deactivate();
	zend_ini_deactivate();
	exit_status = 1;
	goto out;
}
