all: test/ftp-client test/ftp-server

test/%: %.c
	$(CC) -o $@ $<
