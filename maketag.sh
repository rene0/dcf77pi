#!/bin/sh
# Copyright 2013 René Ladan
# SPDX-License-Identifier: BSD-2-Clause

git tag -a $1 -m "Version $1"
git push origin $1
