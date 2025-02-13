// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "user.h"

#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/wait.h>

#include <ctype.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <format>
#include <print>
#include <string>
#include <vector>

#include "conf.h"
#include "config.h"
#include "driver.h"
#include "files.h"
#include "globals.h"
#include "lib.h"
#include "license.h"
#include "macro.h"
#include "main.h"
#include "sep.h"
#include "str.h"
#include "system.h"
#include "yapp.h"

static time_t listtime;
static int listsize;

uid_t
get_nobody_uid(void)
{
    static uid_t nobody_uid = 0;

    /* Resolve uid for "nobody" if needed */
    if (nobody_uid == 0) {
        const auto nobody = get_conf_param("nobody", NOBODY);
        const struct passwd *nuid = getpwnam(nobody.c_str());
        if (nuid != NULL)
            nobody_uid = nuid->pw_uid;
    }

    return nobody_uid;
}

const std::string &
get_sysop_login(void)
{
    static std::string cfadm;
    if (cfadm.empty()) {
        if (getuid() == get_nobody_uid())
            cfadm = get_conf_param("sysop", cfadm);
        if (cfadm.empty()) {
            struct passwd *pwd = getpwuid(geteuid());
            if (pwd != nullptr)
                cfadm = pwd->pw_name;
        }
        if (cfadm.empty())
            cfadm = "cfadm";
    }
    return cfadm;
}

#ifdef WWW
/* IN: login */
/* OUT: suid mode */
std::string
get_userfile(const std::string_view &who, int *suid)
{
    int nullsuid;
    if (suid == nullptr)
        suid = &nullsuid;
#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true")) {
        const auto filename = std::format("{}/{:c}/{:c}/{}/{}",
            get_conf_param("userhome", USER_HOME), tolower(who[0]),
            tolower(who[1]), who, who);
        *suid = SL_USER;
        return filename;
    }
#endif
    const auto file = get_conf_param("userfile", USER_FILE);
    if (file[0] == '~' && file[1] == '/') {
        const auto filename = std::format("{}/{:c}/{:c}/{}/{}",
            get_conf_param("userhome", USER_HOME), tolower(who[0]),
            tolower(who[1]), who, file.c_str() + 2);
        *suid = SL_USER;
        return filename;
    }
    *suid = SL_OWNER;
    return file;
}
/*
 * A "local" user is one whose identity exists only inside Yapp,
 * rather than having their own Unix account.
 */
static int
get_local_user(const std::string_view &login, /* IN: login to find */
    std::string &fullname,                    /* OUT: full name (or NULL) */
    std::string &home,                        /* OUT: home directory to use (or NULL) */
    std::string &email                        /* OUT: email address (or NULL) */
)
{
    FILE *fp;
    std::string buff;
    int suid;

    /* Retrieve information from user file (could be in user dir) */
    if (debug & DB_USER)
        std::println("called get_local_user(\"{}\")", login);

    const auto filename = get_userfile(login, &suid);

#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true")) {
        struct stat st;
        const auto path = str::concat({filename, ".db"});
        if (stat(path.c_str(), &st))
            return 0;
        auto homepath =
            std::format("{}/{}", get_conf_param("userhome", USER_HOME), login);
        if (access(homepath.c_str(), X_OK))
            homepath = std::format("{}/{:c}/{:c}/{}",
                get_conf_param("userhome", USER_HOME), tolower(login[0]),
                tolower(login[1]), login);
        home = homepath;
        fullname = get_dbm(filename, "fullname", SL_USER);
        email = get_dbm(filename, "email", SL_USER);
        return 1;
    }
#endif

    if (suid == SL_USER) {
        /* Here, it's okay for it not to exist, since that just means
         * it's not a web user */
        if ((fp = smopenr(filename, O_R | O_SILENT)) == NULL)
            return 0;
    } else {
        if ((fp = mopen(filename, O_R)) == NULL)
            return 0;
    }
    while (ngets(buff, fp)) {
        const auto field = str::splits(buff, ":", false);
        if (field.empty())
            continue;
        if (str::eq(field[0], login)) {
            fullname = field[1];
            email = field[2];
            auto homepath = std::format(
                "{}/{}", get_conf_param("userhome", USER_HOME), login);
            if (access(homepath.c_str(), X_OK))
                homepath = std::format("{}/{:c}/{:c}/{}",
                    get_conf_param("userhome", USER_HOME), tolower(login[0]),
                    tolower(login[1]), login);
            home = homepath;
            if (suid == SL_USER)
                smclose(fp);
            else
                mclose(fp);
            if (debug & DB_USER)
                std::println(
                    "get_local_user found (\"{}\",\"{}\")", login, fullname);
            return 1;
        }
    }
    if (suid == SL_USER)
        smclose(fp);
    else
        mclose(fp);

    return 0;
}
#endif

/* getuid=nobody, uid=nobody, login there: search WWW by login (%e in rsep),
         also when nobody authenticates
   getuid=nobody, uid != 0, login there: search Unix by uid  (%e in rsep)
                  uid=0, login there: participants with login in ulist
                  uid!=0, login=0:    participants with uid in ulist
                  uid=0,  login=0:    init() to get current user
 */

/* Here's the cases in the order they're covered:
      getuid=nobody,  uid=0, login=0          -> get current WWW user info
      getuid=nobody,  uid=nobody, login=0     -> get current WWW user info
      getuid!=nobody, uid=0, login=0          -> get current Unix user info
                      uid=0, login there      -> search Unix, then WWW by login
                      uid != 0, login=0       -> search Unix by uid
                      uid=nobody, login there -> search WWW by login
                      uid other, login there  -> grab extra info for Unix user
 */

