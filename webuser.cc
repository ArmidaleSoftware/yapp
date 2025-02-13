// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 *  This program creates a web account from an existing Unix account.
 *  The user's web password is duplicated from the user's Unix password.
 *  This means that THIS PROGRAM MUST RUN SETUID ROOT or else it typically
 *  cannot get the encrypted form of the user's password.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <format>
#include <iostream>
#include <print>

#include "str.h"

#undef HAVE_GETSPNAM /* Solaris                  */
#undef HAVE_DBM_OPEN /* Most platforms have dbm_open()          */

#ifdef HAVE_DBM_OPEN
#include <ndbm.h>
#include <fcntl.h>
#endif
#ifdef HAVE_GETSPNAM
#include <shadow.h>
#define GETSPNAM getspnam
#else
#define GETSPNAM getpwnam
#endif

/*
 * The defines below are the DEFAULTs which Yapp uses when the yapp.conf
 * file doesn't override some option.  Do not change these, or they won't
 * match what Yapp uses.
 */
#define USER_HOME "/arbornet/m-net/bbs/home"
#define PASS_FILE "/arbornet/m-net/bbs/etc/.htpasswd"
#define USER_FILE "/arbornet/m-net/bbs/etc/passwd"
#define USERDBM "false"
#define NOBODY "nobody"
#define CFADM "cfadm"
#define USRADM "usradm"

struct passwd nobody, cfadm, usradm;

const char *conf[] = {"/arbornet/m-net/bbs/yapp.conf", "/etc/yapp3.1.conf",
    "/usr/local/etc/yapp3.1.conf", "/usr/bbs/yapp3.1.conf", "./yapp3.1.conf",
    "/etc/yapp.conf", "/usr/local/etc/yapp.conf", "/usr/bbs/yapp.conf",
    "./yapp.conf", NULL};

#define MAX_PARAMS 20

char param_key[MAX_PARAMS][20], param_value[MAX_PARAMS][80];
int num_params = 0;
/* Fill in the param arrays above from the yapp.conf file */
void
get_yapp_conf(void)
{
    FILE *fp;
    char buff[256], *ptr;
    for (int i = 0; conf[i]; i++) {
        if ((fp = fopen(conf[i], "r")) != NULL) {
            while (num_params < MAX_PARAMS && fgets(buff, sizeof(buff), fp)) {
                if (buff[0] == '#' || !(ptr = strchr(buff, ':')))
                    continue;
                *ptr++ = '\0';
                if (ptr[strlen(ptr) - 1] == '\n')
                    ptr[strlen(ptr) - 1] = '\0';
                strcpy(param_key[num_params], buff);
                strcpy(param_value[num_params++], ptr);
            }
            fclose(fp);
            return;
        }
    }
}
/* Look up the value of a yapp configuration option */
/* OUT: value of requested parameter */
/* IN : parameter name to get value of */
/* IN : default value if not found */
const char *
get_conf_param(const char *name, const char *def)
{
    for (int i = 0; i < num_params; i++) {
        if (str::eq(param_key[i], name))
            return param_value[i];
    }
    return def;
}
/* Get the home directory used when logging in from the web */
/* IN: login */
/* OUT: home directory */
std::string
get_homedir(const std::string_view &login)
{
    return std::format("{}/{:c}/{:c}/{}", get_conf_param("userhome", USER_HOME),
        tolower(login[0]), tolower(login[1]), login);
}
/* Get the directory holding the user's participation files */
/* OUT: participation file directory */
/* IN : login */
const char *
get_partdir(const char *login)
{
    static std::string partdir;

    const char *pd = get_conf_param("partdir", "work");
    if (str::eq(pd, "work"))
        partdir = get_homedir(login);
    else {
        partdir = std::format(
            "{}/{:c}/{:c}/{}", pd, tolower(login[0]), tolower(login[1]), login);
    }
    return partdir.c_str();
}
/* Figure out where the user's information should be stored */
/* OUT: filename of user's information file */
/* IN : login */
/* OUT: uid to own file */
/* OUT: gid to own file */
std::string
get_userfile(const std::string_view &login, uid_t *uid, gid_t *gid)
{
    static std::string path;
    const auto home = get_homedir(login);

#ifdef HAVE_DBM_OPEN
    /* If userdbm=true then it's in ~/<login> */
    if (str::eq(get_conf_param("userdbm", USERDBM), "true")) {
        path = std::format("{}/{:c}/{:c}/{}/{}",
            get_conf_param("userhome", USER_HOME), tolower(login[0]),
            tolower(login[1]), login, login);
        if (uid)
            *uid = nobody.pw_uid;
        if (gid)
            *gid = nobody.pw_gid;
        return path.c_str();
    }
#endif

    /* Otherwise it's in the location specified by userfile */
    std::string file = get_conf_param("userfile", USER_FILE);
    if (file[0] == '~' && file[1] == '/') {
        file = std::format("{}/{}", home, file.c_str() + 2);
        if (uid)
            *uid = nobody.pw_uid;
        if (gid)
            *gid = nobody.pw_gid;
    } else {
        if (uid)
            *uid = cfadm.pw_uid;
        if (gid)
            *gid = cfadm.pw_gid;
    }
    return file;
}

