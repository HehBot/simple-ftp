# simple-ftp
Polished version of a computer networks lab assignment

## Usage
Compile

    $ make -j

Server
To start server on port `9000`

    $ ftp-server 9000

Client
To get file `serverfile` from server at `127.0.0.1:9000`

    $ ftp-client 127.0.0.1:9000 get serverfile

To put file `clientfile` onto server at `127.0.0.1:9000`

    $ ftp-client 127.0.0.1:9000 put path/to/clientfile
