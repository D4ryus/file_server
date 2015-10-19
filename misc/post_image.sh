#!/bin/bash
# simple script to create a http post with the given image using curl and
# imagemagick, if xclip is installed the link is copied to clipboard

# server is the address curl will post to
SERVER="http://d4ryus.h4ck.me:8283/"
# SERVER_UPLOAD_DIR is the directoy on the server where the image is
# getting saved
SERVER_UPLOAD_DIR="upload"
# TMP is a local directoy where the images are getting saved
TMP="/tmp/upload"
IMAGE="$1"

if [ ! type "curl" &> /dev/null ]; then
        echo "cannot execute 'curl'!"
        exit 1
fi
if [ ! type "import" &> /dev/null ]; then
        echo "cannot execute 'import'! its part of ImageMagick"
        exit 1
fi

# check if a name is given, if not generate one
if [ "${IMAGE}" == "" ]; then
        IMAGE="ss_`date +%F:%T | md5sum - | dd bs=1 count=5 status=none`.jpg"
fi

# set the full path to image
FULL_PATH="${TMP}/${IMAGE}"

# check if directoy exists, if not create it
if [ ! -d "${TMP}" ]; then
        mkdir "${TMP}"
fi

# take a screenshot
import ${FULL_PATH}

# check if something went wrong, if not upload it via curl
if [ -r ${FULL_PATH} ]; then
        response=$(curl --form "fileupload=@${FULL_PATH}" ${SERVER})
        echo ""
        if [[ $response == 405* ]] ; then
                echo "could not upload. error 405 Method not Allowed."
                exit 1
        fi

        # check if xclip is installed, if so copy link to clipboard
        echo "${SERVER}${SERVER_UPLOAD_DIR}/${IMAGE}"
        if type "xclip" &> /dev/null; then
                echo "${SERVER}${SERVER_UPLOAD_DIR}/${IMAGE}" | xclip -in
                echo "copied to clipboard, paste with 'xclip -out' or middle mouse"
        fi
else
        echo "something went wrong, cant read: '${FILENAME}'"
fi
