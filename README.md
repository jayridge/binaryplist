binaryplist
===========

binaryplist is a python module to encode native objects
as Apple compatible binary plists.

To use this application you will need

 * Python 2.6+


Example usage:

    import binaryplist as plist
    import datetime
    import time

    o = {
        "yes":True,
        "no":False,
        "uni":u'abcd\xe9f',
        "real":12.43243,
        "list":[0,1,2],
        "tuple": ('a','b',('c','d')),
        "today":datetime.date.today(),
        "now":time.time()
    }

    bplist = plist.encode(o)
    f = open('/tmp/foo.plist', 'w+')
    f.write(bplist)
    f.close()

