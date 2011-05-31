#ifndef IO_H
#define IO_H

int fgetline( char **line, FILE *, char *haseol);
/*int fdgetline(char **s, char **buffer, int fd);*/
void chomp_line(void);

#endif
