#ifndef C99CC_ERRNO_H
#define C99CC_ERRNO_H

int* __c99cc_errno(void);
#define errno (*__c99cc_errno())

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOENT
#define ENOENT 2
#endif
#ifndef EIO
#define EIO 5
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif
#ifndef EACCES
#define EACCES 13
#endif

#endif
