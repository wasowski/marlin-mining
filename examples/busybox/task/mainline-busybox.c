#if defined(ANDROID) || defined(__ANDROID__)
# define endgrent() ((void)0)
#endif

#if ENABLE_SELINUX
# include <selinux/selinux.h>
# include <selinux/context.h>
# include <selinux/flask.h>
# include <selinux/av_permissions.h>
#endif

extern off_t bb_copyfd_eof(int fd1, int fd2) FAST_FUNC;
extern off_t bb_copyfd_size(int fd1, int fd2, off_t size) FAST_FUNC;
extern void bb_copyfd_exact_size(int fd1, int fd2, off_t size) FAST_FUNC;
extern void complain_copyfd_and_die(off_t sz) NORETURN FAST_FUNC;

#define OFF_T_MAX  ((off_t)~((off_t)1 << (sizeof(off_t)*8-1)))
struct BUG_off_t_size_is_misdetected {
	char BUG_off_t_size_is_misdetected[sizeof(off_t) == sizeof(uoff_t) ? 1 : -1];
};

#define LIBBB_DEFAULT_LOGIN_SHELL  "-/bin/sh"

putenv((char *) "SHELL=/bin/sh");