/* Get the real user information */
/* IN: user ID or 0 for lookup by login */
/* IN: if uid is 0, login is name to find */
int
get_user(uid_t *uid, std::string &login, std::string &fullname,
    std::string &home, std::string &email)
{
    char *tmp;
    struct passwd *pwd = NULL;

    if (debug & DB_USER)
        std::println("called get_user({},\"{}\")", uid ? *uid : 0, login);

    /* Case 1: Get current WWW user getuid=nobody,  uid=0, login=0 -> get
     * current WWW user info getuid=nobody,  uid=nobody, login=0 -> get
     * current WWW user info */
    if (getuid() == get_nobody_uid() && (!*uid || *uid == get_nobody_uid()) &&
        login.empty() && (tmp = getenv("REMOTE_USER")) && tmp[0]) {
        *uid = get_nobody_uid();
        login = tmp;
        return get_local_user(login, fullname, home, email);
    }

    /* If blank spec, get Unix user getuid!=nobody, uid=0, login=0 -> get
     * current Unix user info */
    if ((*uid) == 0 && login.empty()) {
        *uid = getuid();

        tmp = getlogin();
#ifdef HAVE_CUSERID
        if (!tmp)
            tmp = cuserid(NULL);
#endif

        /* For some really odd reason, I have observed Solaris
         * reporting the login (from both getlogin() and cuserid()) as
         * "LOGIN". Let's just pretend we got a NULL returned instead.
         */
        if (tmp && str::eq(tmp, "LOGIN"))
            tmp = NULL;

        if (tmp) {
            if (!(pwd = getpwnam(tmp))) {
                if (debug & DB_USER)
                    std::println(
                        "getpwnam failed to get anything for login {}", tmp);
                return 0;
            }

            /* Verify that UID = Login's UID */
            if (pwd->pw_uid == *uid)
                login = tmp;
        }
    } else if ((*uid) == 0) { /* search by login */
        /* uid=0, login there -> search Unix, then WWW by login */
        if ((pwd = getpwnam(login.c_str())) == nullptr) {
            /* search Unix first then WWW */
            return get_local_user(login, fullname, home, email);
        }
        *uid = pwd->pw_uid;
    }

    /* Ok, now *uid is valid */
    if (login.empty()) {
        /* uid != 0, login=0       -> search Unix by uid */
        if (!(pwd = getpwuid(*uid)))
            return 0;
        login = pwd->pw_name;
    }
    if (!pwd)
        pwd = getpwuid(*uid);
    if (debug & DB_USER)
        std::println("get_user saw ({},\"{}\",\"{}\")", *uid, login, fullname);

    /* uid=nobody, login there -> search WWW by login */
    if (*uid == get_nobody_uid() &&
        !str::eq(login, get_conf_param("nobody", NOBODY))) {
        return get_local_user(login, fullname, home, email);
    }

    /* uid other, login there  -> grab extra info for Unix user */
    if (!pwd)
        return 0;
    home = pwd->pw_dir;

    fullname = "Unknown";
    const auto namestrs =
        str::splits(pwd->pw_gecos, expand("gecos", DM_VAR), false);
    if (!namestrs.empty())
        fullname = namestrs[0];

    if (debug & DB_USER)
        std::println(
            "get_user found ({},\"{}\",\"{}\")", *uid, login, fullname);

    email = std::format("{}@{}", login, hostname);

    return 1;
}

/*
 * The slow way: search through userfile.   This only works if
 * userdbm is false.
 */
const char *
email2login(const std::string_view &email)
{
    int suid;
    FILE *fp;
    std::string buff;
    static std::string login;

#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true")) {
        /* If it's true, then we don't have a good way to verify
         * registered users at the moment.  In the future, we might
         * want to have a dbm file indexed by email address. XXX */
        return NULL;
    }
#endif

    /* Make sure there's a single user file */
    login.clear();
    const auto userfile = get_userfile(login, &suid);
    if (suid == SL_USER)
        return NULL; /* No good way to do this either */
    if ((fp = mopen(userfile, O_R)) == NULL)
        return NULL;

    const auto partsA = str::split(email, "@");
    if (partsA.size() < 2) {
        mclose(fp);
        return NULL;
    }

    /* Search for email address */
    while (ngets(buff, fp)) {
        const auto field = str::split(buff, ":", false);
        if (field.size() < 3)
            continue;
        const auto partsB = str::split(field[2], "@");
        if (partsB.size() < 2)
            continue;
        if (partsA[0] == partsB[0] && partsA[1].ends_with(partsB[1])) {
            login = field[0];
            mclose(fp);
            return login.c_str();
        }
    }
    mclose(fp);

    return nullptr;
}

#ifdef WWW
/*  This section allows secure WWW POST'ing in Yapp.  From a secure GET
 *  script, we generate a form with a HIDDEN ticket, which is
 *  {time, login, ( password, time )_DES }
 *
 *  When the post comes in, we will recrypt the ticket and compare for
 *  validity.
 *
 *  ticket with 2 arguments outputs the ticket generated
 */

#define TIMEDELTA 60 * 30 /* 30 min lifetime */

static unsigned char itoa64[] = /* 0 ... 63 => ascii - 64 */
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
to64(char *s, long v, int n)
{
    while (--n >= 0) {
        *s++ = itoa64[v & 0x3f];
        v >>= 6;
    }
}

