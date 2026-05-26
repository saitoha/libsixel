$! OpenVMS native bootstrap build for libsixel.
$!
$! Run this procedure from the repository root:
$!
$!   $ @ [.openvms]build.com
$!
$ SET NOON
$ SET PROCESS /PARSE_STYLE=EXTENDED
$ SET DEFAULT SYS$SPECIFIC:[SYSMGR]
$ ON CONTROL_Y THEN GOTO failed
$
$ root = F$ENVIRONMENT("DEFAULT")
$ srcdir = "SYS$SPECIFIC:[SYSMGR.src]"
$ incdir = "SYS$SPECIFIC:[SYSMGR.include]"
$ vmsdir = "SYS$SPECIFIC:[SYSMGR.openvms]"
$ objdir = "SYS$SPECIFIC:[SYSMGR.openvms.obj]"
$ libfile = objdir + "libsixel.olb"
$ created_library == "0"
$
$ ccflags = "/STANDARD=RELAXED_ANSI/NAMES=(AS_IS,SHORTENED)/PREFIX=ALL"
$ ccdefs = "/DEFINE=(HAVE_CONFIG_H=1)"
$ ccincs = "/INCLUDE_DIRECTORY=(SYS$SPECIFIC:[SYSMGR.openvms],SYS$SPECIFIC:[SYSMGR.include],SYS$SPECIFIC:[SYSMGR.src])"
$
$ WRITE SYS$OUTPUT "libsixel OpenVMS bootstrap build"
$ WRITE SYS$OUTPUT "source root: ''root'"
$
$ IF F$SEARCH("SYS$SPECIFIC:[SYSMGR.openvms]obj.dir") .NES. "" THEN GOTO objdir_ready
$ CREATE /DIRECTORY SYS$SPECIFIC:[SYSMGR.openvms.obj]
$objdir_ready:
$ CALL check_status "create object directory"
$
$ CALL generate_sixel_h
$ CALL check_status "generate sixel.h"
$
$ old_library = objdir + "libsixel.olb;*"
$ IF F$SEARCH(old_library) .EQS. "" THEN GOTO old_library_done
$ DELETE 'old_library'
$old_library_done:
$ CALL check_status "delete old object library"
$
$ OPEN /READ srcs SYS$SPECIFIC:[SYSMGR.openvms]sources.dat
$source_loop:
$ READ /END_OF_FILE=source_done srcs source
$ IF source .EQS. "" THEN GOTO source_loop
$ CALL compile_source "''source'"
$ CALL add_object_to_library "''source'"
$ GOTO source_loop
$
$source_done:
$ CLOSE srcs
$ CALL build_smoke
$
$ WRITE SYS$OUTPUT "OpenVMS bootstrap build complete"
$ WRITE SYS$OUTPUT "static library: [.openvms.obj]libsixel.olb"
$ WRITE SYS$OUTPUT "smoke output:   [.openvms.obj]sixel_smoke.six"
$ EXIT 1
$
$compile_source: SUBROUTINE
$ source = P1
$ dot = F$LOCATE(".", source)
$ object_name = F$EXTRACT(0, dot, source) + ".obj"
$ object = objdir + object_name
$ source_file = srcdir + source
$ CC 'ccflags' 'ccdefs' 'ccincs' /OBJECT='object' 'source_file'
$ CALL check_status "compile ''source'"
$ ENDSUBROUTINE
$
$add_object_to_library: SUBROUTINE
$ source = P1
$ dot = F$LOCATE(".", source)
$ object_name = F$EXTRACT(0, dot, source) + ".obj"
$ object = objdir + object_name
$ IF created_library .EQS. "0"
$ THEN
$     LIBRARY /CREATE 'libfile' 'object'
$     created_library == "1"
$ ELSE
$     LIBRARY 'libfile' 'object'
$ ENDIF
$ CALL check_status "archive ''object_name'"
$ ENDSUBROUTINE
$
$build_smoke: SUBROUTINE
$ smoke_obj = objdir + "smoke_sixel.obj"
$ smoke_exe = objdir + "smoke_sixel.exe"
$ smoke_source = vmsdir + "smoke_sixel.c"
$ CC 'ccflags' 'ccdefs' 'ccincs' /OBJECT='smoke_obj' 'smoke_source'
$ CALL check_status "compile smoke_sixel.c"
$ IF F$SEARCH(smoke_exe) .EQS. "" THEN GOTO old_smoke_done
$ DELETE 'smoke_exe';*
$old_smoke_done:
$ LINK /EXECUTABLE='smoke_exe' 'smoke_obj','libfile'/LIBRARY
$ CALL check_status "link smoke_sixel.exe"
$ RUN 'smoke_exe'
$ CALL check_status "run smoke_sixel.exe"
$ ENDSUBROUTINE
$
$generate_sixel_h: SUBROUTINE
$ input_file = incdir + "sixel^.h.in"
$ output_file = vmsdir + "sixel.h"
$ OPEN /READ in 'input_file'
$ OPEN /WRITE out 'output_file'
$generate_loop:
$ READ /END_OF_FILE=generate_done in line
$ token = "@PACKAGE_VERSION@"
$ value = "1.11.0-pre"
$ GOSUB replace_all
$ token = "@LS_LTVERSION@"
$ value = "1:6:0"
$ GOSUB replace_all
$ token = "@attr_func_deprecated@"
$ value = ""
$ GOSUB replace_all
$ token = "@attr_var_deprecated@"
$ value = ""
$ GOSUB replace_all
$ WRITE out line
$ GOTO generate_loop
$replace_all:
$ token_len = F$LENGTH(token)
$replace_loop:
$ pos = F$LOCATE(token, line)
$ IF pos .EQ. F$LENGTH(line) THEN RETURN
$ tail_pos = pos + token_len
$ tail_len = F$LENGTH(line) - tail_pos
$ head = F$EXTRACT(0, pos, line)
$ tail = F$EXTRACT(tail_pos, tail_len, line)
$ line = head + value + tail
$ GOTO replace_loop
$
$generate_done:
$ CLOSE in
$ CLOSE out
$ ENDSUBROUTINE
$
$check_status: SUBROUTINE
$ IF $SEVERITY .GE. 2
$ THEN
$     WRITE SYS$OUTPUT "failed: ''P1'"
$     EXIT 2
$ ENDIF
$ ENDSUBROUTINE
$
$failed:
$ WRITE SYS$OUTPUT "OpenVMS bootstrap build failed"
$ EXIT 2
