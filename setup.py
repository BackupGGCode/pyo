from distutils.core import setup, Extension
import os

path = 'src/engine/'
files = ['pyomodule.c', 'servermodule.c', 'streammodule.c', 'dummymodule.c', 'mixmodule.c', 'inputfadermodule.c']
source_files = [path + f for f in files]
path = 'src/objects/'
files = ['tablemodule.c', 'oscilmodule.c', 'filtremodule.c', 'noisemodule.c', 'distomodule.c', 
        'inputmodule.c', 'fadermodule.c', 'midictlmodule.c', 'oscmodule.c', 'delaymodule.c', 'sfplayermodule.c']
source_files = source_files + [path + f for f in files]

include_dirs = ['include']
libraries = ['portaudio', 'portmidi', 'sndfile', 'lo']

extension = [Extension("_pyo", source_files, include_dirs=include_dirs, libraries=libraries, 
             extra_link_args=["-mmacosx-version-min=10.4"])]

setup(  name = "_pyo",
        author = "Olivier Belanger",
        author_email = "belangeo@gmail.com",
        version = "0.01",
        description = "Python dsp module.",
        long_description = "pyo is a Python module written in C to help digital signal processing script creation.",
        url = "http://code.google.com/p/pyo/",
        ext_modules = extension)
         
os.system('rm -rf build')
      