#ifdef HAVE_DBM_OPEN
int /* RETURNS: 1 on success, 0 on failure */
save_dbm(DBM *db, const char *keystr, const char *valstr)
{
    datum dkey, dval;
    dkey.dptr = (void *)keystr;
    dkey.dsize = strlen(keystr) + 1;
    dval.dptr = (void *)valstr;
    dval.dsize = strlen(valstr) + 1;

    return dbm_store(db, dkey, dval, DBM_REPLACE);
}

const char *
get_dbm(const char *userfile, const char *keystr)
{
    datum dkey, dval;
    DBM *db;
    db = dbm_open(userfile, O_RDWR, 0644);
    if (!db)
        return "";

    dkey.dptr = (void *)keystr;
    dkey.dsize = strlen(keystr) + 1;
    dval = dbm_fetch(db, dkey);
    dbm_close(db);

    return (dval.dptr) ? dval.dptr : "";
}
#endif /* HAVE_DBM_OPEN */

/* Save user information in the appropriate file */
/* IN: login */
/* IN: full name */
/* IN: email address */
void
create_user_info(const std::string_view &login,
    const std::string_view &fullname, const std::string_view &email)
{
    FILE *fp, *tmp;
    uid_t uid;
    gid_t gid;
    const auto userfile = get_userfile(login, &uid, &gid);
    char buff[256];

#ifdef HAVE_DBM_OPEN
    /* Support DBM files */
    if (str::eq(get_conf_param("userdbm", USERDBM), "true")) {
        datum dkey, dval;
        DBM *db;
        if ((db = dbm_open(userfile, O_RDWR | O_CREAT, 0644)) == NULL) {
            perror("dbm_open");
            exit(1);
        }
        if (save_dbm(db, "fullname", fullname) ||
            save_dbm(db, "email", email)) {
            perror("dbm_store");
            exit(1);
        }
        dbm_close(db);

        chown(userfile, uid, gid);
        chmod(userfile, 0644);

        return;
    }
#endif /* HAVE_DBM_OPEN */

    /* Support flat text files */

    /* Open new userfile */
    const auto pathtmp = std::format("{}.{}", userfile, getpid());
    if ((tmp = fopen(pathtmp.c_str(), "w")) == NULL) {
        const auto msg = std::string("fopen ") + pathtmp;
        perror(msg.c_str());
        exit(1);
    }

    if ((fp = fopen(userfile.c_str(), "r")) != NULL) {
        const auto ustr = std::format("{}:", login);
        while (fgets(buff, sizeof(buff), fp)) {
            const auto line = std::string_view(buff);
            /* Find login in user file */
            if (line.starts_with(ustr))
                /* Add user's info to userfile */
                std::println(tmp, "{}:{}:{}", login, fullname, email);
            else
                /* Copy rest of file */
                std::print(tmp, "{}", line);
        }
        fclose(fp);
    }

    /* Commit the changes */
    fclose(tmp);
    if (rename(pathtmp.c_str(), userfile.c_str())) {
        perror("rename");
        exit(1);
    }
    chown(userfile.c_str(), uid, gid);
    chmod(userfile.c_str(), 0644);
}
/* Get the local hostname for use in constructing a full email address */
/* IN : size of buffer */
/* OUT: fully-qualified domain name of local host */
void
get_local_hostname(char *buff, int len)
{
    buff[0] = '\0';
    if (gethostname(buff, len))
        perror("getting host name");

    /* If hostname is not fully qualified, try to get it from
     * /etc/resolv.conf */
    if (!strchr(buff, '.')) {
        FILE *fp;
        if ((fp = fopen("/etc/resolv.conf", "r")) != NULL) {
            char line[256], field[80], value[80];
            while (fgets(line, sizeof(line), fp)) {
                if (sscanf(line, "%s%s", field, value) == 2 &&
                    str::eq(field, "domain")) {
                    strlcat(buff, ".", len);
                    strlcat(buff, value, len);
                    break;
                }
            }
            fclose(fp);
        }
    }
}
/* Create a web password file entry and home directory */
void
create_web_account(char *login, /* IN : login to create */
    char *passwd,               /* IN : encrypted passwd */
    uid_t uid,                  /* IN : user ID of 'nobody' */
    gid_t gid                   /* IN : group ID of 'nobody' */
)
{
    FILE *fp, *tmp;
    const char *passfile = get_conf_param("passfile", PASS_FILE);
    char buff[256];

    if (!str::eq(passfile, get_conf_param("userfile", USER_FILE))) {
        /* Open new .htpasswd file */
        const auto tmpname = std::format("{}.{}", passfile, getpid());
        if ((tmp = fopen(tmpname.c_str(), "w")) == NULL) {
            const auto msg = std::format("fopen {}", tmpname);
            perror(msg.c_str());
            exit(1);
        }

        if ((fp = fopen(passfile, "r")) != NULL) {
            const auto ustr = std::format("{}:", login);
            while (fgets(buff, sizeof(buff), fp)) {
                const auto line = std::string_view(buff);
                /* Find login in pass file */
                if (line.starts_with(ustr))
                    /* Add password to .htpasswd */
                    std::println(tmp, "{}:{}", login, passwd);
                else
                    /* Copy remainder of file */
                    std::print(tmp, "%s", line);
            }
            fclose(fp);
        }

        /* Commit the changes */
        fclose(tmp);
        if (rename(tmpname.c_str(), passfile)) {
            perror("rename");
            exit(1);
        }
        chown(passfile, cfadm.pw_uid, cfadm.pw_gid);
        chmod(passfile, 0644);
    }

    /* Make user's web home directory (and any subdirs needed before it) */
    auto home = std::format(
        "{}/{:c}", get_conf_param("userhome", USER_HOME), tolower(login[0]));
    const char *chome = home.c_str();
    mkdir(chome, 0755);
    if (chown(chome, uid, gid))
        perror(chome);
    home.push_back('/');
    home.push_back(tolower(login[1]));
    chome = home.c_str();
    mkdir(chome, 0755);
    if (chown(chome, uid, gid))
        perror(chome);
    home.push_back('/');
    home.append(login);
    chome = home.c_str();
    mkdir(chome, 0755);
    if (chown(chome, uid, gid))
        perror(chome);
}

