#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <sched.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syscall.h>

static inline void cleanup_fdp (int *fdp)
{
  int fd;

  assert (fdp);

  fd = *fdp;
  if (fd != -1)
    (void) close (fd);
}
#define cleanup_fd __attribute__((cleanup (cleanup_fdp)))

static int pivot_root (const char * new_root, const char * put_old)
{
	return syscall (SYS_pivot_root, new_root, put_old);
}

__attribute__((format(printf, 1, 0))) static void
warnv (const char *format,
       va_list args,
       const char *detail)
{
	fprintf (stderr, "sheath: ");
	vfprintf (stderr, format, args);

	if (detail != NULL)
		fprintf (stderr, ": %s", detail);

	fprintf (stderr, "\n");
}

static void error (const char *format, ...)
{
	va_list args;
	int errsv;

	errsv = errno;

	va_start (args, format);
	warnv (format, args, strerror (errsv));
	va_end (args);
}

static void exit_with_error (const char *format, ...)
{
	va_list args;
	int errsv;

	errsv = errno;

	va_start (args, format);
	warnv (format, args, strerror (errsv));
	va_end (args);

	exit (1);
}

static void exit_(const char *format, ...)
{
	va_list args;

	va_start (args, format);
	warnv (format, args, NULL);
	va_end (args);

	exit (1);
}

#define FILES "/data/data/com.termux/files"
#define TMP FILES "/tmp"
#define VAT FILES "/home/vat"
#define HAT VAT "/hat"
#define NIX VAT "/nix"

static void setup_mounts()
{
	if (mount (NULL, "/", NULL, MS_SILENT | MS_SLAVE | MS_REC, NULL) < 0)
		exit_with_error("Failed to make / slave");

	if (mkdir(TMP, 0755) && errno != EEXIST)
		exit_with_error("mkdir %s", TMP);

	if (mount ("tmpfs", TMP, "tmpfs", MS_SILENT, NULL) != 0)
		exit_with_error("Failed to mount tmpfs");

	if (chdir(TMP))
		exit_with_error("chdir %s: %d");

	if (mkdir("newroot", 0755) && errno != EEXIST)
		exit_with_error("mkdir newroot");

	if (mount ("newroot", "newroot", NULL, MS_SILENT | MS_MGC_VAL | MS_BIND | MS_REC, NULL) < 0)
		exit_with_error ("setting up newroot bind");

	if (mkdir("oldroot", 0755) && errno != EEXIST)
		exit_with_error("mkdir oldroot");

	if (pivot_root(TMP, "oldroot"))
		exit_with_error("pivot_root");

	char old_paths[][256] = {
				  "/oldroot",
				  "/oldroot" HAT,
				  "/oldroot" NIX,
				};
	char new_paths[][256] = {
				  "/newroot/mnt",
				  "/newroot/hat",
				  "/newroot/nix",
				};

	for (size_t i = 0; i < sizeof(old_paths) / sizeof(old_paths[0]); i++) {
		if (mkdir(new_paths[i], 0755) && errno != EEXIST)
			exit_with_error("mkdir %s", new_paths[i]);
		if (mount(old_paths[i], new_paths[i], NULL, MS_SILENT | MS_BIND | MS_REC, NULL) && errno != ENOENT)
			exit_with_error("mount %s to %s", old_paths[i], new_paths[i]);
	}

	/* The old root better be rprivate or we will send unmount events to the parent namespace */
	if (mount ("oldroot", "oldroot", NULL, MS_SILENT | MS_REC | MS_PRIVATE, NULL) != 0)
		exit_with_error ("Failed to make old root rprivate");
	if (umount2 ("oldroot", MNT_DETACH))
		exit_with_error ("unmount old root");

	cleanup_fd int oldrootfd = open ("/", O_DIRECTORY | O_RDONLY);
	if (oldrootfd < 0)
		exit_with_error("cannot open /");
	if (chdir("/newroot"))
		exit_with_error("chdir /newroot");

	if (pivot_root(".", "."))
		exit_with_error("pivot_root(newroot)");
	if (fchdir(oldrootfd))
		exit_with_error("fchdir(oldrootfd)");
	if (umount2(".", MNT_DETACH))
		exit_with_error("umount old root");
	if (chdir("/"))
		exit_with_error("chdir /");
}

int main(int argc, char *argv[]){

	if (unshare(CLONE_NEWNS))
		exit_with_error("unshare");

	setup_mounts();

	if (argc <= 1) {
		if (execl("/hat/simulate", "/hat/simulate", (char *) NULL))
	    		exit_with_error("execl");
	} else {
		if (execvp(argv[1], argv + 1))
			exit_with_error("execvp");
	}

	return 0;
}
