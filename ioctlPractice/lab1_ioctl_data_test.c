
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

struct my_data {
	char s[256];
} my_data;

#define MYIOC_TYPE 'k'

int main(int argc, char *argv[])
{
	int fd, rc;
	int MY_IOCTL;
	char *nodename = "/dev/mycdrv";

	/* open the device node */

	if (argc > 1)
		nodename = argv[1];
	fd = open(nodename, O_RDWR);
	printf(" I opened the device node, file descriptor = %d\n", fd);

	/* fill up the data structure */
	 
	char * sosString = ".s.s.l-s-s-l.s.s.";
	strcpy(my_data.s, sosString);
	printf("\n Sending from user-space:\n");
	printf("    my_data.s = %s\n", my_data.s); 

	/* send to the device */

	MY_IOCTL = (int)_IOW(MYIOC_TYPE, 1, struct my_data);
	rc = ioctl(fd, MY_IOCTL, &my_data);
	printf("\n rc from ioctl = %d \n\n", rc);

	close(fd);
	exit(0);

}