void
usage(void)
{
    std::println(std::cerr,
        "Yapp {} (c)1996 Armidale Software\n usage: webuser [-dehlprsuv] "
        "[login]",
        VERSION);
    std::println(std::cerr, " -d        Disable logins to account");
    std::println(std::cerr, " -e        Enable disabled account");
    std::println(std::cerr, " -h        Help (display this text)");
    std::println(std::cerr, " -l        List all web accounts");
    std::println(std::cerr, " -p        Change password on account");
    std::println(std::cerr, " -r        Remove account");
    std::println(std::cerr, " -s        Show account information");
    std::println(std::cerr,
        " -u        Create/update web account and password (default)");
    std::println(std::cerr, " -v        Version (display this text)");
    std::println(std::cerr, " login     Specify Unix login");
    exit(1);
}

char *
get_web_password(char *login) /* IN : login to find */
{
    const char *passfile = get_conf_param("passfile", PASS_FILE);
    FILE *fp;
    static char buff[256];

#ifdef HAVE_DBM_OPEN
    if (str::eq(get_conf_param("userdbm", USERDBM), "true") &&
        str::eq(get_conf_param("userfile", USER_FILE),
            get_conf_param("passfile", PASS_FILE))) {
        char *userfile = get_userfile(login, NULL, NULL);
        return get_dbm(userfile, "passwd");
    }
#endif /* HAVE_DBM_OPEN */

    /* Find login in pass file */
    if ((fp = fopen(passfile, "r")) != NULL) {
        const auto ustr = std::format("{}:", login);
        while (fgets(buff, sizeof(buff), fp)) {
            const auto line = std::string_view(buff);
            if (line.starts_with(ustr)) {
                if (buff[strlen(buff) - 1] == '\n')
                    buff[strlen(buff) - 1] = '\0';
                fclose(fp);
                return buff + ustr.size(); /* start of password */
            }
        }
        fclose(fp);
    }
    return 0;
}

