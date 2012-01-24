import binaryplist as plist
import datetime
import time

simple = { "dict": {"foo":"bar"}, "a":[13, "butter"], "b":[42], "uni":u'abcd\xe9f', "monkey":"balls" }

o = {
    "yes":True,
    "no":False,
    #"null":None,
    "uni":u'abcd\xe9f',
    "real":12.43243,
    "list":[0,1,2],
    "tuple": ('a','b',('c','d')),
    "today":datetime.date.today(),
    "now":time.time()
}

bplist = plist.encode(o, debug=True)
f = open('/tmp/ass.plist', 'w+')
f.write(bplist)
f.close()



