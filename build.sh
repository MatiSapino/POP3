cmake -S . -B bin/
cd bin
make pop3server

#removing maildir
rm -rf ~/maildir