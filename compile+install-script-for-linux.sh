#!/bin/bash

#cosmetic stuff
RED='\033[0;31m'
REDBG='\033[41m'
BLACK='\033[0;30m'
YELLOWBG='\033[43m'
NC='\033[0m' # No Color

# building
cmake -B build -S ./
cmake --build build #-j5 #<--- remove the # symbol and insert your cpu cores+1 in there for faster compile times
# the install script
printf "${RED}Warning for developers: ${BLACK} ${REDBG}This operation will write the game on your file system with root privilages. This should be only be done if the program is ready for personal use (such as playing) \n "
printf "for testing your builds run the executable located in the build directory, no need to install it to your system.${NC}\n${BLACK}${YELLOWBG}"
printf "this step is optional and are not required for the program to run, it's just for user convinience ${NC} \n "
while true; do
    read -p "Do you wish to proceed with installing this program? [y/n] " yn
    case $yn in
        [Yy]* ) sudo cmake --install build --strip; break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done
