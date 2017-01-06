#!/bin/sh

mkdir inc
svn add inc
mkdir src
svn add src
svn move *.c src
