all: bin/ftp-client bin/ftp-server

clean:
	$(RM) bin/ftp-client bin/ftp-server

bin/%: %.c
	@mkdir -p $(dir $@)
	$(CC) -g -o $@ $< -lpthread
