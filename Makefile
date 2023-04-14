all: ftp-client ftp-server

%: %.c
	$(CC) -o test/$@ $<
