#!/bin/sh
# this creates a complementary message configuration in terms of receiving and sending
sed 's/send:/xsend:/g' $1 | sed 's/receive:/xreceive:/g' | sed 's/xsend:/receive:/g' | sed 's/xreceive:/send:/g'
