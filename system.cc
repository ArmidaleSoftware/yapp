// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 *  SYSTEM.C - Dave Thaler 3/1/93
 * This file does secure replacements for popen and system calls
 */

#include "system.h"

#include <sys/select.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <gdbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <print>
#include <string>
#include <vector>

#include "driver.h"
#include "edbuf.h"
#include "files.h"
#include "globals.h"
#include "lib.h"
#include "macro.h"
#include "main.h"
#include "str.h"
#include "yapp.h"

extern FILE *conffp;

void
process_pipe_input(int fd)
{
    char *p, *eb;
    int n, sz;
    fd_set rfds;
    struct timeval tm;
    /* read output into evalbuf */
    sz = strlen(evalbuf);
    eb = evalbuf + sz;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tm.tv_sec = 0;
    tm.tv_usec = 0;

    /* do { */
    /* must do a non-blocking read in case there was no output */
    if (select(fd + 1, &rfds, NULL, NULL, &tm) > 0) {
        n = read(fd, eb, sizeof(evalbuf) - 1 - sz);
        if (n > 0)
            eb[n] = '\0';
        if (debug & DB_PIPE) {
            std::println(stderr, "read in {} chars from pipe", n);
            fflush(stderr);
        }
    }
    /* } while (n>=0); * repeat until process ends and we get an EOF */

    /* Convert all newlines to spaces */
    for (p = evalbuf; *p; p++) {
        if (*p == '\n' || *p == '\r')
            *p = ' ';
    }

    /*std::println("Got !{}! in evalbuf", evalbuf);*/
}

static int sp_cpid;

int
smclose(FILE *fp)
{
    int statusp, i, wpid;
    int pid = get_pid(fp);

    i = mclose(fp);
    while ((wpid = waitpid(pid, &statusp, 0)) != pid && wpid != -1);

    return i;
}
/******************************************************************************/
/* SECURE mopen() - OPEN A USER FILE                                          */
/******************************************************************************/
/* RETURNS: Open file pointer, or NULL on error */
/* ARGUMENTS: */
/* Filename to open */
/* Flag: 0=append only, 1=create new (only) */
FILE *
smopenw(const std::string &file, long flg)
{
    int fd[2];
    FILE *fp;
    int pid;

    if (pipe(fd) < 0)
        return NULL;
    if ((fp = fdopen(fd[1], "w")) == NULL)
        return NULL;

    /* The following two lines are not necessary, and even slow the
     * program down, but make seen be more accurate in some cases, since
     * less info is buffered by the pager.
     */
    /* fcntl(fd[1], F_SETFL, O_SYNC);
       setsockopt(fd[1],SOL_SOCKET, SO_SNDBUF, 4, 1);
     */

    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    pid = fork();
    if (pid) { /* parent */
        if (pid < 0)
            return NULL; /* error: couldn't fork */
        close(fd[0]);
        status |= S_PIPE;
        madd(fd[1], file, O_PIPE, pid);
        return fp;
    } else { /* child */
        FILE *ufp;
        char buff[1024];
        int len;
        setuid(getuid());
        setgid(getgid());
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(0);
        dup(fd[0]);
        close(fd[1]);

        if ((ufp = mopen(file, flg)) != NULL) {
            /* Now copy all standard input to ufp */
            while ((len = read(0, buff, sizeof(buff))) > 0) {
                fwrite(buff, len, 1, ufp);
            }
            mclose(ufp);
        }
        exit(1);
    }
    return NULL;
}

