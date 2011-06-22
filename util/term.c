#include <termios.h>
#include <unistd.h>

void term_canon(int fd, int on)
{
	struct termios attr;

	if(tcgetattr(fd, &attr) == -1)
		return;

	if(on){
		attr.c_lflag    |=  (ISIG|ICANON|ECHO);
		attr.c_cc[VMIN]  = 0;
		attr.c_cc[VTIME] = 0;
	}else{
		attr.c_lflag    &= ~(ISIG|ICANON|ECHO);
		attr.c_cc[VMIN]  = 1;
		attr.c_cc[VTIME] = 2;
	}

	tcsetattr(fd, TCSANOW, &attr);
}
