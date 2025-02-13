// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include "main.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <print>

#include "driver.h"
#include "yapp.h"

/******************************************************************************/
/* PROCESS COMMAND LINE ARGUMENTS                                             */
/******************************************************************************
Function:    main
Called by:   (user)
Arguments:   command line arguments
Returns:     (nothing)
Calls:       init to set up global variables
             join to start up first conference
             command to process user commands
Description: This function parses the command line arguments,
             and acts as the main driver.
*******************************************************************************/
int
main(int argc, char **argv)
{
    if (!strncmp(argv[0] + strlen(argv[0]) - 9, "yappdebug", 9)) {
        std::println("Content-type: text/plain\n\nSTART OF OUTPUT:");
        fflush(stdout);
    }
    init(argc, argv); /* set up globals */

    while (get_command("", 0));
    endbbs(0);

    return 0;
}

/******************************************************************************/
/* The following output routines simply call the standard output routines.    */
/* In the Motif version, the w-output routines send output to the windows     */
/******************************************************************************/
void
wputs(const std::string_view &s)
{
    fwrite(s.data(), s.size(), 1, stdout);
}

extern char evalbuf[MAX_LINE_LENGTH];
/* WARNING: the caller is responsible for doing an fflush on the stream
 *          when finished with calls to wfputs and wfputc
 */
void
wfputs(const std::string_view &s, FILE *stream)
{
    if (stream)
        fwrite(s.data(), s.size(), 1, stream);
    else {
        const size_t max = std::min(s.size() + 1, MAX_LINE_LENGTH);
        strlcat(evalbuf, s.data(), max);
    }
}

void
wputchar(int c)
{
    putchar(c);
}

/* WARNING: the caller is responsible for doing an fflush on the stream
 *          when finished with calls to wfputs and wfputc
 */
void
wfputc(int c, FILE *fp)
{
    if (fp)
        fputc(c, fp);
    else {
        size_t len = strlen(evalbuf);
        if (len != MAX_LINE_LENGTH) {
            evalbuf[len] = c;
            evalbuf[len + 1] = 0;
        }
    }
}