/* RETURNS: Open file pointer, or NULL on error */
/* ARGUMENTS: */
/* Filename to open */
/* Flag: 0=append only, 1=create new (only) */
FILE *
smopenr(const std::string &file, long flg)
{
    int fd[2];
    FILE *fp;
    int pid;
    if (pipe(fd) < 0)
        return NULL;
    if ((fp = fdopen(fd[0], "r")) == NULL)
        return NULL;

    /* The following two lines are not necessary, and even slow the
     * program down, but make seen be more accurate in some cases, since
     * less info is buffered by the pager. */
    /* fcntl(fd[1], F_SETFL, O_SYNC);
       setsockopt(fd[1],SOL_SOCKET, SO_SNDBUF, 4, 1);
     */

    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    pid = fork();
    if (pid) { /* parent */
        if (pid < 0)
            return NULL; /* error: couldn't fork */
        close(fd[1]);
        status |= S_PIPE;
        madd(fd[0], file, O_PIPE, pid);
        return fp;
    } else { /* child */
        FILE *ufp;
        char buff[1024];
        int len;
        setuid(getuid());
        setgid(getgid());
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(1);
        if (status & S_PAGER)
            fclose(st_glob.outp);
        dup(fd[1]);
        close(fd[0]);

        /* Close stdin, stderr in case they are also pipes */
        /* 10/28/95: these were 'if 0'ed out, but I'm not sure why.
         * Solaris requires this to be done or else the following test
         * fails: "source read2" gives duplicate output, where read2
         * is: echo normal eval < rh echo duplicated and rh is:
         * anything */
        /* 1/13/97 These were inside the if block below, but if the
         * file doesn't exist, then we have the same problem:
         * #!/usr/local/bin/bbs -qx debug ioredir set source join yapp
         * echo HELLO and the HELLO is printed twice. */
        close(0);
        close(2);

        /*std::println("opening {} for {:x}", file, flg);*/
        if ((ufp = mopen(file, flg)) != NULL) {

            /* Now copy ufp to standard output */
            while ((len = fread(buff, 1, sizeof(buff), ufp)) > 0) {
                write(1, buff, len);
            }

            mclose(ufp);
        }
        exit(1);
    }
    return NULL;
}

/******************************************************************************/
/* SECURE sdpopen() - OPEN A TWO-WAY PIPE TO A PROCESS                        */
/******************************************************************************/
/* RETURNS: 1 on success, 0 on failure */
/* ARGUMENTS:                          */
/* OUT: file pointer for input         */
/* OUT: file pointer for output        */
/* IN : command to execute             */
int
sdpopen(FILE **finp, FILE **foutp, const std::string &cmd)
{
    int fd_tocmd[2];
    int fd_fromcmd[2];
    FILE *fin, *fout;

    if (foutp && pipe(fd_tocmd) < 0)
        return 0;

    if (finp && pipe(fd_fromcmd) < 0) {
        close(fd_tocmd[0]);
        close(fd_tocmd[1]);
        return 0;
    }

    /* The following two lines are not necessary, and even slow the
     * program down, but make seen be more accurate in some cases, since
     * less info is buffered by the pager. */
    /* fcntl(fd[1], F_SETFL, O_SYNC);
       setsockopt(fd[1],SOL_SOCKET, SO_SNDBUF, 4, 1);
     */
    fflush(stdout);
    fflush(stderr);
    sp_cpid = fork();
    if (sp_cpid < 0) {
        perror("fork failed");
        return 0;
    }
    if (sp_cpid == 0) {
        // Child.
        setuid(getuid());
        setgid(getgid());
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        if (foutp) {
            close(0);
            dup(fd_tocmd[0]);
            close(fd_tocmd[1]);
        }
        if (finp) {
            close(1);
            dup(fd_fromcmd[1]);
            close(fd_fromcmd[1]);
        }
        if (confidx >= 0)
            mclose(conffp);
        if (strpbrk(cmd.c_str(), "<>*?|![]{}~`$&';\\\"") == NULL) {
            const auto args = str::splits(cmd, " ", true);
            auto argv = new const char *[args.size() + 1];
            std::transform(args.begin(), args.end(), argv,
                [](const std::string &s) { return s.c_str(); });
            argv[args.size()] = nullptr;
            execvp(argv[0], (char *const *)argv);
        } else {
            const auto shpathname = expand("shell", DM_VAR);
            const auto *shpath = shpathname.c_str();
            const auto *sh = strrchr(shpath, '/');
            if (sh == nullptr)
                sh = shpath;
            else
                ++sh;
            execl(shpath, sh, "-c", cmd.c_str(), (char *)NULL);
        }
        std::println("oops: Can't execute \"{}\"!", cmd);
        exit(1);
    }
    // Parent.
    status |= S_PIPE;
    if (debug & DB_PIPE) {
        std::print(stderr, "(stderr) Opened pipe to '{}': ", cmd);
        if (finp)
            std::print(stderr, "in fd {}", fd_fromcmd[0]);
        if (finp && foutp)
            std::print(stderr, " ");
        if (foutp)
            std::print(stderr, "out fd {}", fd_tocmd[1]);
        std::println(stderr, ".");
    }
    if (foutp) {
        close(fd_tocmd[0]);
        madd(fd_tocmd[1], cmd, O_PIPE, sp_cpid);
        if ((fout = fdopen(fd_tocmd[1], "w")) == NULL)
            return 0;
        *foutp = fout;
    }
    if (finp) {
        close(fd_fromcmd[1]);
        madd(fd_fromcmd[0], cmd, O_PIPE, sp_cpid);
        if ((fin = fdopen(fd_fromcmd[0], "r")) == NULL)
            return 0;
        *finp = fin;
    }
    return 1;
}