void
get_user_info(char *login, /* IN : Unix login    */
    char *fullname,        /* OUT: Full name     */
    char *email            /* OUT: Email address */
)
{
    static char buff[256];
    FILE *fp;
    const auto userfile = get_userfile(login, NULL, NULL);

#ifdef HAVE_DBM_OPEN
    if (str::eq(get_conf_param("userdbm", USERDBM), "true")) {
        strcpy(fullname, get_dbm(userfile, "fullname"));
        strcpy(email, get_dbm(userfile, "email"));
        return;
    }
#endif /* HAVE_DBM_OPEN */

    /* Find login in user file */
    if ((fp = fopen(userfile.c_str(), "r")) != NULL) {
        const auto ustr = std::format("{}:", login);
        while (fgets(buff, sizeof(buff), fp)) {
            const auto line = std::string_view(buff);
            if (line.starts_with(ustr)) {
                (void)strtok(buff, ":\n");
                strcpy(fullname, strtok(NULL, ":\n"));
                strcpy(email, strtok(NULL, ":\n"));
                break;
            }
        }
        fclose(fp);
    }
}
/* Make a web account with the same password as a Unix account */
void
update(char *login /* IN: Unix login */
)
{
#ifdef HAVE_GETSPNAM
    struct spwd *spw;
#endif
    struct passwd *pw, user;
    char hostname[MAXHOSTNAMELEN];
    /* If root, usradm, or nobody, then allow arbitrary login to be
     * specified */
    pw = getpwuid(getuid());
    if (pw && (pw->pw_uid == 0 || pw->pw_uid == usradm.pw_uid ||
                  pw->pw_uid == nobody.pw_uid))
        pw = getpwnam(login);
    else if (!str::eq(login, getlogin())) {
        std::println("webuser: Permission denied");
        exit(1);
    }
    if (pw == NULL) {
        std::println("webuser: no such login");
        exit(1);
    }
    if (!pw->pw_uid) {
        std::println("webuser: cannot create a web account with root access");
        exit(1);
    }
    memcpy(&user, pw, sizeof(user)); /* save everything */

    /* Construct home directory */
#ifdef HAVE_GETSPNAM
    spw = getspnam(pw->pw_name); /* get encrypted password */
    create_web_account(
        user.pw_name, spw->sp_pwdp, nobody.pw_uid, nobody.pw_gid);
#else
    create_web_account(
        user.pw_name, user.pw_passwd, nobody.pw_uid, nobody.pw_gid);
#endif

    /* Construct email address */
    get_local_hostname(hostname, sizeof(hostname));
    const auto email = std::format("{}@{}", user.pw_name, hostname);

    /* Add web user information */
    create_user_info(user.pw_name, strtok(user.pw_gecos, ","), email);
}
/* Re-enable a disabled web account */
void
enable(char *login /* IN: Web login */
)
{
    char *old_ep;
    /* Only root and usradm may do this */
    if (getuid() != 0 && getuid() != usradm.pw_uid) {
        std::println("webuser: Permission denied");
        exit(1);
    }

    /* Get old password */
    old_ep = get_web_password(login);
    if (!old_ep) {
        std::println("webuser: no such login '{}'", login);
        exit(1);
    }

    /* Make sure it isn't already enabled */
    if (strncmp(old_ep, "*:", 2)) {
        std::println("webuser: '{}' is already enabled", login);
        exit(1);
    }

    /* Change password */
    create_web_account(login, old_ep + 2, nobody.pw_uid, nobody.pw_gid);
}
/* Disable a web account */
void
disable(char *login /* IN: Web login */
)
{
    char *old_ep;
    char new_ep[256];
    /* Only root and usradm may do this */
    if (getuid() != 0 && getuid() != usradm.pw_uid) {
        std::println("webuser: Permission denied");
        exit(1);
    }

    /* Get old password */
    old_ep = get_web_password(login);
    if (!old_ep) {
        std::println("webuser: no such login '{}'", login);
        exit(1);
    }

    /* Make sure it isn't already disabled */
    if (!strncmp(old_ep, "*:", 2)) {
        std::println("webuser: '{}' is already disabled", login);
        exit(1);
    }

    /* Change password */
    strlcpy(new_ep, "*:", sizeof(new_ep));
    strlcat(new_ep, old_ep, sizeof(new_ep));
    create_web_account(login, new_ep, nobody.pw_uid, nobody.pw_gid);
}
/* Recursively remove a directory and everything under it */
void
recursive_rmdir(const std::string &path)
{
    DIR *dp;
    struct dirent *fp;
    struct stat st;

    if ((dp = opendir(path.c_str())) == NULL) {
        perror(path.c_str());
        return;
    }
    while ((fp = readdir(dp)) != NULL) {
        if (str::eq(fp->d_name, ".") || str::eq(fp->d_name, ".."))
            continue;
        const auto pname = std::format("{}/{}", path, fp->d_name);
        if (stat(pname.c_str(), &st) != 0) {
            perror(pname.c_str());
            continue;
        }
        if (st.st_mode & S_IFDIR) {
            recursive_rmdir(pname);
        } else {
            if (unlink(pname.c_str()))
                perror(pname.c_str());
        }
    }
    closedir(dp);
    if (rmdir(path.c_str()))
        perror(path.c_str());
}
/* Remove a web account */
void
rmuser(char *login /* IN: Web login */
)
{
    FILE *fp, *tmp;
    const char *passfile = get_conf_param("passfile", PASS_FILE);
    uid_t uid;
    gid_t gid;
    const auto userfile = get_userfile(login, &uid, &gid);
    char buff[PATH_MAX];
    const auto home = get_homedir(login);
    char *epass;

    /* Only root and usradm may do this */
    if (getuid() != 0 && getuid() != usradm.pw_uid) {
        std::println("webuser: Permission denied");
        exit(1);
    }

    /* See if account exists */
    epass = get_web_password(login);
    if (!epass) {
        std::println("webuser: no such login '{}'", login);
        exit(1);
    }

    /* Prompt for verification */
    std::print("Delete all files for {} [no]? ", login);
    std::fflush(stdout);
    fgets(buff, sizeof(buff), stdin);
    if (tolower(buff[0]) != 'y') {
        std::println("webuser: Deletion aborted");
        exit(1);
    }

    /* REMOVE PASSWORD FILE ENTRY... */
    if (passfile != userfile) {
        /* Open new .htpasswd file */
        const auto tmpname = std::format("{}.{}", passfile, getpid());
        if ((tmp = fopen(tmpname.c_str(), "w")) == NULL) {
            const auto msg = "fopen " + tmpname;
            perror(msg.c_str());
            exit(1);
        }

        /* Copy all of passfile except any line(s) for specified login
         */
        if ((fp = fopen(passfile, "r")) != NULL) {
            const auto ustr = std::format("{}:", login);
            while (fgets(buff, sizeof(buff), fp)) {
                const auto line = std::string_view(buff);
                if (!line.starts_with(ustr))
                    std::print(tmp, "{}", line);
            }
            fclose(fp);
        }

        /* Commit the changes */
        fclose(tmp);
        if (rename(tmpname.c_str(), passfile)) {
            perror("rename");
            exit(1);
        }
        chown(passfile, cfadm.pw_uid, cfadm.pw_gid);
        chmod(passfile, 0644);
    }

    /* If using a combined userfile, remove the entry from there */
    if (!userfile.starts_with(home)) {
        /* Open new userfile */
        const auto tmpname = std::format("{}.{}", userfile, getpid());
        if ((tmp = fopen(tmpname.c_str(), "w")) == NULL) {
            const std::string msg("fopen " + tmpname);
            perror(msg.c_str());
            exit(1);
        }

        /* Find login in user file */
        if ((fp = fopen(userfile.c_str(), "r")) != NULL) {
            const auto ustr = std::format("{}:", login);
            while (fgets(buff, sizeof(buff), fp)) {
                const auto line = std::string_view(buff);
                if (!line.starts_with(ustr))
                    std::print(tmp, "{}", line);
            }
            fclose(fp);
        }

        /* Commit the changes */
        fclose(tmp);
        if (rename(tmpname.c_str(), userfile.c_str())) {
            perror("rename");
            exit(1);
        }
        chown(userfile.c_str(), uid, gid);
        chmod(userfile.c_str(), 0644);
    }

    /* Remove the web home directory */
    recursive_rmdir(home);

    /* If there is no Unix account of the same name */
    if (!getpwnam(login)) {
        /* remove the partfile dir */
        recursive_rmdir(get_partdir(login));
    }
}
time_t
get_last_change_time(char *login /* IN : login to check */
)
{
    DIR *dp;
    struct dirent *fp;
    struct stat st;
    time_t last = 0;
    const char *partdir = get_partdir(login);

    if ((dp = opendir(partdir)) == NULL)
        return 0;
    while ((fp = readdir(dp)) != NULL) {
        const auto file = std::format("{}/{}", partdir, fp->d_name);
        if (stat(file.c_str(), &st) || !(st.st_mode & S_IFREG))
            continue;
        if (st.st_mtime > last)
            last = st.st_mtime;
    }
    closedir(dp);
    return last;
}
/* List all web accounts */
void
listall(void)
{
    const char *passfile = get_conf_param("passfile", PASS_FILE);
    FILE *fp;
    char buff[256], fullname[256], email[256], tstr[40];
    time_t tm, ctm;
    std::println(
        "S Login      Date   Email                               Full name");

    /* For each entry in the web password file... */
    if ((fp = fopen(passfile, "r")) != NULL) {
        char *p;
        while (fgets(buff, sizeof(buff), fp)) {
            p = strchr(buff, ':');
            if (!p)
                continue;
            (*p) = '\0';

            get_user_info(buff, fullname, email);
            tm = get_last_change_time(buff);
            strcpy(tstr, ((tm) ? ctime(&tm) + 4 : "--- --"));
            tstr[6] = '\0';
            time(&ctm);
            if (strncmp(tstr + 16, ctime(&ctm) + 20, 4)) {
                strncpy(tstr, tstr + 16, 4);
                tstr[4] = '\0';
            }
            std::println("{:c} {:<10s} {:<6s} {:<35s} {}",
                ((p[1] == '*') ? 'D' : '-'), buff, tstr, email, fullname);
        }
        fclose(fp);
    }
}