#ifdef INCLUDE_EXTRA_COMMANDS
/* Given a login, return the user's encrypted password */
std::string
get_passwd(const std::string_view &who)
{
    if (who.empty())
        return "";

#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true") &&
        str::eq(get_conf_param("userfile", USER_FILE),
            get_conf_param("passfile", PASS_FILE))) {
        int suid;
        const auto userfile = get_userfile(login, &suid);
        return get_dbm(userfile, "passwd", SL_USER);
    }
#endif /* HAVE_DBM_OPEN */

    /* Check local user file */
    FILE *fp = mopen(get_conf_param("passfile", PASS_FILE), O_R);
    if (fp == NULL) {
        error("opening", " user file");
        exit(1);
    }
    std::string line;
    while (ngets(line, fp)) {
        if (line.empty() || line[0] == '#')
            continue;
        auto w = line.find(':');
        if (w != std::string::npos) {
            const auto wline = std::string_view(line).substr(0, w);
            line.erase(w);
            if (who == wline) {
                mclose(fp);
                line.erase(0, w + 1);
                return line;
            }
        }
    }
    mclose(fp);

    return "";
}
#endif

/*
 * Generate a ticket that proves that the key (i.e. encrypted password)
 * was known at time tm.  This assumes that the PASS_FILE file is only
 * readable by cfadm.  Used by %{ticket} macro, and license file.
 */
static std::string
make_ticket(const std::string &key, time_t tm)
{
#ifdef int_CRYPT
    char buff[MAX_LINE_LENGTH];
    int len;
#endif
    char salt[3];
    salt[2] = '\0';
    srand(tm);
    to64(&salt[0], rand(), 2);
    salt[2] = '\0';
#ifdef int_CRYPT
    len = key.size();
    const char *p = key.data();
    std::string out(crypt(p, salt));
    strcpy(buff, crypt(p, salt));
    p += 8;
    while (p - key.data() < len) {
        strcat(buff, crypt(p, salt) + 2);
        p += 8;
    }
    return std::string(buff);
#else
    return std::string(crypt(key, salt));
#endif
}

#ifdef INCLUDE_EXTRA_COMMANDS
/* The %{ticket} macro should expand to:
 *   ticket(0, who);
 */
/* tm: 0 = now, or timestamp */
/* who: login */
std::string
get_ticket(time_t tm, const std::string_view &who)
{
    time_t rtm;

    /* Validate timestamp */
    time(&rtm);
    if (!tm)
        tm = rtm;

    if (tm > rtm || tm < rtm - TIMEDELTA) {
        return "";
    }

    /* Validate login */
    const auto password = get_passwd(who);
    if (password.empty()) {
        return "";
    }

    /* Output ticket */
    return str::join(
        ":", {std::to_string(tm), std::string(who), make_ticket(password, tm)});
}

static int
do_authenticate(const std::string &ticket)
{
    /* Only "nobody" can authenticate */
    if ((status & S_NOAUTH) == 0) {
        std::println("You are already authenticated.");
        return 0;
    }
    const auto field = str::split(ticket, ":", false);
    if (field.empty())
        return 0;

    time_t tm = str::toi(field[0]);
    auto ticket1 = get_ticket(tm, field[1]);
    if (!ticket.empty() && ticket == ticket1) {
        login = field[1];
        status &= ~S_NOAUTH;
        return 1;
    }

    return 0;
}
#endif /* INCLUDE_EXTRA_COMMANDS */

std::string
get_partdir(const std::string_view &login)
{
    const auto pdir = get_conf_param("partdir", "work");
    if (match(pdir, "work"))
        return work;
    else {
        const auto path = std::format("{}/{:c}/{:c}/{}", pdir,
            tolower(login[0]), tolower(login[1]), login);
        mkdir_all(path, 0700);
        return path;
    }
}

/*
 * Assuming login is already set, do everything else necessary to initialize
 * stuff for the user.  This is usually only called once, except when
 * nobody "authenticates" it is called again.  When this happens,
 * init() has already set the user to be the Unix nobody.  We have
 * just changed login to its new value, uid=nobody, getuid=nobody,
 * and we want to get the info for the new account.
 */
void
login_user(void)
{
    /* preserve original flags throughout */
    static int old_flags = 0, first_call = 1;

    if (first_call) {
        first_call = 0;
        old_flags = (flags & (O_READONLY | O_OBSERVE));
    }

    /* Reset home */
    if (!get_user(&uid, login, st_glob.fullname, home, email)) {
        error("reading ", "user entry");
        endbbs(2);
    }

    /* Set mailbox macro */
    std::string mail(getenv("MAIL"));
    if (mail.empty())
        mail = std::format("{}/{}", get_conf_param("maildir", MAILDIR), login);
    def_macro("mailbox", DM_VAR, mail);

    /* Set global variable "work" */
    if (access(home.c_str(), X_OK)) {
        if (!str::eq(login, get_conf_param("nobody", NOBODY)))
            error("accessing ", home);

        /* If can't access home directory, continue as an observer */
        flags |= O_READONLY | O_OBSERVE;
    } else {
        /* restore original readonly/observe flag settings */
        flags = (flags & ~(O_READONLY | O_OBSERVE)) | old_flags;
    }
    work = home + "/.cfdir";
    if (access(work.c_str(), X_OK))
        work = home;

    /* Set partdir (must be done before refresh_list() below) */
    partdir = get_partdir(login);

    /* Read in PARTDIR/.cflist */
    listtime = 0;
    refresh_list();

    /* Execute user's cfonce file */
    std::string buff(work);
    source(work, ".cfonce", 0, SL_USER);

#ifdef INCLUDE_EXTRA_COMMANDS
    /* If "work" changed above, do the cflist again */
    if (!str::eq(work, buff)) { /* for cfdir command */
        source(work, ".cfonce", 0, SL_USER);
        refresh_list();
    }
#endif /* INCLUDE_EXTRA_COMMANDS */

    /* Reset fullname -- why??? */
    if (!get_user(&uid, login, st_glob.fullname, home, email)) {
        error("reading ", "user entry");
        endbbs(2);
    }
}