/******************************************************************************/
/* SECURE spopen() - OPEN A PIPE TO A PROCESS                                 */
/******************************************************************************/
/* RETURNS: file pointer of pipe */
/* ARGUMENTS: */
/* Command to pipe to */
FILE *
spopen(const std::string &cmd)
{
    if (status & (S_PAGER | S_SOCKET))
        wputs("Error, pipe already open\n");
    FILE *fp;
    if (!sdpopen(NULL, &fp, cmd))
        return NULL;
    return fp;
}

/******************************************************************************/
/* DUMP A STRING TO A SECURE PIPE                                             */
/******************************************************************************/
/* RETURNS: # bytes written */
/* ARGUMENTS: */
/* File descriptor of pipe */
/* String to dump */
int
spout(int fd, const char *buff)
{
    return write(fd, buff, strlen(buff));
}
/******************************************************************************/
/* CLOSE A SECURE PIPE                                                        */
/******************************************************************************/
int
sdpclose(FILE *fin, FILE *fout)
{
    int i;
    int statusp, wpid;
    if (!(status & (S_PAGER | S_PIPE))) {
        std::println("Error, pipe not open");
    }
    if (fout) {
        if (debug & DB_PIPE) {
            std::println(
                stderr, "(stderr) Closing pipe on fd {}", fileno(fout));
            std::println("(stdout) Closing pipe on fd {}", fileno(fout));
            fflush(stdout);
        }
        i = mclose(fout);
    }
    if (status & S_PIPE)
        while ((wpid = waitpid(sp_cpid, &statusp, 0)) != sp_cpid && wpid != -1);

    if (fin) {
        if (status & S_EXECUTE)
            process_pipe_input(fileno(fin));
        i = mclose(fin);
    }
    status &= ~(S_PIPE | S_PAGER | S_INT);
    return i;
}
/******************************************************************************/
/* CLOSE A SECURE PIPE                                                        */
/******************************************************************************/
/* RETURNS: error code */
/* ARGUMENTS: */
/* File pointer to close */
int
spclose(FILE *pp)
{
    return sdpclose(NULL, pp);
}

