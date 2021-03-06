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
    if isinstance(o, bson.ObjectId):
        return str(o)
    elif isinstance(o, CustomObj):
        return o.to_dict()
    return None

o = {
    "hashtest":[0, 1, 1453079729203098304, 'ass'],
    "yes":True,
    "maybe":True,
    "no":False,
    # plutil cannot convert this to xml but it encodes properly.
    "null":None,
    "uni":u'abcd\xe9f',
    "real":12.43243,
    "list":[0,1,2],
    "tuple": ('a','b',('a','b')),
    "today":datetime.date.today(),
    "stilltoday":datetime.date.today(),
    "now":time.time(),
    "bson":bson.objectid.ObjectId(),
    "custom":CustomObj(),
    "data":plist.Data('this is my awesome data'),
    "uid":plist.Uid('13')
}

bplist = plist.encode(o, debug=True, unique=True, convert_nulls=True, object_hook=hook)
f = open('/tmp/ass.plist', 'w+')
f.write(bplist)
f.close()