int
partfile_perm(void)
{
    static int ret = -1;
    if (ret < 0)
        ret = (match(get_conf_param("partdir", "work"), "work")) ? SL_USER
                                                                 : SL_OWNER;
    return ret;
}

/* fp is password file */
void
add_password(const std::string_view &login, const char *password, FILE *fp)
{
    char *cpw, salt[3];

    srand((int)time((time_t *)NULL));
    to64(&salt[0], rand(), 2);
    salt[2] = '\0';
    cpw = crypt(password, salt);
    std::println(fp, "{}:{}", login, cpw);
    error("warning", std::format("!{}", salt));
}

/* Sanity check on fullname */
bool
sane_fullname(const std::string_view &name)
{
    for (const auto c : name)
        if (c == ':' || c == '|' || !isprint(c))
            return false;
    return true;
}

/* Sanity check on email */
static bool
sane_email(const std::string_view &addr)
{
    for (const auto c : addr)
        if (c == ':' || c == '|' || !isprint(c))
            return 0;
    return 1;
}

/* others contains a list of variables to save to the dbm file,
 * excluding fullname, email, passwd
 */
static int
save_user_info(
    const std::string_view &newlogin, const std::string_view &newname,
    const std::string_view &newemail, const char *newpass,
    const char
        *others /* IN: other vars to save (space-sep), or NULL for none */
)
{
    FILE *fp, *out = stdout;
    int suid;
    const auto userfilename = get_userfile(newlogin, &suid);

#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true")) {
        if (!save_dbm(userfilename, "fullname", newname, SL_USER) ||
            !save_dbm(userfilename, "email", newemail, SL_USER) ||
            (str::eq(get_conf_param("userfile", USER_FILE),
                 get_conf_param("passfile", PASS_FILE)) &&
                !save_dbm(userfilename, "passwd", newpass, SL_USER))) {
            const auto msg = std::format(
                "ERROR: Couldn't modify user file {}\n", userfilename);
            wfputs(msg, out);
            return 0;
        }

        /* step through others and save those, except
         * fullname/email/passwd */
        if (others) {
            const auto list = str::split(others, " ");
            for (size_t i = 0; i < list.size(); i++) {
                const auto var = expand(list[i], DM_VAR);
                if (!var.empty() &&
                    !save_dbm(userfilename, list[i], var, SL_USER)) {
                    wfputs(std::format("ERROR: Couldn't modify user file {}\n",
                               userfilename),
                        out);
                    return 0;
                }
            }
        }
    } else {
#endif /* HAVE_DBM_OPEN */

        if (suid == SL_USER)
            fp = smopenw(userfilename, O_A);
        else
            fp = mopen(userfilename, O_A);
        if (fp == NULL) {
            error("opening ", userfilename);
            wfputs(std::format(
                       "ERROR: Couldn't modify user file {} \n", userfilename),
                out);
            return 0;
        }
        std::println(fp, "{}:{}:{}", newlogin, newname, newemail);
        if (suid == SL_USER)
            smclose(fp);
        else
            mclose(fp);

#ifdef HAVE_DBM_OPEN
    }
#endif

    /* Add password to passfile */
    if ((fp = mopen(get_conf_param("passfile", PASS_FILE), O_A)) == NULL) {
        error("opening", " passwd file");
        exit(1);
    }

    /* Encrypt password and save in file */
    add_password(newlogin, newpass, fp);
    mclose(fp);

    return 1;
}

/*
 * Generate a random 8-character password in static buffer
 * make sure it's not guessable by someone knowing what time the
 * account was created.
 */
char *
random_password(void)
{
    static char buff[10];
    int seed;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    seed = (getpid() << 16) | (tv.tv_usec & 0xFFFF);
    srandom(seed);
    to64(buff, random(), 4);
    to64(buff + 4, random(), 4);
    buff[8] = '\0';

    return buff;
}

/* Send email to current user with their password */
/* IN : plaintext password to include in email */
int
email_password(const char *pass)
{
    /* Compose email with their password */
    const auto filename = std::format("/tmp/notify.{}", getpid());
    def_macro("password", DM_VAR, pass);
    FILE *fout = mopen(filename, O_W);
    if (fout == NULL) {
        error("opening ", filename);
        return 0;
    }
    const auto filein =
        str::concat({expand("wwwdir", DM_VAR), "/templates/newuser.email"});
    FILE *fin = mopen(filein, O_R);
    if (fin == NULL) {
        error("opening ", filein);
        return 0;
    }
    char *str;
    while ((str = xgets(fin, stdin_stack_top)) != NULL) {
        fitemsep(fout, str, 0);
        free(str);
    }
    mclose(fin);
    mclose(fout);
    undef_name("password");

    /* Send it off */
    unix_cmd(std::format(
        "{} -t < {}", get_conf_param("sendmail", SENDMAIL), filename));
    unlink(filename.c_str());
    return 1;
}

/* command: newuser login  - just checks for validity */
/* This allows an unauthenticated user to create a new login */
/* usage: newuser login newpass newpasstoo newname newemail */
/* cfadm gets prompts, anyone else except 'nobody' gets same web account */
/* when verifyemail is false, userdbm is true, syntax will be:
 * newuser newlogin newpass newpasstoo newname newemail extras
 */
