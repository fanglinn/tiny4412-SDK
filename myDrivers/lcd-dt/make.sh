#!/bin/bash

echo "### clean..."
make clean
echo "### make..."
make
#echo "make dtbs"
#make dtbs
echo "### install..."
make install
echo "### done"
