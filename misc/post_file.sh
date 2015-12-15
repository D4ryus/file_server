#!/bin/bash
# simple script to create a http post with the given file using curl
# if xclip is installed the link is copied to clipboard

# server is the address curl will post to
SERVER="http://d4ryus.h4ck.me:8283/upload"
# FILE contains the filename, just in case a path is specified
FILE=$(basename "$1")

if [ ! type "curl" &> /dev/null ]; then
        echo "cannot execute 'curl'!"
        exit 1
fi

# check if a name is given, if not exit
if [ "${FILE}" == "" ]; then
        echo "please specify a file"
        exit 1
fi

response=$(curl --form "fileupload=@$1" ${SERVER})
echo ""
if [[ $response == 405* ]] ; then
        echo "could not upload. error 405 Method not Allowed."
        exit 1
fi
# check if xclip is installed, if so copy link to clipboard
echo "${SERVER}/${IMAGE}"
if type "xclip" &> /dev/null; then
        echo "${SERVER}/${FILE}" | xclip -in
        echo "copied to clipboard, paste with 'xclip -out' or middle mouse."
fi