int
newuser(int argc, char **argv)
{
    struct stat st;
    char *others = NULL;
    std::string newlogin;
    char newpass1[MAX_LINE_LENGTH];
    char newpass2[MAX_LINE_LENGTH];
    std::string newname;
    std::string newemail;
    FILE *out = stdout;
    int cpid, wpid;
    int normaluser;
    if (status & S_EXECUTE)
        out = NULL;

    normaluser =
        (getuid() == get_nobody_uid() || getuid() == geteuid()) ? 0 : 1;

    /* #ifdef XXX */
    /*
     * 8/10/96 put this code back in because we currently only support
     * running newuser from the nobody account
     */
    /* Only the "nobody" account can do newuser */
    if (getuid() != get_nobody_uid()) {
        wfputs("You already have a login.\n", out);
        return 1;
    }
    /* #endif XXX */

    /* Get new login */
    if (normaluser) {
        newlogin = login;
        newname = fullname;
        newemail = email;
    } else {
        if (argc > 1)
            newlogin = argv[1];
        else {
            std::print("Choose a login: ");
            ngets(newlogin, st_glob.inp);
        }

        /* Sanity check on login */
        if (newlogin.size() < 2) {
            wfputs("Login must be at least 2 characters long\n", out);
            return 1;
        }
        for (const char *p = newlogin.c_str(); *p; p++)
            if (!isalnum(*p)) {
                const auto msg =
                    std::format("Illegal character '{:c}' (ascii {}) in "
                                "login.  Please choose another login.\n",
                        *p, *p);
                wfputs(msg, out);
                return 1;
            }

        /* Check to see if login is already in Unix or local use */
        if (getpwnam(newlogin.c_str())) {
            const auto cmd = str::concat({"/usr/local/bin/webuser ", newlogin});
            int err = unix_cmd(cmd);
            if (err)
                wfputs("Couldn't create web account.\n", out);
            else
                wfputs("Web account created\n", out);
            return 1;
        }
        std::string fn, h, e;
        if (get_local_user(newlogin, fn, h, e)) {
            const auto msg =
                std::format("The login \"{}\" is already in use.  Please "
                            "choose another login\n",
                    newlogin);
            wfputs(msg, out);
            return 1;
        }

        /* First command form simply tests legality of login */
        if (argc < 3) {
            wfputs("Login is legal\n", out);
            return 1;
        }
    }

    if (match(get_conf_param("verifyemail", VERIFY_EMAIL), "true")) {
        strcpy(newpass1, random_password());
    } else {
        /* Get new pass1 */
        if (!normaluser && argc > 2)
            strcpy(newpass1, argv[2]);
        else {
            strcpy(newpass1, getpass("New password: "));
        }

        /* Get new pass2 */
        if (!normaluser && argc > 3)
            strcpy(newpass2, argv[3]);
        else {
            strcpy(newpass2, getpass("New password (again): "));
        }

        /* Sanity checks on passwords */
        if (!str::eq(newpass1, newpass2)) {
            wfputs("The two copies of your password do not match. "
                   "Please try again.",
                out);
            return 1;
        }
        argc -= 2;
        argv += 2;
    }

    if (!normaluser) {
        /* Get new fullname */
        if (argc > 2)
            newname = argv[2];
        else {
            std::print("Full name: ");
            fflush(stdout);
            ngets(newname, st_glob.inp);
        }

        /* Sanity check on fullname */
        if (!sane_fullname(newname)) {
            wfputs("Illegal character in fullname.  Please try "
                   "again.\n",
                out);
            return 1;
        }

        /* Get new email */
        if (argc > 3)
            newemail = argv[3];
        else {
            std::println("Email address: ");
            fflush(stdout);
            ngets(newemail, st_glob.inp);
        }

        /* Sanity check on email */
        if (!sane_email(newemail.c_str())) {
            wfputs("Illegal character in email address.  Please try "
                   "again.\n",
                out);
            return 1;
        }
#ifdef HAVE_DBM_OPEN
        /* argv[4] contains a list of variables to save to the dbm
         * file, excluding fullname, email, passwd */
        if (argc > 4)
            others = argv[4];
#endif
    }

    /* Make a home directory */
    const auto homedir =
        std::format("{}/{:c}/{:c}/{}", get_conf_param("userhome", USER_HOME),
            tolower(newlogin[0]), tolower(newlogin[1]), newlogin);
    /* FORK */
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    cpid = fork();
    if (cpid) { /* parent */
        if (cpid < 0)
            return 1; /* error: couldn't fork */
        while ((wpid = wait((int *)0)) != cpid && wpid != -1);
    } else { /* child */
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(0);

        setuid(getuid());
        setgid(getgid());

        mkdir_all(homedir, 0755);
        exit(0);
    }

    /* Now test to make sure it succeeded */
    if (stat(homedir.c_str(), &st) || st.st_uid != getuid()) {
        error("creating ", homedir);
        wfputs("System administrator needs to check file permissions on "
               "web home dir.\n",
            out);
        return 1;
    }

    /* Save newuser info to user file (could be done in user dir) */
    save_user_info(newlogin, newname, newemail, newpass1, others);

    /* Set partdir */
    /* Copy templates to user's home directory */
    partdir = get_partdir(newlogin);

    /* copy default .cflist */
    copy_file(str::concat({bbsdir, "/defaults/.cflist"}),
        str::concat({partdir, "/.cflist"}), partfile_perm());

    /* copy default .cfrc */
    copy_file(str::concat({bbsdir, "/defaults/.cfrc"}),
        str::concat({homedir, "/.cfrc"}), SL_USER);

    /* copy default .cfonce */
    copy_file(str::concat({bbsdir, "/defaults/.cfonce"}),
        str::concat({homedir, "/.cfonce"}), SL_USER);

    if (match(get_conf_param("verifyemail", VERIFY_EMAIL), "true")) {
        /* Set fields which might be referenced */
        login = newlogin;
        st_glob.fullname = newname;
        email = newemail;
        home = homedir;

        email_password(newpass1);
        wfputs("You are now registered and should receive email "
               "shortly.\n",
            out);
    } else {
        wfputs("You are now registered and may log in.\n", out);
    }
    return 1;
}

