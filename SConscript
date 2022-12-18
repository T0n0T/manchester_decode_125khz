Import('RTT_ROOT')
Import('rtconfig')
from building import *

# get current directory
cwd     = GetCurrentDir()
# The set of source files associated with this SConscript file.
src     = Glob('*.c')
CPPPATH = [cwd, str(Dir('#'))]

group = DefineGroup('mancher', src, depend = [''], CPPPATH = CPPPATH)

Return('group')
