#ifndef __MMIND_H
#define __MMIND_H

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define MMIND_IOC_MAGIC  'k'
#define MMIND_REMAINING    _IOR(MMIND_IOC_MAGIC, 0, int)
#define MMIND_ENDGAME    _IO(MMIND_IOC_MAGIC,  1) 
#define MMIND_NEWGAME    _IOW(MMIND_IOC_MAGIC,  2, char*)
#define MMIND_IOC_MAXNR 2

#endif