#ifdef INCLUDE_EXTRA_COMMANDS
/* command: authenticate ticket */
/* This can authenticate a user according to the Yapp local user file ONLY */
int
authenticate(int argc, char **argv)
{
    if (argc != 2) {
        std::println("Usage: authenticate ticket");
        return 1;
    }

    /* Trash newline */
    if (argv[1][strlen(argv[1]) - 1] == '\n')
        argv[1][strlen(argv[1]) - 1] = '\0';
    if (do_authenticate(argv[1]))
        login_user();
    else
        std::println("Authentication failed.");
    return 1;
}
#endif /* INCLUDE_EXTRA_COMMANDS */
#endif /* WWW */

/*******************************/
/* Functions to modify .cflist */
/*******************************/

/*
 * Reload cflist if different timestamp than when we last read it
 * or if different size (in case changed in < 1 second)
 */
static void
force_refresh_list(const std::string &path)
{
    struct stat st{};
    if (stat(path.c_str(), &st) < 0)
        return;

    current = -1;
    cflist = grab_file(partdir, ".cflist", GF_WORD | GF_SILENT | GF_IGNCMT);

    /* Set up cfliststr */
    std::vector<std::string_view> cflistvs(cflist.begin(), cflist.end());
    cfliststr = " " + str::join(" ", cflistvs) + (!cflistvs.empty() ? " " : "");

    listtime = st.st_mtime;
    listsize = st.st_size;
}

void
refresh_list(void)
{
    if (cfliststr.empty())
        cfliststr = " ";
    struct stat st{};
    const auto path = std::format("{}/{}", partdir, ".cflist");
    if (stat(path.c_str(), &st) >= 0 &&
        (st.st_mtime != listtime || st.st_size != listsize))
        force_refresh_list(path);
}

/*
 * Append conference name to user's .cflist
 */
static void
add_cflist(const std::string_view &cfname)
{

    const auto path = std::format("{}/{}", partdir, ".cflist");
    force_refresh_list(path);
    auto perm = partfile_perm();
    if (perm == SL_USER) {
        FILE *fp = smopenw(path, O_A);
        if (fp == nullptr)
            return;
        std::println(fp, "{}", cfname);
        smclose(fp);
    } else { /* SL_OWNER */
        FILE *fp = mopen(path, O_A);
        if (fp == nullptr)
            return;
        std::println(fp, "{}", cfname);
        mclose(fp);
    }
    /*
     * Manually add to cflist in memory.
     * We don't use `refresh_list` because it may
     * have changed less than 1 second ago.
     */
    force_refresh_list(path);
}

/********************************************************/
/* Delete conference from .cflist                       */
/********************************************************/
/* ARGUMENTS:                        */
/* Conference name to delete      */
int
del_cflist(const std::string &cfname)
{
    FILE *newfp;
    int perm = partfile_perm();

    const auto path = std::format("{}/{}", partdir, ".cflist");
    force_refresh_list(path);

    /* First see if it's in the list at all */
    auto it = std::find(cflist.begin(), cflist.end(), cfname);
    if (it == cflist.end())
        return 0;
    cflist.erase(it);

    if (perm == SL_USER)
        newfp = smopenw(path, O_W);
    else /* SL_OWNER */
        newfp = mopen(path, O_W);
    if (newfp == NULL) {
        std::println("Couldn't open .cflist for writing");
        return 0;
    }
    for (const auto &cf : cflist) std::println(newfp, "{}", cf);

    if (perm == SL_USER)
        smclose(newfp);
    else /* SL_OWNER */
        mclose(newfp);

    force_refresh_list(path);

    return 1;
}

void
show_cflist(void)
{
    refresh_list();
    for (auto i = 0; i < cflist.size(); i++)
        std::println("{} {}", (current == i) ? "-->" : "   ", cflist[i]);
}

/* The "cflist" command */
int
do_cflist(int argc, char **argv)
{
    int i;

    if (argc > 2 && match(argv[1], "a_dd")) {
        for (i = 2; i < argc; i++) add_cflist(argv[i]);
    } else if (argc > 2 && match(argv[1], "d_elete")) {
        for (i = 2; i < argc; i++) del_cflist(argv[i]);
    } else if (argc == 2 && match(argv[1], "s_how")) {
        show_cflist();
    } else if (argc == 2 && match(argv[1], "r_estart")) {
        if (cflist.empty()) {
            current = -1;
            do_next(0, nullptr);
        }
    } else {
        std::println("usage: cflist add <{}> ...", conference(0));
        std::println("       cflist delete <{}> ...", conference(0));
        std::println("       cflist show");
        std::println("       cflist restart");
    }

    return 1;
}

/*
 * Change user fullname (the permanent, global one)
 */
