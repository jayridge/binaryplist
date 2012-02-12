#!/usr/bin/env python
# encoding: utf-8
"""
iOS/OS X compatible binary plist encoder/decoder.

These routines are used to wrap native object types so they
can be coerced into plist specific data types.

These are compatible with biplist.
"""

class Uid(int):
    pass

class Data(str):
    pass

# Uid and Data need to exists before this import
import libbinaryplist
encode = libbinaryplist.encode
