// This file is part of the eix project and distributed under the
// terms of the GNU General Public License v2.
//
// Copyright (c)
//   Wolfgang Frisch <xororand@users.sourceforge.net>
//   Emil Beinroth <emilbeinroth@gmx.net>
//   Martin V�th <vaeth@mathematik.uni-wuerzburg.de>

#ifndef __SYSUTILS_H__
#define __SYSUTILS_H__

class ExBasic;

#include <unistd.h>
#include <time.h>

/** Return false if the file is not writable/readable by users in the group portage. */
bool is_writable(const char *file);

bool is_dir(const char *file);

/** Return true if the current user is in the group_name. */
bool user_in_group(const char *group_name);

/** Get uid of a user.
 * @param u pointer to uid_t .. uid is stored there.
 * @param name name of user
 * @return true if user exists */
bool get_uid_of(const char *name, uid_t *u);

/** Get gid of a group.
 * @param g pointer to gid_t .. gid is stored there.
 * @param name name of group
 * @return true if group exists */
bool get_gid_of(const char *name, gid_t *g);

/** Return mtime of file. */
time_t get_mtime(const char *file);

/** @return mydate formatted according to locales and dateFormat */
const char *date_conv(const char *dateFormat, time_t mydate);

#endif /* __SYSUTILS_H__ */
