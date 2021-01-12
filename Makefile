# NAME: Khoa Quach
# EMAIL: khoaquachschool@gmail.com
# ID: 105123806
CC = gcc
CFLAGS = -Wall -Wextra
.SILENT:

default:
	$(CC) $(CFLAGS) -o lab1a lab1a.c
clean:
	rm -f lab1a *.tar.gz *.o *~
dist: 
	tar -czvf lab1a-105123806.tar.gz lab1a.c Makefile README
