#ifndef CCAN_LINT_H
#define CCAN_LINT_H
#include "config.h"
#include <ccan/list/list.h>
#include <ccan/dgraph/dgraph.h>
#include <ccan/autodata/autodata.h>
#include <stdbool.h>
#include "../doc_extract.h"
#include "licenses.h"

AUTODATA_TYPE(ccanlint_tests, struct ccanlint);
#define REGISTER_TEST(test) AUTODATA(ccanlint_tests, &test)

/* 0 == Describe failed tests.
   1 == Describe results for partial failures.
   2 == One line per test, plus details of failures.

   Mainly for debugging ccanlint:
   3 == Describe every object built.
   4 == Describe every action. */
extern int verbose;

enum compile_type {
	COMPILE_NORMAL,
	COMPILE_NOFEAT,
	COMPILE_COVERAGE,
	COMPILE_TYPES
};

struct manifest {
	char *dir;
	/* The module name, ie. final element of dir name */
	char *basename;
	struct ccan_file *info_file;

	/* Linked off deps. */
	struct list_node list;
	/* Where our final compiled output is */
	char *compiled[COMPILE_TYPES];

	struct list_head c_files;
	struct list_head h_files;

	struct list_head run_tests;
	struct list_head api_tests;
	struct list_head compile_ok_tests;
	struct list_head compile_fail_tests;
	struct list_head other_test_c_files;
	struct list_head other_test_files;

	struct list_head other_files;
	struct list_head examples;
	struct list_head mangled_examples;

	/* From tests/check_depends_exist.c */
	struct list_head deps;

	/* From tests/license_exists.c */
	enum license license;
};

/* Get the manifest for a given directory. */
struct manifest *get_manifest(const void *ctx, const char *dir);

/* Error in a particular file: stored off score->per_file_errors. */
struct file_error {
	struct list_node list;
	struct ccan_file *file;
	unsigned int line;
};

/* The score for an individual test. */
struct score {
	/* Starts as false: if not set to true, ccanlint exits non-zero.
	 * Thus it is usually set for compilation or other serious failures. */
	bool pass;
	/* Starts at 0 and 1 respectively. */
	unsigned int score, total;
	/* The error message to print. */
	char *error;
	/* Per file errors, set by score_file_error() */
	struct list_head per_file_errors;
};

struct ccanlint {
	/* More concise unique name of test. */
	const char *key;

	/* Unique name of test */
	const char *name;

	/* Can we run this test?  Return string explaining why, if not. */
	const char *(*can_run)(struct manifest *m);

	/* Should we stop immediately if test fails? */
	bool compulsory;

	/* If timeleft is set to 0, means it timed out.
	 * score is the result, and a talloc context freed after all our
	 * depends are done. */
	void (*check)(struct manifest *m,
		      unsigned int *timeleft, struct score *score);

	/* Can we do something about it? (NULL if not) */
	void (*handle)(struct manifest *m, struct score *score);

	/* Options from _info. */
	char **options;
	/* If not set, we'll give an error if they try to set options. */
	bool takes_options;

	/* Space-separated list of dependency keys. */
	const char *needs;

	/* Internal use fields: */
	/* We are a node in a dependency graph. */
	struct dgraph_node node;
	/* Did we skip a dependency?  If so, must skip this, too. */
	const char *skip;
	/* Have we already run this? */
	bool done;
};

/* Ask the user a yes/no question: the answer is NO if there's an error. */
bool ask(const char *question);

enum line_info_type {
	PREPROC_LINE, /* Line starts with # */
	CODE_LINE, /* Code (ie. not pure comment). */
	DOC_LINE, /* Line with kernel-doc-style comment. */
	COMMENT_LINE, /* (pure) comment line */
};

/* So far, only do simple #ifdef/#ifndef/#if defined/#if !defined tests,
 * and #if <SYMBOL>/#if !<SYMBOL> */
struct pp_conditions {
	/* We're inside another ifdef? */
	struct pp_conditions *parent;

	enum {
		PP_COND_IF,
		PP_COND_IFDEF,
		PP_COND_UNKNOWN,
	} type;

