import binaryplist as plist
import datetime
import time
import bson

class CustomObj:
    def __init__(self):
        self._dork = "pudding"
        self.monkey = "doodle"
        self.apple = "butter"
    def to_dict(self):
        return dict([(k, getattr(self, k)) for k in self.__dict__.keys() if not k.startswith("_")])


def hook(o):
    print "ARRRGH!, ye called me hook!"
    if isinstance(o, bson.ObjectId):
        return str(o)
    elif isinstance(o, CustomObj):
        return o.to_dict()
    return None

o = {
    "yes":True,
    "no":False,
    # plutil cannot convert this to xml but it encodes properly.
    #"null":None,
    "uni":u'abcd\xe9f',
    "real":12.43243,
    "list":[0,1,2],
    "tuple": ('a','b',('a','b')),
    "today":datetime.date.today(),
    "now":time.time(),
    "bson":bson.objectid.ObjectId(),
    "custom":CustomObj()
}

bplist = plist.encode(o, debug=True, unique=True, object_hook=hook)
f = open('/tmp/ass.plist', 'w+')
f.write(bplist)
f.close()
