from distutils.core import setup, Extension
 
module1 = Extension('binaryplist',
                    sources = ['binaryplist.c', 'encoder.c'],
                    include_dirs = ['.'])
 
setup (name = 'binaryplist',
        version = '0.1',
        description = 'OS X compatible binary plist encoder/decoder.',
        author = 'Jay Ridgeway',
        author_email ='jayridge@gmail.com',
        ext_modules = [module1])