	bool inverse;
	const char *symbol;
};

/* Preprocessor information about each line. */
struct line_info {
	enum line_info_type type;

	/* Is this actually a continuation of line above? (which ends in \) */
	bool continued;

	/* Conditions for this line to be compiled. */
	struct pp_conditions *cond;
};

struct ccan_file {
	struct list_node list;

	/* Name (usually, within m->dir). */
	char *name;

	/* Full path name. */
	char *fullname;

	/* Pristine version of the original file.
	 * Use get_ccan_file_contents to fill this. */
	const char *contents;
	size_t contents_size;

	/* Use get_ccan_file_lines / get_ccan_line_info to fill these. */
	unsigned int num_lines;
	char **lines;
	struct line_info *line_info;

	struct list_head *doc_sections;

	/* If this file gets compiled (eg. .C file to .o file), result here. */
	char *compiled[COMPILE_TYPES];

	/* Filename containing output from valgrind. */
	char *valgrind_log;

	/* Leak output from valgrind. */
	char *leak_info;

	/* Simplified stream (lowercase letters and single spaces) */
	char *simplified;
};

/* A new ccan_file, with the given name (talloc_steal onto returned value). */
struct ccan_file *new_ccan_file(const void *ctx, const char *dir, char *name);

/* Use this rather than accessing f->contents directly: loads on demand. */
const char *get_ccan_file_contents(struct ccan_file *f);

/* Use this rather than accessing f->lines directly: loads on demand. */
char **get_ccan_file_lines(struct ccan_file *f);

/* Use this rather than accessing f->lines directly: loads on demand. */
struct line_info *get_ccan_line_info(struct ccan_file *f);

/* Use this rather than accessing f->simplified directly: loads on demand. */
const char *get_ccan_simplified(struct ccan_file *f);

enum line_compiled {
	NOT_COMPILED,
	COMPILED,
	MAYBE_COMPILED,
};

/* Simple evaluator.  If symbols are set this way, is this condition true?
 * NULL values mean undefined, NULL symbol terminates. */
enum line_compiled get_ccan_line_pp(struct pp_conditions *cond,
				    const char *symbol,
				    const unsigned int *value, ...);

/* Get token if it's equal to token. */
bool get_token(const char **line, const char *token);
/* Talloc copy of symbol token, or NULL.  Increment line. */
char *get_symbol_token(void *ctx, const char **line);

/* Similarly for ->doc_sections */
struct list_head *get_ccan_file_docs(struct ccan_file *f);

/* Get NULL-terminated array options for this file for this test */
char **per_file_options(const struct ccanlint *test, struct ccan_file *f);

/* Append message about this file (and line, if non-zero) to the score->error */
void score_file_error(struct score *, struct ccan_file *f, unsigned line,
		      const char *errorfmt, ...);

/* Start a command in the background. */
void run_command_async(const void *ctx, unsigned int time_ms,
		       const char *fmt, ...);

/* Async version of compile_and_link. */
void compile_and_link_async(const void *ctx, unsigned int time_ms,
			    const char *cfile, const char *ccandir,
			    const char *objs, const char *compiler,
			    const char *cflags,
			    const char *libs, const char *outfile);

/* Get results of a command, returning ctx (and free it). */
void *collect_command(bool *ok, char **output);

/* Normal tests. */
extern struct ccanlint trailing_whitespace;

/* Dependencies */
struct dependent {
	struct list_node node;
	struct ccanlint *dependent;
};

/* Is this test excluded (cmdline or _info). */
bool is_excluded(const char *name);

/* Called to add options from _info, once it's located. */
void add_info_options(struct ccan_file *info);

/* Are we happy to compile stuff, or just non-intrusive tests? */
extern bool safe_mode;

/* Did the user want to keep all the results? */
extern bool keep_results;

/* Where is the ccan dir?  Available after first manifest. */
extern const char *ccan_dir;

/* Compiler and CFLAGS, from config.h if available. */
extern const char *compiler, *cflags;

/* Contents of config.h (or NULL if not found) */
extern const char *config_header;

#endif /* CCAN_LINT_H */
