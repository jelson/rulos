#!/bin/bash

cat $2 $1 > /tmp/tmp-cat-$$
mv /tmp/tmp-cat-$$ $1
