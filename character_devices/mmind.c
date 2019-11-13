#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>


#include <asm/switch_to.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */


//  check if linux/uaccess.h is required for copy_*_user
//instead of asm/uaccess
//required after linux kernel 4.1+ ?
#ifndef __ASM_ASM_UACCESS_H
    #include <linux/uaccess.h>
#endif

#include "mmind_ioctl.h"

//definitions to be used in module parameters
#define MMIND_MAJOR 0
#define MMIND_MINOR 0
#define MMIND_BUFFER_LENGTH 16
#define MMIND_BUFFER_MAX_LINES 256
#define MMIND_NR_DEVS 1 //how many devices are we going to support?
#define MMIND_MAX_GUESSES 10
#define MMIND_INIT_NUMBER 1234 //defined an initial 4 digit number for mmind_number

int mmind_major = MMIND_MAJOR;
int mmind_minor = MMIND_MINOR;
char* mmind_number;
int mmind_max_guesses = MMIND_MAX_GUESSES;
int mmind_buffer_length = MMIND_BUFFER_LENGTH;
int mmind_buffer_max_lines = MMIND_BUFFER_MAX_LINES;
int mmind_nr_devs = MMIND_NR_DEVS;

module_param(mmind_major, int, S_IRUGO);
module_param(mmind_number, charp, S_IRUGO);
module_param(mmind_max_guesses, int, S_IRUGO);


MODULE_AUTHOR("Sinem Elif Haseki, Hakan Sarac, Refik Ozgun Yesildag");
MODULE_LICENSE("Dual BSD/GPL");
//struct for mmind_dev

struct mmind_dev{
    char **buffer;
    char number[5]; //for making sure that number has 4 digits
    int max_guesses;
    int buf_length;
    int buf_lines; //current line
    struct semaphore sem;
    struct cdev cdev; //for adding our char device to the kernel
};
struct mmind_dev *mmind_devices;

int mmind_open(struct inode *inode, struct file *filp)
{
    struct mmind_dev *dev;

    dev = container_of(inode->i_cdev, struct mmind_dev, cdev);
    filp->private_data = dev;

    
    
    printk(KERN_INFO "Mastermind: Open function\n");
    return 0;
}

int mmind_release(struct inode *inode, struct file *filp)
{
    return 0;
}

int mmind_trim(struct mmind_dev *dev){ //for emptying the memory allocations
    int i;
    if (dev->buffer) {
        for (i = 0; i < dev->buf_lines; i++) {
            if (dev->buffer[i])
                kfree(dev->buffer[i]);
        }
        kfree(dev->buffer);
    }
    dev->buffer = NULL;
    dev->buf_length = mmind_buffer_length;
    dev->buf_lines = 0;
    dev->max_guesses = mmind_max_guesses;
    
    return 0;
}



long mmind_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

	int err = 0;
	int retval = 0;
	int i;
    struct mmind_dev *dev = filp->private_data;
	
	if (_IOC_TYPE(cmd) != MMIND_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > MMIND_IOC_MAXNR) return -ENOTTY;
	
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;


	switch(cmd) {
	  case MMIND_ENDGAME://clears guess history and guess count
        dev->buf_lines = 0;
        dev->buffer=NULL;
		break;

	  case MMIND_NEWGAME: //starts new game with new number
		retval = __get_user(mmind_number, (char* __user *)arg);
        dev->max_guesses = mmind_max_guesses;
        for(i = 0 ; i < 4 ; i++){
			dev->number[i] = mmind_number[i];
		}
		break;

	  case MMIND_REMAINING: //returns the remaining guess amount
        retval = __put_user((dev->max_guesses - dev->buf_lines), (int __user *)arg);
		break;

	  default:  //redundant
		return -ENOTTY;
	}
	return retval;
}

ssize_t mmind_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos) //for reading the buffer in the character device
{
    struct mmind_dev *dev = filp->private_data;
    ssize_t retval = 0;
    int i;
    int line_pos  = (long) *f_pos / 16;//gets the current line we are at
    

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    if (line_pos >= 256) { //if we have reached the end of lines
		goto out;
	}
	if (*f_pos + count > 4096)//initial value of count is 65536 and for eliminating that we subscribe the current fpos from the max possible position (4096)
		count = 4096 - *f_pos;

	
    if (dev->buffer == NULL || ! dev->buffer[line_pos])
        goto out;

   

    for(i=0; i<dev->buf_lines; i++){//copies the buffer to the user space
        if (copy_to_user(buf + i*16, dev->buffer[i],16)) {//we add i*16 so that we can continue moving forward in the buffer for each lines
            retval = -EFAULT;
            goto out;
        }
    }
 
    

    retval += dev->buf_lines * 16;//update the retvalue for the current total size of the used buf
    
	*f_pos += dev->buf_lines * 16; //update the fpos for the current total size of the used buf

  out:
    up(&dev->sem);
    return retval;
}


