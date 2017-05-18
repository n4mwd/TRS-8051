#
# Borland C++ IDE generated makefile
# Generated 5/18/2017 at 9:56:25 AM 
#
.AUTODEPEND


#
# Borland C++ tools
#
IMPLIB  = Implib
BCC32   = Bcc32 +BccW32.cfg 
BCC32I  = Bcc32i +BccW32.cfg 
TLINK32 = TLink32
ILINK32 = Ilink32
TLIB    = TLib
BRC32   = Brc32
TASM32  = Tasm32
#
# IDE macros
#


#
# Options
#
IDE_LinkFLAGS32 =  -LC:\BC5\LIB
IDE_ResFLAGS32 = 
LinkerLocalOptsAtW32_bas51dexe =  -Tpe -aa -V4.0 -c
ResLocalOptsAtW32_bas51dexe = 
BLocalOptsAtW32_bas51dexe = 
CompInheritOptsAt_bas51dexe = -IC:\BC5\INCLUDE 
LinkerInheritOptsAt_bas51dexe = -x
LinkerOptsAt_bas51dexe = $(LinkerLocalOptsAtW32_bas51dexe)
ResOptsAt_bas51dexe = $(ResLocalOptsAtW32_bas51dexe)
BOptsAt_bas51dexe = $(BLocalOptsAtW32_bas51dexe)

#
# Dependency List
#
Dep_bas51 = \
   bas51.exe

bas51 : BccW32.cfg $(Dep_bas51)
  echo MakeNode

Dep_bas51dexe = \
   strfuncs.obj\
   stacks.obj\
   strings.obj\
   floats.obj\
   renum.obj\
   flow_cmds.obj\
   io_cmds.obj\
   graphics.obj\
   drivers.obj\
   evaluator.obj\
   expression.obj\
   command.obj\
   memory.obj\
   editor.obj\
   tokenizer.obj\
   vars.obj\
   globals.obj\
   bas51.obj

bas51.exe : $(Dep_bas51dexe)
  $(ILINK32) @&&|
 /v $(IDE_LinkFLAGS32) $(LinkerOptsAt_bas51dexe) $(LinkerInheritOptsAt_bas51dexe) +
C:\BC5\LIB\c0w32.obj+
strfuncs.obj+
stacks.obj+
strings.obj+
floats.obj+
renum.obj+
flow_cmds.obj+
io_cmds.obj+
graphics.obj+
drivers.obj+
evaluator.obj+
expression.obj+
command.obj+
memory.obj+
editor.obj+
tokenizer.obj+
vars.obj+
globals.obj+
bas51.obj
$<,$*
C:\BC5\LIB\import32.lib+
C:\BC5\LIB\cw32mt.lib



|
strfuncs.obj :  strfuncs.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ strfuncs.c
|

stacks.obj :  stacks.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ stacks.c
|

strings.obj :  strings.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ strings.c
|

floats.obj :  floats.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ floats.c
|

renum.obj :  renum.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ renum.c
|

flow_cmds.obj :  flow_cmds.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ flow_cmds.c
|

io_cmds.obj :  io_cmds.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ io_cmds.c
|

graphics.obj :  graphics.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ graphics.c
|

drivers.obj :  drivers.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ drivers.c
|

evaluator.obj :  evaluator.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ evaluator.c
|

expression.obj :  expression.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ expression.c
|

command.obj :  command.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ command.c
|

memory.obj :  memory.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ memory.c
|

editor.obj :  editor.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ editor.c
|

Dep_tokenizerdobj = \
   tokenizer.h\
   tokenizer.c

tokenizer.obj : $(Dep_tokenizerdobj)
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ tokenizer.c
|

vars.obj :  vars.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ vars.c
|

globals.obj :  globals.c
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ globals.c
|

Dep_bas51dobj = \
   bas51.h\
   bas51.c

bas51.obj : $(Dep_bas51dobj)
  $(BCC32) -P- -c @&&|
 $(CompOptsAt_bas51dexe) $(CompInheritOptsAt_bas51dexe) -o$@ bas51.c
|

# Compiler configuration file
BccW32.cfg : 
   Copy &&|
-w
-R
-v
-WM-
-vi
-H
-H=bas51.csm
-Od
-WM
-W
| $@


