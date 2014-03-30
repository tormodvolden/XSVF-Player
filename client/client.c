#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include "../common/xsvf.h"

int setup_stream(const char *devname)
{
	int serial_fd;

	struct termios tio;
        memset(&tio,0,sizeof(tio));
        tio.c_iflag=0;
        tio.c_oflag=0;
        tio.c_cflag=CS8|CREAD|CLOCAL;	// 8n1
        tio.c_lflag=0;
        tio.c_cc[VMIN]=1;
        tio.c_cc[VTIME]=5;
        cfsetospeed(&tio,B57600);	// bauds
	cfsetispeed(&tio,B57600);

	serial_fd = open(devname, O_RDWR);

	if (serial_fd >= 0)
		tcsetattr(serial_fd, TCSANOW, &tio);
	return serial_fd;
}

void print_line(int serial_fd)
{
	char c;
	do {
		read(serial_fd,&c,1);
//		c = fgetc(stdin);
		printf("%c",c);
	} while (c!='\n');
}

int command_plus(int serial_fd, FILE *xsvf_file)
{
	static unsigned int total = 0;
	char buffer[0x100];
	int size;

	size = load_next_instr(buffer, xsvf_file);

	// end of file or XCOMPLETE command => stop
	if (size==0) {
		printf("\nreached end of file.\n");
		exit(2);
	}
	if (buffer[0]==0) {
		printf("\nXCOMPLETE command.\n");
		return 1;
	}

	// else send it
	write(serial_fd, buffer, size);

	total += size;
	printf("\r%08x bytes",total);
	fflush(stdout);
	return 0;
}

int data_ready(int serial_fd)
{
	size_t nbytes;
        if ( ioctl(serial_fd, FIONREAD, (char*)&nbytes) < 0 )  {
                fprintf(stderr, "%s - failed to get byte count.\n", __func__);
                return -1;
        }
        return ((int)nbytes);

}

int process_command(char c, int serial_fd)
{
	switch (c) {
	case '+': return 1;
	case '-': printf("process failed\n"); exit(1);
	case 'd': printf("DEBUG  : "); print_line(serial_fd); break;
	case 'i': printf("INFO   : "); print_line(serial_fd); break;
	case 'w': printf("WARNING: "); print_line(serial_fd); break;
	case 'e': printf("ERROR  : "); print_line(serial_fd); break;
	}
	return 0;
}

int main()
{
	int serial_fd;
	FILE *xsvf_file;

	serial_fd = setup_stream("/dev(ttyUSB1");
	if (serial_fd < 0) {
		exit(1);
	}

	xsvf_file = fopen("cram.xsvf","rb");

	while (1) {
		int n;
		char c;
		while (data_ready(serial_fd) > 0) {
			read(serial_fd, &c, 1);
			if (process_command(c, serial_fd) > 0) {
				goto sync_ok;
			}
		}
		printf("waiting for sync...\n");
		usleep(500000);
		c = 0;
		write(serial_fd, &c, 1);
		usleep(500000);
	}
sync_ok:
	printf("sync ok.\n");

	while (1) {
		char c;
		if (command_plus(serial_fd, xsvf_file)) {
			break;
		}
		do {
			read(serial_fd, &c, 1);
		} while (process_command(c, serial_fd) <= 0);
	}
	return 0;
}
