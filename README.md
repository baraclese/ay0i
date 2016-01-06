# ay0i
A self-replicating and mutating linux process.

## How it works
- Reads its own executable file
- Writes out the bytes of the executable to a random filename possibly inserting mutations
- Forks and execs the new executable

## Mutations
There are three types of mutations that can occur:
- Insertion of random byte
- Substituting existing byte with random byte
- Deletion of a byte

## Running
1. Create a new user.
2. Create empty file of 128MB size (dd if=/dev/zero of=myfile bs=1M count=128).
3. Format this file with a filesystem, i.e. mkfs.ext4 myfile.
4. Mount this file as the home directory of the new user (mount -t ext4 -o loop myfile /home/newuser)
5. Set limits for newuser, in Ubuntu this can be done by creating the file /etc/security/limits.d/myuser.conf with the following example content:

    ```
newuser hard nice 19
newuser hard priority 100
newuser hard cpu 1
newuser hard nproc 300
    ```

    These settings help your computer to stay responsive during the experiment. If you switch to newuser via the su command then you also need to add the following line to the file /etc/pam.d/su:

    ```
    session required pam_limits.so
    ```

6. Compile the program.
7. Run program under the new user account.

## Risks
Anything could happen, but what generally happens is that (without user limits) it behaves like a forkbomb so that your system will become unresponsive and you may need to reboot your computer. It will also fill the disk with random files of itself.

## System requirements
- x86-64 processor and linux environment
- kernel >= 3.17 for the getrandom syscall

## TODO
Write a daemon or a cronjob that kills long-running processes of the executing user. This will regulate the population of the processes.

