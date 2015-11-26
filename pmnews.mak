#===================================================================
#
#   PMNEWS Make file
#
#===================================================================

#===================================================================
#
#   Sample application makefile,common definitions for the IBM C
#   compiler environment
#===================================================================
.SUFFIXES:
.SUFFIXES: .rc .res .obj .lst .c .asm .hlp .itl .ipf
#===================================================================
# Default compilation macros for sample programs
#
# Compile switchs  that are enabled
# /c      compile don't link
# /Gm+    use the multi-threaded libraries
# /ss     allow  "//" for comment lines
# /Ms     use the system calling convention and not optilink as the default
# /Gd-    static linking
# /Se     allow cset  extensions
# /Kb     basic diagnostics
#
#Note: /D__MIG_LIB__ will be coming out after LA and code should be changed
#      accordingly.
#



AFLAGS  = /Mx -t -z
ASM     = ml /c /Zm
LFLAGS   = /NOL /NOE /NOD /BASE:65536 /ALIGN:16 /EXEPACK /M /De
!IFDEF  NODBG
LFLAGS   = /NOL /NOE /NOD /BASE:65536 /ALIGN:16 /EXEPACK /M
!ENDIF
LINK    = LINK386  $(LFLAGS)
LIBS    = DDE4MBS + OS2386
STLIBS  = DDE4SBS + OS2386
MTLIBS  = DDE4MBS + DDE4MBM  + os2386
DLLLIBS = DDE4NBS + os2386
VLIBS   = DDE4SBS + vdh + os2386

.c.lst:
    $(CC) -Fc$*.lst -Fo$*.obj $*.c

.c.obj:
    $(CC) -Fo$*.obj $*.c

.asm.obj:
    $(ASM)   $*.asm

.ipf.hlp:
        ipfc $*.ipf /W3

.itl.hlp:
        cc  -P $*.itl
        ipfc $*.i
        del $*.i

.rc.res:
        rc -r -p -x $*.rc

CC         = icc /c /Kb /Ge /Gd- /Se /Re /ss /Gm+ /Ti
!IFDEF NODBG
CC         = icc /c /Kb /Ge /Gd- /Se /Re /ss /Gm+
!ENDIF

HEADERS = pmnews.h defs.h pmdefs.h

#-------------------------------------------------------------------
#   A list of all of the object files
#-------------------------------------------------------------------
ALL_OBJ1 = pmnews.obj  pmnews1.obj active.obj history.obj pmmthrd.obj


all: pmnews.exe


pmnews.l: pmnews.mak
    echo $(ALL_OBJ1)            > pmnews.l
    echo pmnews.exe           >> pmnews.l
    echo pmnews.map           >> pmnews.l
    echo $(MTLIBS)            >> pmnews.l
    echo pmnews.def           >> pmnews.l




pmnews.res: pmnews.rc pmnews.ico pmnews.h pmdefs.h

pmnews.hlp: pmnews.ipf pmdefs.h

history.obj: history.c history.h defs.h

active.obj:  active.c active.h defs.h

pmmthrd.obj: pmmthrd.c $(HEADERS)

pmnews1.obj: pmnews1.c $(HEADERS)

pmnews.obj: pmnews.c $(HEADERS)

pmnews.exe: $(ALL_OBJ1)  pmnews.def pmnews.l pmnews.res pmnews.hlp
    $(LINK) @pmnews.l
    rc pmnews.res pmnews.exe
