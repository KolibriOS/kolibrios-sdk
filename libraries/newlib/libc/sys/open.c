/* open.c -- open a file.
 *
 * Copyright (c) 1995 Cygnus Support
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ksys.h>

#include "glue.h"
#include "io.h"

#undef errno
extern int errno;

extern int write_file(const char *path,const void *buff,
                      size_t offset, size_t count, size_t *writes);

extern int read_file(const char *path, void *buff,
               size_t offset, size_t count, size_t *reads);

static inline int is_slash(char c)
{
    return c=='/' || c=='\\';
}

void fix_slashes(char * in,char * out)
{
    int slash_count;

    for(slash_count=1;in && out && *in;in++)
    {
        if(is_slash(*in))
        {
            slash_count++;
            continue;
        }
        else
        {
            if(slash_count)
            {
                slash_count=0;
                *out++='/';
            }
            *out++=*in;
        }
    }
    *out='\0';
}


void buildpath(char *buf, const char* file)
{
    char *ptr;

    ptr = buf + strlen(buf);

    while (*file)
    {
        if (file[0] == '.' && file[1] == 0)
            break;

        if (file[0] == '.' && file[1] == '/')
        {
            file+=2;
            continue;
        };

        if (file[0] == '.' && file[1] == '.' &&
            (file[2] == 0 || file[2] == '/'))
        {
            while (ptr > buf && ptr[-1] != '/')
                --ptr;
            file+=2;
            if (*file == 0)
                break;
            ++file;
            //continue;
            goto __do_until_slash;
        }
        *ptr++ = '/';
__do_until_slash:
        if (*file == '/')
            ++file;
        while (*file && *file!='/')
            *ptr++ = *file++;
    }
    *ptr = 0;
}

int open (const char * filename, int flags, ...)
{
    char buf[1024];

    __io_handle *ioh;
    ksys_file_info_t info;
    int iomode, rwmode, offset;
    int hid;
    int err;

    hid = __io_alloc();
    if(hid < 0)
    {
        errno = EMFILE;
        __io_free(hid);
        return (-1);
    };

    if (filename[0]=='/')
    {
        strcpy(buf,filename);
    }
    else
    {
        _ksys_getcwd(buf, 1024);
        buildpath(buf, filename);
    }

    err = _ksys_file_info(buf, &info);

    if (flags & O_EXCL &&
        flags & O_CREAT )
    {
        if (!err)
        {
            errno = EEXIST;
            __io_free(hid);
            return (-1);
        }
    }

    if (err)
    {
        if(flags & O_CREAT)
            err = _ksys_file_create(buf).status;
        if(err)
        {
            errno = EACCES;
            __io_free(hid);
            return -1;
        };
    };

    if (flags & O_TRUNC)
        _ksys_file_set_size(buf, 0);

    ioh = &__io_tab[hid];

    rwmode = flags & ( O_RDONLY | O_WRONLY | O_RDWR );

    iomode = 0;
    offset = 0;

    if (rwmode == O_RDWR)
        iomode |= _READ | _WRITE;
    else if (rwmode == O_RDONLY)
        iomode |= _READ;
    else if (rwmode == O_WRONLY)
        iomode |= _WRITE;

    if (flags & O_APPEND)
    {
        iomode |= _APPEND;
        offset = info.size;
    }

    if (flags & (O_BINARY|O_TEXT))
    {
        if (flags & O_BINARY)
            iomode |= _BINARY;
    }
    else
    {
        iomode |= _BINARY;
    }

    ioh->name   = strdup(buf);
    ioh->offset = offset;
    ioh->mode   = iomode;
    ioh->read   = read_file;
    ioh->write  = write_file;

    return hid;
}