int
chfn(           /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    std::string newname;
    std::string newemail;
    FILE *fp = NULL, *tmp = NULL, *out = stdout;
    const char *oldname = NULL;
    char tmpname[MAX_LINE_LENGTH];
    std::string line;
    char *w;
    char *oldemail = NULL;
    int found = 0, suid;
#ifdef HAVE_DBM_OPEN
    int dbm = 0;
    char oldnamebuff[MAX_LINE_LENGTH];
    char oldemailbuff[MAX_LINE_LENGTH];
#endif /* HAVE_DBM_OPEN */

    if (status & S_EXECUTE)
        out = NULL;

    /* If it's a Unix user, do a normal chfn command */
    if (getuid() != get_nobody_uid()) {
        unix_cmd("/usr/bin/chfn");

        /* Reload fullname */
        if (!get_user(&uid, login, fullname, home, email)) {
            error("reading ", "user entry");
            endbbs(2);
        }
        return 1;
    }

    /* Insure that the user has authenticated */
    if (status & S_NOAUTH) {
        wfputs("You are not authenticated.\n", out);
        return 1;
    }
    const auto userfile = get_userfile(login, &suid);

#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true")) {
        strcpy(oldnamebuff, get_dbm(userfile, "fullname", SL_USER).c_str());
        strcpy(oldemailbuff, get_dbm(userfile, "email", SL_USER).c_str());
        if (!oldnamebuff[0] && !oldemailbuff[0]) {
            error("opening ", userfile);
            return 1;
        }
        found = 1;
        dbm = 1;
        oldname = oldnamebuff;
        oldemail = oldemailbuff;
    }
    if (!dbm) {
#endif /* HAVE_DBM_OPEN */

        /* Open old userfile (could be in user dir) */
        if (suid == SL_USER)
            fp = smopenr(userfile, O_R);
        else
            fp = mopen(userfile, O_R);
        if (fp == NULL) {
            error("opening ", userfile);
            exit(1);
        }

        /* Open new userfile */
        const auto tmpname = std::format("{}.{}", userfile, getpid());
        if (suid == SL_USER)
            tmp = smopenw(tmpname, O_W);
        else
            tmp = mopen(tmpname, O_W);
        if (tmp == NULL) {
            error("opening ", tmpname);
            exit(1);
        }

        /* Find login in user file */
        while (ngets(line, fp)) {
            if (line.empty() || line[0] == '#') {
                std::println(tmp, "{}", line);
                continue;
            }
            char *cline = line.data();
            if ((w = strchr(cline, ':')) != NULL) {
                *w = '\0';
                if (!str::eq(login, cline))
                    std::println(tmp, "{}:{}", cline, w + 1);
                else {
                    found = 1;
                    oldname = w + 1;
                    oldemail = strchr(w + 1, ':');
                    if (!oldemail)
                        continue;
                    *oldemail++ = '\0';
                    break;
                }
            }
        }

        /* Verify that it's a user account, not a Unix account */
        if (!found) {
            wfputs("Couldn't find local Yapp user information.\n", out);
            if (suid == SL_USER) {
                smclose(tmp);
                smclose(fp);
            } else {
                mclose(tmp);
                mclose(fp);
            }
            return 1;
        }
#ifdef HAVE_DBM_OPEN
    }
#endif

    /* Get new fullname */
    if (argc > 1)
        newname = argv[1];
    else {
        if (!(flags & O_QUIET)) {
            std::println("Your old name is: {}", oldname);
            std::print("Enter replacement or return to keep old? ");
        }
        if (!ngets(newname, st_glob.inp) || newname.empty()) {
            wfputs("Name not changed.\n", out);
        } else {
            wfputs("Fullname changed.\n", out);
        }
    }

    /* Get new email */
    if (argc > 2)
        newemail = argv[2];
    else {
        if (!(flags & O_QUIET)) {
            std::println("Your old email address is: {}", oldemail);
            std::print("Enter replacement or return to keep old? ");
        }
        if (!ngets(newemail, st_glob.inp) || newemail.empty()) {
            wfputs("Email address not changed.\n", out);
            newemail = oldemail;
        } else
            wfputs("Email address changed.\n", out);
    }

    /* Sanity check on fullname */
    if (!sane_fullname(newname)) {
        wfputs("Illegal character in fullname.  Please try again.\n", out);
#ifdef HAVE_DBM_OPEN
        if (!dbm) {
#endif

            if (suid == SL_USER) {
                smclose(tmp);
                smclose(fp);
            } else {
                mclose(tmp);
                mclose(fp);
            }
#ifdef HAVE_DBM_OPEN
        }
#endif
        return 1;
    }

    /* Sanity check on email */
    if (!sane_email(newemail)) {
        wfputs("Illegal character in email address.  Please try again.\n", out);
#ifdef HAVE_DBM_OPEN
        if (!dbm) {
#endif

            if (suid == SL_USER) {
                smclose(tmp);
                smclose(fp);
            } else {
                mclose(tmp);
                mclose(fp);
            }
#ifdef HAVE_DBM_OPEN
        }
#endif
        return 1;
    }
#ifdef HAVE_DBM_OPEN
    if (dbm) {
        save_dbm(userfile, "fullname", newname, SL_USER);
        save_dbm(userfile, "email", newemail, SL_USER);
        return 1;
    }
#endif /* HAVE_DBM_OPEN */

    /* Insert new one */
    std::println(tmp, "{}:{}:{}", login, newname, newemail);

    /* Copy remainder of file */
    while (ngets(line, fp)) std::println(tmp, "{}", line);

    if (suid == SL_USER) {
        smclose(tmp);
        smclose(fp);
    } else {
        mclose(tmp);
        mclose(fp);
    }

    move_file(tmpname, userfile, suid);
    /*
       if (rename(tmpname, userfile)) {
          error("renaming ", tmpname);
          exit(1);
       }
    */

    return 1;
}
/*
 * Change user password
 * usage: passwd [oldpass [newpass1 [newpass2]]]
 */
