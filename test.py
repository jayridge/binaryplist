import binaryplist as plist
import datetime
import time
import bson

def hook(o):
    return str(o)

o = {
    "yes":True,
    "no":False,
    # plutil cannot convert this.
    #"null":None,
    "uni":u'abcd\xe9f',
    "real":12.43243,
    "list":[0,1,2],
    "tuple": ('a','b',('c','d')),
    "today":datetime.date.today(),
    "now":time.time(),
    "bson":bson.objectid.ObjectId()
}
o = {
    "bson":bson.objectid.ObjectId()
}

bplist = plist.encode(o, debug=True, object_hook=hook)
f = open('/tmp/ass.plist', 'w+')
f.write(bplist)
f.close()



