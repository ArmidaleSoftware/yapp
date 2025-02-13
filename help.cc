// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "globals.h"
#include "lib.h"
#include "yapp.h"

/* Index files of help text, in order by mode */
#define HELPINDEX "Index"

/* Number of arguments */
/* Argument list       */
/* Filename of list    */
/* Help display header */
namespace
{
void
show_help(int *count, int argc, char **argv, const std::string &file,
    const std::optional<std::string_view> &hdr)
{
    std::string_view dir, fil;
    bool is_valid;

    /* Set location */
    if (file[0] == '%') {
        dir = bbsdir;
        fil = std::string_view(file).substr(1);
    } else {
        dir = helpdir;
        fil = file;
    }

    /* Is this a text file or a list? */
    const auto headers = grab_file(dir, fil, GF_HEADER);
    if (headers.empty())
        return;
    is_valid = headers[0] == "!<hl01>";

    if (!is_valid) {
        if (*count < argc)
            std::println("Sorry, only this message available.");
        else if (hdr.has_value())
            std::println("****    {}    ****", *hdr);
        if (!more(dir, fil))
            std::println("Can't find help file {}/{}.", dir, fil);
        return;
    }

    /* Read in help list */
    const auto helpvec = grab_list(dir, fil, 0);
    if (helpvec.empty())
        return;
    const assoc_t *help = nullptr;
    const auto *topic = "(none)";
    if (*count < argc) {
        topic = argv[*count];
        help = assoc_list_find(helpvec, topic);
    }

    /* Display requested file */
    if (*count >= argc) {
        /* No arguments, use default file */
        ++*count;
        show_help(count, argc, argv, helpvec[0].location, hdr);
        return;
    }

    ++*count;
    if (help == NULL) {
        std::println("Sorry, no help available for \"{}\"", topic);
        return;
    }

    /* %filename indicates file is in bbsdir not helpdir */
    if (help->location[0] == '%') {
        /* normal help files are in helpdir and get a
         * header displayed */
        show_help(count, argc, argv, help->location, hdr);
    } else {
        auto header = compress(help->name);
        for (auto &c : header) c = toupper(c);
        if (hdr)
            header = std::string(*hdr) + " " + header;

        show_help(count, argc, argv, help->location, header);
    }
}
} // namespace

//*****************************************************************************
// GET HELP ON SOME TOPIC
//*****************************************************************************
// ARGUMENTS:
// Number of arguments
// Argument list
int
help(int argc, char **argv)
{
    auto helpfile = grab_file(helpdir, HELPINDEX, false);
    if (argc < 1 || helpfile.size() <= mode)
        return 1;

    int count = 1;
    do {
        show_help(&count, argc, argv, helpfile[mode], {});
    } while (count < argc);

    return 1;
}
