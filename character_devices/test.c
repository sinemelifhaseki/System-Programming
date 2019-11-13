#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>


#include "mmind_ioctl.h"

int main()
{
	int temp,fd, retval,i;
	char secretNumber[5];
	int remaining_guesses;
	char* mmind_num;
	fd = open("/dev/mmind", O_RDWR);
	
	if (fd == -1) {
		printf("Device could not open!");
	}
	
	printf("1 : REMAINING\n");
	printf("2 : END GAME\n");
	printf("3 : NEW GAME\n");

	printf("Make Your Choice:\n");
	int choice;
	scanf("%d", &choice);
	
	if(choice == 1){
		retval = ioctl(fd, MMIND_REMAINING, &remaining_guesses);
		printf("Remaining guesses = %d \n", remaining_guesses);
	}
	else if (choice == 2){
		retval = ioctl(fd, MMIND_ENDGAME);
		
	}
	else if (choice == 3){
		printf("Enter a new secret number: ");
		scanf("%d", &temp);
		mmind_num = (char*)malloc(sizeof(char)*5);
		mmind_num[4]='\0';
		for(i=3;i>=0;i--){
			mmind_num[i] = temp % 10 + '0';
			temp /= 10;
		}
		retval = ioctl(fd, MMIND_NEWGAME, &mmind_num);
	}
	else{
		printf("You have entered an invalid operation.\n");
	}
	close(fd);
	return 0;
}
