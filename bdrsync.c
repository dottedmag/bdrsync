/*
 * bdrsync - block devices synchronization tool.
 * © 2009 Mikhail Gusarov <dottedmag@dottedmag.net>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _FILE_OFFSET_BITS 1
#define _BSD_SOURCE 1
#define _GNU_SOURCE 1

#include <errno.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define BDRSYNC_NAME "bdrsync"
#define BDRSYNC_VERSION "0.1"

/*
 * Sigh.
 */
#ifndef BLKGETSIZE64
#define BLKBSZGET  _IOR(0x12,112,size_t)
#define BLKGETSIZE64 _IOR(0x12,114,size_t)
#endif

static struct option longopts[] = {
    { "help", no_argument, NULL, 'h' },
    { "verbose", no_argument, NULL, 'v' },
    { "version", no_argument, NULL, 'V' },
    { NULL, 0, NULL, 0 }
};

void usage()
{
    printf("Usage: " BDRSYNC_NAME " [OPTIONS] BLKDEV1 BLKDEV2\n");
    printf("Efficiently copy contents of BLKDEV1 to BLKDEV2.\n");
    printf("\n");
    printf(" -v, --verbose         explain what is being done\n");
    printf(" -V, --version         output verison information and exit\n");
    printf(" -h, --help            display this help and exit\n");
}

void version()
{
    printf(BDRSYNC_NAME " " BDRSYNC_VERSION "\n");
    printf("© 2009 Mikhail Gusarov <dottedmag@dottedmag.net>\n");
    printf("\n");
    printf("License GPLv2+: GNU GPL version 2 or later "
           "<http://gnu.org/licenses/gpl.html>\n");
    printf("This is free software: you are free to change and redistribute it.\n");
    printf("There is NO WARRANTY, to the extent permitted by law.\n");
}

int gcd(int a, int b)
{
    while(a && b)
        if(a >= b)
            a = a % b;
        else
            b = b % a;
   return a + b;
}

int lcs(int a, int b)
{
    return a / gcd(a, b) * b;
}

void check_get_blkdev(int fd, const char* name, struct stat* out)
{
    if(fstat(fd, out))
    {
        perror(name);
        exit(EXIT_FAILURE);
    }

    if(!S_ISBLK(out->st_mode))
    {
        fprintf(stderr, "%s: is not a block device\n", name);
        exit(EXIT_FAILURE);
    }
}

#if 0
void check_subdevice(const char* name1, struct stat* stat1,
                     const char* name2, struct stat* stat2)
{
    if(major(stat1->st_rdev) == major(stat2->st_rdev)
       && minor(stat1->st_rdev) == 0)
    {
        fprintf(stderr, "%s is a part of %s\n", name2, name1);
        exit(EXIT_FAILURE);
    }
}
#endif

long long get_size(const char* name, int fd)
{
    long long res;
    int err = ioctl(fd, BLKGETSIZE64, &res);
    if(err == -1)
    {
        perror(name);
        exit(EXIT_FAILURE);
    }
    return res;
}

int get_block_size(const char* name, int fd)
{
    int res;
    int err = ioctl(fd, BLKBSZGET, &res);
    if(err == -1)
    {
        perror(name);
        exit(EXIT_FAILURE);
    }
    return res;
}

int lread(int fh, char* buf, size_t count)
{
    while(count)
    {
        int res = read(fh, buf, count);
        if(res == -1 && (errno == EINTR || errno == EAGAIN))
            continue;
        if(res > 0)
        {
            buf += res;
            count -= res;
            continue;
        }

        return -1;
    }
    return 0;
}

int lwrite(int fh, const char* buf, size_t count)
{
    while(count)
    {
        int res = write(fh, buf, count);
        if(res == -1 && (errno == EINTR || errno == EAGAIN))
            continue;
        if(res > 0)
        {
            buf += res;
            count -= res;
            continue;
        }

        return -1;
    }
    return 0;
}

/*
 * Returns 0 if block does not need to be sync'ed.
 */
int syncblock(const char* name1, int fd1, char* buffer1,
              const char* name2, int fd2, char* buffer2,
              int blocksize)
{
    if(-1 == lread(fd1, buffer1, blocksize))
    {
        perror(name1);
        exit(EXIT_FAILURE);
    }
    if(-1 == lread(fd2, buffer2, blocksize))
    {
        perror(name2);
        exit(EXIT_FAILURE);
    }

    if(!memcmp(buffer1, buffer2, blocksize))
        return 0;

    if(-1 == lseek(fd2, -blocksize, SEEK_CUR))
    {
        perror(name2);
        exit(EXIT_FAILURE);
    }

    if(-1 == lwrite(fd2, buffer1, blocksize))
    {
        perror(name2);
        exit(EXIT_FAILURE);
    }

    return 1;
}

void syncdev(const char* name1, int fd1, long long size1,
             const char* name2, int fd2, long long size2)
{
    int blocksize1 = get_block_size(name1, fd1);
    int blocksize2 = get_block_size(name2, fd2);

    int blocksize = lcs(blocksize1, blocksize2);

    char* buffer1 = malloc(blocksize);
    char* buffer2 = malloc(blocksize);

    long long count = size1/blocksize;
    long long i;

    fprintf(stderr, "%lld %lld %d\n", size1, count, blocksize);

    for(i = 0; i != count; ++i)
    {
        if(!(i%80))
            putc('\n', stderr);

        syncblock(name1, fd1, buffer1, name2, fd2, buffer2, blocksize);
    }

    if(count * blocksize != size1)
    {
        int tail = size1 - count*blocksize;
        syncblock(name1, fd1, buffer1, name2, fd2, buffer2, tail);
    }

    putc('\n', stderr);

    free(buffer1);
    free(buffer2);
}

int main(int argc, char** argv)
{
    int verbosity = 0;

    int r;
    while((r = getopt_long(argc, argv, "hvV", longopts, NULL)) != -1)
        switch(r)
        {
        case 'h':
            usage();
            exit(EXIT_SUCCESS);
        case 'v':
            verbosity++;
            break;
        case 'V':
            version();
            exit(EXIT_SUCCESS);
        case '?':
            usage();
            exit(EXIT_FAILURE);
        }

    if(argc != optind+2)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    char* first_device = argv[optind];
    char* second_device = argv[optind+1];

    int first_fd = open(first_device, O_NOATIME | O_RDONLY);
    if(first_fd == -1)
    {
        perror(first_device);
        exit(EXIT_FAILURE);
    }
    int second_fd = open(second_device, O_NOATIME | O_RDWR);
    if(second_fd == -1)
    {
        perror(second_device);
        exit(EXIT_FAILURE);
    }

    struct stat d1_stat;
    struct stat d2_stat;
    check_get_blkdev(first_fd, first_device, &d1_stat);
    check_get_blkdev(second_fd, second_device, &d2_stat);

    if(major(d1_stat.st_rdev) == major(d2_stat.st_rdev)
       && minor(d1_stat.st_rdev) == minor(d2_stat.st_rdev))
    {
        fprintf(stderr, "%s and %s are the same device\n",
                first_device, second_device);
        exit(EXIT_FAILURE);
    }

    long long d1_size = get_size(first_device, first_fd);
    long long d2_size = get_size(second_device, second_fd);

    if (d2_size < d1_size)
    {
        fprintf(stderr, "target device %s is smaller than source %s",
                second_device, first_device);
        exit(EXIT_FAILURE);
    }

    syncdev(first_device, first_fd, d1_size,
            second_device, second_fd, d2_size);

    return 0;
}