int
passwd(         /* ARGUMENTS:             */
    int argc,   /* Number of arguments */
    char **argv /* Argument list       */
)
{
    char oldpass[MAX_LINE_LENGTH], newpass1[MAX_LINE_LENGTH],
        newpass2[MAX_LINE_LENGTH];
    FILE *fp = NULL, *tmp = NULL, *out = stdout;
    const char *passfile = NULL;
    char tmpname[MAX_LINE_LENGTH];
    std::string line;
    char *w = NULL;
    char *cpw;
    int found = 0;
#ifdef HAVE_DBM_OPEN
    int dbm = 0;
    char savedpass[MAX_LINE_LENGTH];
    const char *userfile = NULL;
#endif /* HAVE_DBM_OPEN */

    if (status & S_EXECUTE)
        out = NULL;

    /* If it's a Unix user, do a normal passwd command */
    if (getuid() != get_nobody_uid()) {
        unix_cmd("/usr/bin/passwd");
        return 1;
    }
    if (status & S_NOAUTH) {
        wfputs("You are not authenticated.\n", out);
        return 1;
    }
#ifdef HAVE_DBM_OPEN
    if (match(get_conf_param("userdbm", USERDBM), "true") &&
        str::eq(get_conf_param("userfile", USER_FILE),
            get_conf_param("passfile", PASS_FILE))) {
        int suid;
        const auto userfile = get_userfile(login, &suid);
        strcpy(savedpass, get_dbm(userfile, "passwd", SL_USER).c_str());
        if (!savedpass[0]) {
            error("opening ", userfile);
            return 1;
        }
        found = 1;
        dbm = 1;
    }
    if (!dbm) {
#endif /* HAVE_DBM_OPEN */

        /* Open old passfile */
        const auto passfile = get_conf_param("passfile", PASS_FILE);
        if ((fp = mopen(passfile, O_R)) == NULL) {
            error("opening ", passfile);
            exit(1);
        }

        /* Open new passfile */
        const auto tmpname = std::format("{}.{}", passfile, getpid());
        if ((tmp = mopen(tmpname, O_W)) == NULL) {
            error("opening ", tmpname);
            exit(1);
        }

        /* Find login in pass file */
        while (ngets(line, fp)) {
            if (line.empty() || line[0] == '#') {
                std::println(tmp, "{}", line);
                continue;
            }
            const auto w = line.find(':');
            if (w != std::string::npos) {
                const auto lineview = std::string_view(line);
                const auto user = lineview.substr(0, w);
                const auto pass = lineview.substr(w + 1);
                if (!str::eq(login, line))
                    std::println(tmp, "{}:{}", user, pass);
                else {
                    found = 1;
                    break;
                }
            }
        }

        /* Verify that it's a user account, not a Unix account */
        if (!found) {
            wfputs("You're not using a local Yapp account.\n", out);
            mclose(tmp);
            mclose(fp);
            return 1;
        }
#ifdef HAVE_DBM_OPEN
    }
#endif

    /* Get old password */
    if (argc > 1)
        strcpy(oldpass, argv[1]);
    else {
        wfputs("Old password: ", out);
        fflush(out);
        strcpy(oldpass, getpass(""));
    }

    /* Verify old password */
#ifdef HAVE_DBM_OPEN
    if (dbm) {
        if (!str::eq(oldpass, savedpass)) {
            wfputs("You did not enter your old password correctly.\n", out);
            return 1;
        }
    } else {
#endif

        cpw = crypt(oldpass, w + 1);
        if (!str::eq(cpw, w + 1)) {
            wfputs("You did not enter your old password correctly.\n", out);
            mclose(tmp);
            mclose(fp);
            return 1;
        }
#ifdef HAVE_DBM_OPEN
    }
#endif

    if (argc > 2)
        strcpy(newpass1, argv[2]);
    else {
        wfputs("New password: ", out);
        fflush(out);
        strcpy(newpass1, getpass(""));
    }

    if (argc > 3)
        strcpy(newpass2, argv[3]);
    else {
        wfputs("Retype new password: ", out);
        fflush(out);
        strcpy(newpass2, getpass(""));
    }

    if (!newpass1[0] || !newpass2[0] || !str::eq(newpass1, newpass2)) {
        wfputs("The two copies of your password do not match. Please try "
               "again.\n",
            out);
#ifdef HAVE_DBM_OPEN
        if (!dbm) {
#endif
            mclose(tmp);
            mclose(fp);
#ifdef HAVE_DBM_OPEN
        }
#endif
        return 1;
    }
#ifdef HAVE_DBM_OPEN
    if (dbm && str::eq(get_conf_param("userfile", USER_FILE),
                   get_conf_param("passfile", PASS_FILE))) {
        save_dbm(userfile, "passwd", newpass1, SL_USER);
        wfputs("Password changed.\n", out);
        return 1;
    }
#endif /* HAVE_DBM_OPEN */

    /* Encrypt new one */
    add_password(login, newpass1, tmp);
    /*
       (void)srand((int)time((time_t *)NULL));
       to64(&salt[0],rand(),2);
       cpw = crypt(newpass1,salt);
       std::println(tmp, "{}:{}", login, cpw);
    */

    /* Copy remainder of file */
    while (ngets(line, fp)) std::println(tmp, "{}", line);

    if (fp)
        mclose(fp);
    if (tmp)
        mclose(tmp);

    if (rename(tmpname, passfile)) {
        error("renaming ", tmpname);
        exit(1);
    }
    wfputs("Password changed.\n", out);
    return 1;
}
