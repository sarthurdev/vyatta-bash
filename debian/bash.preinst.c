/*
 * This file is in the public domain.
 * You may freely use, modify, distribute, and relicense it.
 */

#include "bash.preinst.h"
#include <errno.h>
#include <unistd.h>

static void backup(const char *file, const char *dest)
{
	const char * const cmd[] = {"cp", "-dp", file, dest, NULL};
	if (exists(file))
		run(cmd);
}

static void force_symlink(const char *target, const char *link,
						const char *temp)
{
	/*
	 * Forcibly create a symlink to "target" from "link".
	 * This is performed in two stages with an
	 * intermediate temporary file because symlink(2) cannot
	 * atomically replace an existing file.
	 */
	if ((unlink(temp) && errno != ENOENT) ||
	    symlink(target, temp) ||
	    rename(temp, link))
		die_errno("cannot create symlink %s -> %s", link, target);
}

int main(int argc, char *argv[])
{
	/* /bin/sh needs to point to a valid target. */

	if (access("/bin/sh", X_OK)) {
		backup("/bin/sh", "/bin/sh.distrib");
		backup("/usr/share/man/man1/sh.1.gz",
			"/usr/share/man/man1/sh.distrib.1.gz");

		force_symlink("bash", "/bin/sh", "/bin/sh.temp");
		force_symlink("bash.1.gz", "/usr/share/man/man1/sh.1.gz",
			"/usr/share/man/man1/sh.1.gz.temp");
	}
	return 0;
}
