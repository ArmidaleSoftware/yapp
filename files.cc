// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "files.h"

#include <sys/file.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <format>
#include <print>
#include <string>

#include "globals.h"
#include "lib.h"
#include "yapp.h"

/* Information about each open file */
typedef struct fd_tt {
    std::string filename;
    int flg;
    int fd;
    int pid; /* child process id, if secure */
    struct fd_tt *next;
} fd_t;
static fd_t *first_fd = 0;

/******************************************************************************/
/* DUMP ALL FILES CURRENTLY OPEN                                              */
/******************************************************************************/
void
mdump(void)
{
    /* ARGUMENTS: (none)  */
    fd_t *thisfd;
    for (thisfd = first_fd; thisfd; thisfd = thisfd->next) {
        std::println("mdump: fd={} filename='{}' flg={:x}", thisfd->fd,
            thisfd->filename, thisfd->flg);
    }
}
/******************************************************************************/
/* ADD AN OPEN FD TO THE DATABASE (FROM SPOPEN/MOPEN)                         */
/******************************************************************************/
void
madd(int fd, const std::string_view &file, int flg, int pid)
{
    fd_t *thisfd;
    /* Save info for debugging */
    thisfd = new fd_t{
        .filename = std::string(file),
        .flg = flg,
        .fd = fd,
        .pid = pid,
        .next = first_fd,
    };
    first_fd = thisfd;
}
/******************************************************************************/
/* OPEN A FILE AND LOCK IT FOR EXCLUSIVE ACCESS                               */
/******************************************************************************/
/* ARGUMENTS: */
/* Filename to open */
/* Flag: 0=append only, 1=create new (only) */
FILE *
mopen(const std::string &file, long flg)
{
    struct stat st;
    short err = 0;
    int fd, perm;
    const char *modestr;
    FILE *fp;

    if (debug & DB_FILES)
        std::println("mopen: flags={:x}", flg);

    /* Process flags: ensure it exists or doesn't exist if required */
    // XXX: TOCTOU bug here.
    if (flg & (O_EXCL | O_NOCREATE))
        err = stat(file.c_str(), &st);
    if (err && (flg & O_NOCREATE)) {
        if (!(flg & O_SILENT))
            error("opening ", file);
        return NULL; /* append: doesn't exist  */
    } else if (!err && (flg & O_CREAT) && (flg & O_EXCL)) {
        if (!(flg & O_SILENT))
            error("creating ", file);
        return NULL; /* create: already exists */
    }
    perm = umask(0);

    /* Open file */
    fd = open(file.c_str(), flg & O_PASSTHRU, (flg & O_PRIVATE) ? 0600 : 0644);
    if (fd < 0) {
        if (!(flg & O_SILENT))
            error("opening ", file);
        umask(perm);
        return NULL;
    }
    /* Lock it */
    if (flg & O_LOCK) {
        if (flock(fd, (flg & O_NOBLOCK) ? LOCK_EX | LOCK_NB : LOCK_EX) < 0) {
            /*
             * ignore this error, since it may be /dev/null or
             * something weird like that. 8/4/95
             * error("Lock failed on ", file);
             *
             * allow it to continue without lock, can't flock an NFS
             * file in BSD1.1 it seems
             * umask(perm); return NULL;
             *
             * 8/28/95 need it to fail for license, fail only on
             * NOBLOCK
             */
            if (flg & O_NOBLOCK) {
                umask(perm);
                close(fd);
                /*error("Lock failed on ", file);*/
                return NULL;
            }
        }
    }

    /* Open/lock succeeded */
    umask(perm);
    if (flg & O_APPEND)
        lseek(fd, 0L, 2);

    /* Determine mode string */
    modestr = "";
    if ((flg & O_WPLUS) == O_WPLUS)
        modestr = "w+";
    else if ((flg & O_W) == O_W)
        modestr = "w";
    else if ((flg & O_APLUS) == O_APLUS)
        modestr = "a+";
    else if ((flg & O_A) == O_A)
        modestr = "a";
    else if ((flg & O_RPLUS) == O_RPLUS)
        modestr = "r+"; /* should be next to last */
    else if ((flg & O_R) == O_R)
        modestr = "r"; /* MUST be last */
    else
        std::println("KKK Invalid mopen mode");

    /* Save info for debugging */
    madd(fd, file, flg, 0);

    /* Reopen fd as file pointer of equivalent mode */
    if ((fp = fdopen(fd, modestr)) == NULL) {
        if ((flg & O_SILENT) == 0) {
            const auto msg =
                std::format("{} for {} after mode {:x}\n", file, modestr, flg);
            error("reopening ", msg);
        }
    }
    return fp;
}

int
get_pid(FILE *fp)
{
    if (fp == nullptr) {
        error("invalid file pointer passed to get_pid");
        return 0;
    }
    for (fd_t *thisfd = first_fd; thisfd != nullptr; thisfd = thisfd->next)
        if (thisfd->fd == fileno(fp))
            return thisfd->pid;
    error("file pointer not found by get_pid");
    return 0; /* not found */
}
/******************************************************************************/
/* CLOSE AND UNLOCK A FILE                                                    */
/******************************************************************************/
/* ARGUMENTS:               */
/* File pointer to close */
int
mclose(FILE *fp)
{
    fd_t *thisfd, *prev = 0;
    int ret;

    fflush(fp);
    for (thisfd = first_fd; thisfd && thisfd->fd != fileno(fp);
        prev = thisfd, thisfd = thisfd->next);
    if (!thisfd) {
        std::println("Tried to close unopened file");
        return 1; /* not found */
    }
    if (!fp) {
        std::println("Tried to close null file");
        return 1;
    }
    ret = fclose(fp); /* flock automatically closes */

    /* Remove from debugging database */
    if (!prev)
        first_fd = thisfd->next;
    else
        prev->next = thisfd->next;
    delete thisfd;

    return ret;
}
/******************************************************************************/
/* VERIFY THAT ALL FILES HAVE BEEN CLOSED                                     */
/******************************************************************************/
void
mcheck(void)
{
    if (!first_fd) {
        if (debug & DB_FILES)
            puts("mcheck: Everything closed.\n");
    } else {
        std::println("mcheck: Error, failed to close the following:");
        mdump();
    }
}