int exit_status = 0; /* exit status of last unix command executed */
/******************************************************************************/
/* SECURE system() - EXECUTE A UNIX COMMAND                                   */
/******************************************************************************/
/* RETURNS: exit status of command */
/* ARGUMENTS: */
/* Command to execute */
int
unix_cmd(const std::string &cmd)
{
    int cpid, wpid;
    int statusp;
    int fd[2];

    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    /* Prepare to capture output for a `unix ...` replacement */
    if (status & S_EXECUTE) {
        if (pipe(fd) < 0)
            return -1;
    }

    cpid = fork();
    if (cpid) { /* parent */
        if (cpid < 0)
            return -1; /* error: couldn't fork */
        signal(SIGINT, SIG_IGN);
        signal(SIGPIPE, SIG_IGN);
        while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
        signal(SIGINT, handle_int);
        signal(SIGPIPE, handle_pipe);
        if (status & S_EXECUTE) {
            process_pipe_input(fd[0]);
        }
    } else { /* child */
        setuid(getuid());
        setgid(getgid());
        if (status & S_EXECUTE) {
            close(1);     /* close stdout */
            dup(fd[1]);   /* reopen pipe-out as stdin */
            close(fd[0]); /* close pipe-in */
        }
        /* If this is a script, and we didn't specify a specific input
         * redirection, then restore the REAL stdin */
        if ((status & S_BATCH) && !(status & S_NOSTDIN)) {
            close(0);
            dup(saved_stdin[0].fd); /* dup(real_stdin); */
        }

        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        if (confidx >= 0)
            mclose(conffp);
        if (strpbrk(cmd.c_str(), "<>*?|![]{}~`$&';\\\"") == NULL) {
            const auto args = str::splits(cmd, " ", true);
            auto argv = new const char *[args.size() + 1];
            std::transform(args.begin(), args.end(), argv,
                [](const std::string &s) { return s.c_str(); });
            argv[args.size()] = nullptr;
            execvp(argv[0], (char *const *)argv);
            std::println("oops: Can't execute \"{}\"!", cmd);
        } else {
            const auto shvar = expand("shell", DM_VAR);
            const auto *shpath = shvar.c_str();
            const auto *sh = strrchr(shpath, '/');
            if (sh == nullptr)
                sh = shpath;
            else
                ++sh;
            execl(shpath, sh, "-c", cmd.c_str(), (char *)NULL);
        }
        exit(1);
    }

    /*std::println("UNIX2 ftell={}", ftell(st_glob.inp));*/
    exit_status = WEXITSTATUS(statusp);
    return statusp;
}
/******************************************************************************/
/* REMOVE A FILE                                                              */
/******************************************************************************/
/* RETURNS: error code */
/* ARGUMENTS: */
/* File to delete */
/* sec: As owner(0) or user(1)? */
int
rm(const std::string &file, int sec)
{
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);
    if (!sec)
        return unlink(file.c_str());
    auto cpid = fork();
    if (cpid < 0)
        return -1; /* error: couldn't fork */
    if (cpid == 0) {
        /* child */
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(0);
        setuid(getuid());
        setgid(getgid());
        exit(unlink(file.c_str()));
    }
    /* parent */
    pid_t wpid;
    int statusp;
    while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid > 0);

    /*
     * This Error message removed 7/24/95 at request of janc, so
     * that his `gate` editor will not give this error
     *
     * if (statusp) error("removing ",file);
     */

    return statusp;
}
/******************************************************************************/
/* SECURELY COPY ONE FILE TO ANOTHER                                          */
/******************************************************************************/
/* RETURNS: error code */
/* ARGUMENTS: */
/* Source file */
/* Destination file */
/* As owner(0) or user(1)? */
int
copy_file(const std::string &src, const std::string &dest, int sec)
{
    FILE *fsrc, *fdest;
    int c;
    int cpid, wpid;
    int statusp;
    long mod;
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    cpid = fork();
    if (cpid) { /* parent */
        if (cpid < 0)
            return -1; /* error: couldn't fork */
        while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
    } else { /* child */
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(0);

        if (confidx >= 0)
            mclose(conffp);

        mod = O_W;
        if (!sec && (st_glob.c_security & CT_BASIC))
            mod |= O_PRIVATE;

        if (sec) { /* cfadm to user */
            if ((fsrc = mopen(src, O_R)) == NULL)
                exit(1);
            setuid(getuid());
            setgid(getgid());
            if ((fdest = mopen(dest, mod)) == NULL) {
                mclose(fsrc);
                exit(1);
            }
        } else { /* user to cfadm */
            if ((fdest = mopen(dest, mod)) == NULL)
                exit(1);
            setuid(getuid());
            setgid(getgid());
            if ((fsrc = mopen(src, O_R)) == NULL) {
                mclose(fdest);
                exit(1);
            }
        }

        while ((c = fgetc(fsrc)) != EOF) fputc(c, fdest);
        mclose(fdest);
        mclose(fsrc);
        exit(0);
    }
    return statusp;
}
/******************************************************************************/
/* SECURELY MOVE ONE FILE TO ANOTHER                                          */
/******************************************************************************/
/* RETURNS: error code */
/* ARGUMENTS: */
/* Source file */
/* Destination file */
/* As owner(0) or user(1)? */
int
move_file(const std::string &src, const std::string &dest, int sec)
{
    int cpid, wpid, statusp;
    int ret;

    if (sec == SL_OWNER) {
        ret = rename(src.c_str(), dest.c_str());
        if (ret < 0)
            error("renaming ", src);
        return ret;
    }
    fflush(stdout);
    if (status & S_PAGER)
        fflush(st_glob.outp);

    cpid = fork();
    if (cpid < 0)
        return -1;  /* error: couldn't fork */
    if (cpid > 0) { /* parent */
        while ((wpid = waitpid(cpid, &statusp, 0)) != cpid && wpid != -1);
    } else { /* child */
        signal(SIGINT, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        close(0);

        if (confidx >= 0)
            mclose(conffp);

        setuid(getuid());
        setgid(getgid());

        exit(rename(src.c_str(), dest.c_str()));
    }
    if (statusp)
        error("renaming ", src);

    return statusp;
}

/******************************************************************************/
/* INVOKE EDITOR ON A FILE                                                    */
/******************************************************************************/
/* RETURNS: (nothing)                       */
/* ARGUMENTS:                               */
/* Directory containing file                */
/* Filename to edit                         */
/* Flags: 1=visual, 2=force                 */
int
priv_edit(const std::string &dir, const std::string &file, int flags)
{
    std::string origfile(dir);
    if (!file.empty()) {
        origfile.append("/");
        origfile.append(file);
    }
    auto cfbuffer = str::concat({work, "/cf.buffer"});
    /* Copy file to cf.buffer owned by user */
    copy_file(origfile, cfbuffer, SL_USER); /* Assume readable by user? XXX */
    /* Edit it */
    auto ret = edit(cfbuffer, "", flags & 1);
    if (!ret) {
        std::println("Aborting...");
        return 0;
    }
    if ((flags & 2) || get_yes("Ok to install this? ", false))
        copy_file(cfbuffer, origfile, SL_OWNER);
    rm(cfbuffer, SL_USER);
    return ret;
}

/* RETURNS: (nothing)                       */
/* ARGUMENTS:                               */
/* Directory containing file                */
/* Filename to edit                         */
/* Flag: visual editor?                     */
int
edit(const std::string &dir, const std::string &file, bool visual)
{
    struct stat st;

    std::string origfile(dir);
    if (!file.empty()) {
        origfile.append("/");
        origfile.append(file);
    }

    const auto ed = expand(visual ? "visual" : "editor", DM_VAR);

    if (str::eq(ed, "builtin")) {
        flags &= ~O_EDALWAYS;

        /* Copy file to user's cf.buffer */
        auto cfbuffer = str::concat({work, "/cf.buffer"});
        const auto is_cfbuffer = (cfbuffer == origfile);
        if (!is_cfbuffer)
            copy_file(origfile, cfbuffer, SL_USER);
        if (text_loop(0, "text")) {
            /* Now install cf.buffer contents in original filename
             */
            if (!is_cfbuffer) {
                copy_file(cfbuffer, origfile, SL_USER);
                rm(cfbuffer, SL_USER);
            }
        }
    } else {
        const auto cmd = str::join(" ", {ed, origfile});
        unix_cmd(cmd);
    }

    return stat(origfile.c_str(), &st) == 0;
}

/* RETURNS: 1 on success, 0 on failure */
int
ssave_dbm(const std::string &userfile, const std::string_view &key,
    const std::string_view &value)
{
    const auto flags = GDBM_READER | GDBM_WRITER | GDBM_WRCREAT;
    auto db = gdbm_open(userfile.c_str(), 0, flags, 0644, nullptr);
    if (db == nullptr)
        return 0;
    datum dkey;
    dkey.dptr = (char *)key.data();
    dkey.dsize = key.size();
    datum dvalue;
    dvalue.dptr = (char *)value.data();
    dvalue.dsize = value.size();
    gdbm_store(db, dkey, dvalue, GDBM_REPLACE);
    gdbm_close(db);
    return 1;
}

/* RETURNS: 1 on success, 0 on failure */
int
save_dbm(const std::string &userfile, const std::string_view &key,
    const std::string_view &value, int suid)
{

    if (suid == SL_OWNER) {
        return ssave_dbm(userfile, key, value);

    } else { /* suid==SL_USER */
        fflush(stdout);
        if (status & S_PAGER)
            fflush(st_glob.outp);
        auto cpid = fork();
        if (cpid < 0)
            return -1;   /* error: couldn't fork */
        if (cpid == 0) { /* child */
            signal(SIGINT, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            close(0);

            setuid(getuid());
            setgid(getgid());
            exit(!ssave_dbm(userfile, key, value));
        }
        /* Parent */
        int wpid, status;
        while ((wpid = waitpid(cpid, &status, 0)) != cpid && wpid > 0);
        return !status;
    }
}

void
dump_dbm(const std::string &userfile)
{
    auto db = gdbm_open(userfile.c_str(), 0, GDBM_READER, 0644, nullptr);
    if (db == nullptr) {
        perror(userfile.c_str());
        return;
    }
    datum dkey = gdbm_firstkey(db);
    while (dkey.dptr != nullptr) {
        datum dval = gdbm_fetch(db, dkey);
        std::println("{:.{}}: {:.{}}", (const char *)dkey.dptr, dkey.dsize,
            (const char *)dval.dptr, dval.dsize);
        datum next = gdbm_nextkey(db, dkey);
        free(dkey.dptr);
        dkey = next;
    }
    gdbm_close(db);
}

static std::string
sget_dbm(const std::string &userfile, const std::string_view &key)
{
    const auto flags = GDBM_READER | GDBM_WRITER | GDBM_WRCREAT;
    auto db = gdbm_open(userfile.c_str(), 0, flags, 0644, nullptr);
    if (db == nullptr)
        return "";
    datum dkey;
    dkey.dptr = (char *)key.data();
    dkey.dsize = key.size();
    datum dvalue = gdbm_fetch(db, dkey);
    gdbm_close(db);
    if (dvalue.dptr == nullptr)
        return "";
    std::string value((const char *)dvalue.dptr, dvalue.dsize);
    free(dvalue.dptr);
    return value;
}

/* This function must use a pipe to send back the string */
/* suid: As owner(0) or user(1)? */
std::string
get_dbm(const std::string &userfile, const std::string_view &key, int suid)
{
    if (suid == SL_OWNER) {
        return sget_dbm(userfile, key);

    } else { /* suid==SL_USER */
        int fd[2];

        /* Open a pipe to use to pass string back through */
        if (pipe(fd) < 0)
            return "";

        /* Flush output to avoid duplication */
        fflush(stdout);
        if (status & S_PAGER)
            fflush(st_glob.outp);

        auto cpid = fork();
        if (cpid < 0)
            return ""; /* error: couldn't fork */
        if (cpid == 0) {
            /* child */
            signal(SIGINT, SIG_DFL);
            signal(SIGPIPE, SIG_DFL);
            close(0);
            close(fd[0]);

            setuid(getuid());
            setgid(getgid());

            const auto str = sget_dbm(userfile, key);
            write(fd[1], str.c_str(), str.size());
            close(fd[1]);
            exit(0);
        }
        /* parent */
        close(fd[1]);
        char buf[4096];
        ssize_t n = read(fd[0], buf, sizeof(buf));
        close(fd[0]);
        std::string value(buf, n);
        int wpid, status;
        while ((wpid = waitpid(cpid, &status, 0)) != cpid && wpid > 0);
        if (n <= 0)
            return "";
        return std::string(buf, n);
    }
}
