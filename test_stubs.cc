// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT
//
// Minimal stub definitions for linking test binaries.
// Provides globals and no-op function stubs so that modules
// under test can link without pulling in the entire BBS.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>
#include <vector>

#include "struct.h"
#include "yapp.h"
#include "globals.h"
#include "files.h"
#include "driver.h"
#include "security.h"
#include "stats.h"
#include "conf.h"
#include "user.h"
#include "license.h"
#include "sep.h"

// --- Global variable definitions ---

flag_t flags;
unsigned char mode;
flag_t status;
flag_t debug;

stdin_t saved_stdin[STDIN_STACK_SIZE];
stdin_t orig_stdin[STDIN_STACK_SIZE];
int stdin_stack_top;

int current;
int confidx = -1;
int defidx;
int joinidx;

std::vector<std::string> cflist;
std::string cfliststr;
std::vector<std::string> fw;

std::string bbsdir;
std::string helpdir;
std::vector<assoc_t> conflist;
std::vector<assoc_t> desclist;
std::string hostname;

uid_t uid;
std::string login;
std::string fullname;
std::string email;
std::string home;
std::string work;
std::string partdir;

status_t st_glob, st_new;
response_t re[MAX_RESPONSES];
sumentry_t sum[MAX_ITEMS];
partentry_t part[MAX_ITEMS];

char evalbuf[MAX_LINE_LENGTH];

std::string pipebuf;
int exit_status;
std::string acl_list[NUM_RIGHTS];

// --- Stub function definitions ---

FILE *mopen(const std::string &, long) { return nullptr; }
int mclose(FILE *) { return 0; }
void mcheck(void) {}

void open_pipe(void) {}
int spclose(FILE *) { return 0; }
void wputchar(int) {}
void wputs(const std::string_view &) {}
void wfputs(const std::string_view &, FILE *) {}
void wfputc(int, FILE *) {}
void ints_on(void) {}
void ints_off(void) {}
void pop_stdin(void) {}
void push_stdin(FILE *, int) {}
void endbbs(int) {}
void init(int, char **) {}
char source(const std::string &, const std::string &, int, int) { return 0; }
char command(const std::string &, int) { return 0; }
void print_prompt(int) {}
char ok_cmd_dispatch(int, char **) { return 0; }
bool get_command(const std::string_view &, int) { return false; }
void handle_int(int) {}
void handle_pipe(int) {}
void handle_other(int, int, void *, char *) {}
void open_cluster(const std::string &, const std::string &) {}

// conf.h stubs
int check(int, char **) { return 0; }
int show_conf_index(int, char **) { return 0; }
char checkpoint(int, unsigned int, int) { return 0; }
int do_next(int, char **) { return 0; }
char join(const std::string &, int, int) { return 0; }
int leave(int, char **) { return 0; }
int participants(int, char **) { return 0; }
int describe(int, char **) { return 0; }
int resign(int, char **) { return 0; }
void ylog(int, const std::string_view &) {}
const char *get_desc(const std::string_view &) { return ""; }
const char *nextconf(void) { return ""; }
const char *nextnewconf(void) { return ""; }
const char *prevconf(void) { return ""; }
unsigned int security_type(const std::vector<std::string> &, int) { return 0; }
bool is_inlistfile(int, const std::string &) { return false; }
bool is_fairwitness(int) { return false; }
bool check_password(int) { return false; }
bool is_validorigin(int) { return false; }
std::string_view fullname_in_conference(status_t *) { return ""; }
std::vector<std::string> grab_recursive_list(const std::string &, const std::string &) { return {}; }

// user.h stubs
std::string get_ticket(std::time_t, const std::string_view &) { return ""; }
int authenticate(int, char **) { return 0; }
void refresh_list(void) {}
int do_cflist(int, char **) { return 0; }
int passwd(int, char **) { return 0; }
int chfn(int, char **) { return 0; }
int newuser(int, char **) { return 0; }
static std::string sysop_login_stub;
const std::string &get_sysop_login(void) { return sysop_login_stub; }
uid_t get_nobody_uid(void) { return 65534; }
int get_user(uid_t *, std::string &, std::string &, std::string &, std::string &) { return 0; }
void login_user(void) {}
int del_cflist(const std::string &) { return 0; }
void show_cflist(void) {}
const char *email2login(const std::string_view &) { return ""; }
int partfile_perm(void) { return 0; }
std::string get_partdir(const std::string_view &) { return ""; }
std::string get_userfile(const std::string_view &, int *) { return ""; }
bool sane_fullname(const std::string_view &) { return true; }

// security.h stubs
bool check_acl(int, int) { return false; }
void load_acl(int) {}
void reinit_acl(void) {}
bool is_auto_ulist(int) { return false; }

// stats.h stubs
const char *get_subj(int, int, sumentry_t *) { return ""; }
const char *get_auth(int, int, sumentry_t *) { return ""; }
static std::vector<std::string> empty_config;
const std::vector<std::string> &get_config(int) { return empty_config; }
void clear_cache(void) {}
void store_subj(int, int, const std::string_view &) {}
void store_auth(int, int, const std::string_view &) {}

// license.h stubs
void read_config(void) {}
std::string get_conf_param(const std::string_view &, const std::string_view &def) { return std::string(def); }
void free_config(void) {}
int get_hits_today(void) { return 0; }

// sep.h stubs
void confsep(const std::string_view &, int, status_t *, partentry_t *, int) {}
void itemsep(const std::string_view &, int) {}
void fitemsep(FILE *, const std::string_view &, int) {}
void sepinit(int) {}
void skip_new_response(int, int, int) {}
char *get_sep(const char **) { return nullptr; }
void init_show(void) {}
char itemcond(const char **, long) { return 0; }
char confcond(const char **, int, status_t *) { return 0; }

// system.h stubs
int unix_cmd(const std::string &) { return 0; }
FILE *spopen(const std::string &) { return nullptr; }
FILE *smopenw(const std::string &, long) { return nullptr; }
FILE *smopenr(const std::string &, long) { return nullptr; }
int smclose(FILE *) { return 0; }
int sdpopen(FILE **, FILE **, const std::string &) { return 0; }
int sdpclose(FILE *, FILE *) { return 0; }
int edit(const std::string &, const std::string &, bool) { return 0; }
int priv_edit(const std::string &, const std::string &, int) { return 0; }
int rm(const std::string &, int) { return 0; }
int copy_file(const std::string &, const std::string &, int) { return 0; }
int move_file(const std::string &, const std::string &, int) { return 0; }

// sum.h stubs: refresh_sum, dirty_part, dirty_sum, refresh_stats
// are provided by sum.o (part of TEST_COMMON)

// news.h stubs
#ifdef NEWS
std::string dot2slash(const std::string_view &str) { return std::string(str); }
int make_rnhead(response_t *, int) { return 0; }
static std::string empty_mid;
const std::string &message_id(const std::string_view &, int, int, response_t *) { return empty_mid; }
void get_article(response_t *) {}
void refresh_news(sumentry_t *, partentry_t *, status_t *, int) {}
int incorporate(long, sumentry_t *, partentry_t *, status_t *, int) { return 0; }
int make_emhead(response_t *, int) { return 0; }
int make_emtail(void) { return 0; }
#endif

#ifdef HAVE_DBM_OPEN
int save_dbm(const std::string &, const std::string_view &, const std::string_view &, int) { return 0; }
std::string get_dbm(const std::string &, const std::string_view &, int) { return ""; }
void dump_dbm(const std::string &) {}
#endif

void madd(int, const std::string_view &, int, int) {}
int get_pid(FILE *) { return 0; }
void mdump(void) {}
