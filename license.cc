// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * This module is the license server.  There is a license/ directory
 * under the BBSDIR which contains files which we lock.
 */

#include "license.h"

#include <pwd.h>
#include <unistd.h>
#include <vector>

#include "lib.h"
#include "struct.h"
#include "yapp.h"

#define CFADM "cfadm"
#define UNLIMITED -1
#define LIC_TIME_LEEWAY 300 /* Allow 5 minute difference between saved */
                            /* timestamp, and file timestamp           */

/* Allow 1 week difference between saved timestamp, and file timestamp */
#define INITIAL_LIC_TIME_LEEWAY (7 * 24 * 60 * 60)

const char *regto = "Registered to: Arbornet";

/*****************************/
/* Routines to configure Yapp according to /etc/yapp.conf
 * if it exists (otherwise try /usr/local/etc/yapp.conf, then ~/yapp.conf,
 * and finally ./yapp.conf).
 */
static std::vector<assoc_t> conf_params;

static void
read_config2(const std::string &filename)
{
    conf_params = grab_list("/etc", filename, GF_SILENT | GF_NOHEADER);
    if (conf_params.empty())
        conf_params =
            grab_list("/usr/local/etc", filename, GF_SILENT | GF_NOHEADER);
    if (conf_params.empty())
        conf_params =
            grab_list("/arbornet/m-net/bbs", filename, GF_SILENT | GF_NOHEADER);
    if (conf_params.empty()) {
        struct passwd *pwd = getpwuid(geteuid());
        if (pwd != nullptr)
            conf_params =
                grab_list(pwd->pw_dir, filename, GF_SILENT | GF_NOHEADER);
    }
    if (conf_params.empty())
        conf_params = grab_list(".", filename, GF_SILENT | GF_NOHEADER);
}

void
read_config(void)
{
    read_config2("yapp3.1.conf");
    if (conf_params.empty())
        read_config2("yapp.conf");
}

std::string
get_conf_param(const std::string_view &name, const std::string_view &def)
{
    if (conf_params.empty())
        return std::string(def);
    auto i = get_idx(name, conf_params);
    if (i == ~0z)
        return std::string(def);
    return conf_params[i].location;
}

void
free_config(void)
{
    conf_params.clear();
}

int
get_hits_today(void)
{
    const auto dir = get_conf_param("licensedir", LICENSEDIR);
    const auto file = grab_file(dir, "registered", GF_NOHEADER);
    if (file.size() < 3)
        return 0;
    return std::stoi(file[2]);
}