void
show(char *login /* IN: Web login */
)
{
    char *epass; /* encrypted web password */
    char fullname[256], email[256];
    time_t tm;
    epass = get_web_password(login);
    if (!epass) {
        std::println("webuser: no such login '{}'", login);
        exit(1);
    }
    std::println("Web login     : {}", login);
    std::println("Status        : {}",
        (!strncmp(epass, "*:", 2)) ? "Disabled" : "Active");

#ifdef HAVE_DBM_OPEN
    if (str::eq(get_conf_param("userdbm", USERDBM), "true")) {
        DBM *db;
        char *userfile = get_userfile(login, NULL, NULL);
        if ((db = dbm_open(userfile, O_RDONLY, 0644)) != NULL) {
            datum dkey, dval;
            /* XXX need to alphabetize this list when printing it
             * either A) malloc array, fill in, qsort, and walk it
             * or  B) 2 nested for loops and keep the best next
             * key in 1st one */
            for (dkey = dbm_firstkey(db); dkey.dptr != NULL;
                dkey = dbm_nextkey(db)) {
                dbm_fetch(db, dkey);
                dval = dbm_fetch(db, dkey);
                dkey.dptr[0] = toupper(dkey.dptr[0]);
                std::println("{:<14.{}s}: {:.{}s}", (const char *)dkey.dptr,
                    dkey.dlen, (const char *)dval.dptr, dval.dlen);
            }
            dbm_close(db);
        }
    } else {
#endif /* HAVE_DBM_OPEN */
        get_user_info(login, fullname, email);
        std::println("Full name     : {}", fullname);
        std::println("Email address : {}", email);
#ifdef HAVE_DBM_OPEN
    }
#endif /* HAVE_DBM_OPEN */

    tm = get_last_change_time(login);
    std::print("Last read time: {}", ((tm) ? ctime(&tm) : "never"));
}