ssize_t mmind_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos)//writes to character device
{
    struct mmind_dev *dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    int same_index = 0, same_nr_diff_pl = 0;
    char *guessarr = kmalloc(sizeof(char)*5, GFP_KERNEL);
    int i,j, temp3;
    char tsecret[4],tguess[4];
    if(dev->max_guesses > mmind_buffer_max_lines){ //quota of max guesses has been reached
        goto out;
    }


    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if(dev->buf_lines > dev->max_guesses) {
        retval = -EDQUOT;
        goto out;
    }
    if (!dev->buffer) {
        dev->buffer = kmalloc(256 * sizeof(char *), GFP_KERNEL);
        if (!dev->buffer)
            goto out;
        memset(dev->buffer, 0, 256 * sizeof(char *));
    }
    if (!dev->buffer[dev->buf_lines]) {
        dev->buffer[dev->buf_lines] = kmalloc(sizeof(char)*16, GFP_KERNEL);
        if (!dev->buffer[dev->buf_lines])
            goto out;
        memset(dev->buffer[dev->buf_lines], 0, 16 * sizeof(char));
    }
   

    if (copy_from_user(guessarr, buf, 5)) {
        retval = -EFAULT; //if there are bytes that cant be copied
        goto out;
    }
    
//our algorithm compares the secret number to the guessed number as two char arrays, and we compare them digit by digit
//for eliminating counting the same values twice, when we get a hit on the same digits for both numbers, we replace them with 
//x and y chars respectively.
    for(i=0;i<4;i++){
        tsecret[i]=dev->number[i];
        tguess[i]=guessarr[i];
    }
    for(i=0;i<4;i++){
        if(tsecret[i]==tguess[i]){
            same_index++; // will be printed as +
            tsecret[i]='x';
            tguess[i]='y';
        }
    }
    for(i=0;i<4;i++){
        for(j=0;j<4;j++){
            if(i!=j){
                if(tsecret[i]==tguess[j]) {
                    same_nr_diff_pl++;
                    tguess[j]='y';
                    break; //doesn't count the same digit for a digit of the secret number for more than once
                }
            }
        }
    }
    for(i=0;i<4;i++){
        dev->buffer[dev->buf_lines][i]= guessarr[i];
    }
    dev->buffer[dev->buf_lines][4]=' ';
    dev->buffer[dev->buf_lines][5]=same_index+'0';
    dev->buffer[dev->buf_lines][6]='+';
    dev->buffer[dev->buf_lines][7]=' ';
    dev->buffer[dev->buf_lines][8]=same_nr_diff_pl+'0';
    dev->buffer[dev->buf_lines][9]='-';
    dev->buffer[dev->buf_lines][10]=' ';
    temp3 = dev->buf_lines + 1;
    for (i=4; i>=1; i--){
        dev->buffer[dev->buf_lines][10+i]= temp3 % 10+'0'; //get the digit
        temp3 /= 10; //and divide it by 10 to get the other digit
    }
    dev->buffer[dev->buf_lines][15]='\n';
    //update current line number
    
    
    if(dev->buf_lines<dev->max_guesses) {
        dev->buf_lines++;
    }
    else{
        retval = -EDQUOT; //quota of max guesses has been reached
        goto out;
    }
 
    retval = count;

  out:
    up(&dev->sem);
    kfree(guessarr);
    return retval;
}



struct file_operations mmind_fops = {
    .owner =    THIS_MODULE,
    .read =     mmind_read,
    .write =    mmind_write,
    .unlocked_ioctl =  mmind_ioctl,
    .open =     mmind_open,
    .release =  mmind_release,
};

void mmind_cleanup_module(void)
{
    int i;
    dev_t devno = MKDEV(mmind_major, mmind_minor);

    if (mmind_devices) {
        for (i = 0; i < mmind_nr_devs; i++) {
            mmind_trim(mmind_devices + i);
            cdev_del(&mmind_devices[i].cdev);
        }
    kfree(mmind_devices);
    }

    unregister_chrdev_region(devno, mmind_nr_devs);
}

int mmind_init_module(void){//init module
    int result, i, err, num;
    dev_t devno;
    struct mmind_dev *dev;


    i = 0;
    devno = 0;
    num = 0;

    if (mmind_major){ //dynamic selection
        devno = MKDEV(mmind_major, mmind_minor);
        result = register_chrdev_region(devno, 1, "mmind");
    }
    else{
        result = alloc_chrdev_region(&devno, mmind_minor, 1,"mmind");
        mmind_major = MAJOR(devno);
    }
    if (result < 0) {
        printk(KERN_WARNING "mmind: can't get major %d\n", mmind_major);
        return result;
    }

    mmind_devices = kmalloc(mmind_nr_devs * sizeof(struct mmind_dev),GFP_KERNEL);
    if (!mmind_devices) {
        result = -ENOMEM;
        goto fail;
    }
    memset(mmind_devices, 0, mmind_nr_devs * sizeof(struct mmind_dev));

        dev = mmind_devices;
        dev->buf_length = mmind_buffer_length;
        dev->buf_lines = 0;
        dev->buffer=NULL;
        dev->max_guesses = mmind_max_guesses;
		
		for (i=0; i<4 ; i++){
			dev->number[i] = mmind_number[i];
		}
        sema_init(&dev->sem,1);
        devno = MKDEV(mmind_major, mmind_minor);
        cdev_init(&dev->cdev, &mmind_fops);
        dev->cdev.owner = THIS_MODULE;
        dev->cdev.ops = &mmind_fops;
        err = cdev_add(&dev->cdev, devno, 1);
        if (err)
            printk(KERN_NOTICE "Error %d adding mmind", err);
    
    return 0; 

   fail:
    mmind_cleanup_module();
    return result;
}



module_init(mmind_init_module);
module_exit(mmind_cleanup_module);