void
chpass(char *login) /* IN: Web login */
{
    char *p, *old_ep;
    char newpass1[256];
    char newpass2[256];

    /* If root or usradm, then allow arbitrary login to be specified */
    if (login && !str::eq(login, getlogin()) && getuid() != 0 &&
        getuid() != usradm.pw_uid) {
        std::println("webuser: Permission denied");
        exit(1);
    }

    /* Make sure a current account exists */
    old_ep = get_web_password(login);
    if (!old_ep) {
        std::println("webuser: no such login '{}'", login);
        exit(1);
    }

    /* Get new password */
    p = getpass("New password:");
    strcpy(newpass1, crypt(p, old_ep));
    memset(p, 0, strlen(p));

    p = getpass("Retype new password:");
    strcpy(newpass2, crypt(p, old_ep));
    memset(p, 0, strlen(p));

    if (!str::eq(newpass1, newpass2)) {
        std::println("webuser: password mismatch");
        exit(1);
    }

    /* Change password */
    create_web_account(login, newpass1, nobody.pw_uid, nobody.pw_gid);
}

int
main(int argc, char **argv)
{
    struct passwd *pw;
    char *login = NULL;
    const char *options = "dehlprsuv"; /* Command-line options */
    int i;
    char cmd = 'u';
    while ((i = getopt(argc, argv, options)) != -1) {
        switch (i) {
        case 'd':
            cmd = i;
            break;
        case 'e':
            cmd = i;
            break;
        case 'r':
            cmd = i;
            break;
        case 'p':
            cmd = i;
            break;
        case 's':
            cmd = i;
            break;
        case 'u':
            cmd = i;
            break;
        case 'l':
            cmd = i;
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (argc > 0) {
        login = argv[0];
        argc--;
        argv++;
    }
    if (argc > 0)
        usage();

    if (geteuid()) {
        std::println("This program must be installed setuid root");
        exit(0);
    }

    /* Read in yapp configuration file */
    get_yapp_conf();

    /* Look up some standard logins to get UIDs for file permissions */
    if ((pw = getpwnam(get_conf_param("nobody", NOBODY))) == NULL) {
        perror("nobody");
        exit(1);
    }
    memcpy(&nobody, pw, sizeof(cfadm)); /* save uid/gid */
    if ((pw = getpwnam(get_conf_param("cfadm", CFADM))) == NULL) {
        perror("cfadm");
        exit(1);
    }
    memcpy(&cfadm, pw, sizeof(cfadm)); /* save uid/gid */
    if ((pw = getpwnam(get_conf_param("usradm", USRADM))) == NULL) {
        usradm.pw_uid = 0;
        usradm.pw_gid = 0;
    } else
        memcpy(&usradm, pw, sizeof(usradm)); /* save uid/gid */

    if (!login)
        login = getlogin();
    if (!login) {
        perror("getlogin");
        exit(1);
    }
    switch (cmd) {
    case 'd':
        disable(login);
        break;
    case 'e':
        enable(login);
        break;
    case 'l':
        listall();
        break;
    case 'p':
        chpass(login);
        break;
    case 'r':
        rmuser(login);
        break;
    case 's':
        show(login);
        break;
    case 'u':
        update(login);
        break;
    }

    exit(0);
}
