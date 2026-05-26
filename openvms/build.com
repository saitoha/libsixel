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
$ convdir = "SYS$SPECIFIC:[SYSMGR.converters]"
$ vmsdir = "SYS$SPECIFIC:[SYSMGR.openvms]"
$ objdir = "SYS$SPECIFIC:[SYSMGR.openvms.obj]"
$ libfile = objdir + "libsixel.olb"
$ created_library == "0"
$
$ ccflags = "/STANDARD=RELAXED_ANSI/NAMES=(AS_IS,SHORTENED)/PREFIX=ALL"
$ ccdefs = "/DEFINE=(HAVE_CONFIG_H=1)"
$ imgdefs = "/DEFINE=(HAVE_CONFIG_H=1,BUILD_IMG2SIXEL=1)"
$ ccincs = "/INCLUDE_DIRECTORY=(SYS$SPECIFIC:[SYSMGR.openvms],SYS$SPECIFIC:[SYSMGR.include],SYS$SPECIFIC:[SYSMGR.src],SYS$SPECIFIC:[SYSMGR.converters])"
$
$ WRITE SYS$OUTPUT "libsixel OpenVMS bootstrap build"
$ WRITE SYS$OUTPUT "source root: ''root'"
$
$ command_severity = 1
$ IF F$SEARCH("SYS$SPECIFIC:[SYSMGR.openvms]obj.dir") .NES. "" THEN GOTO objdir_ready
$ CREATE /DIRECTORY SYS$SPECIFIC:[SYSMGR.openvms.obj]
$ command_severity = $SEVERITY
$objdir_ready:
$ CALL check_status "create object directory" "''command_severity'"
$
$ CALL generate_sixel_h
$ command_severity = $SEVERITY
$ CALL check_status "generate sixel.h" "''command_severity'"
$
$ command_severity = 1
$ old_library = objdir + "libsixel.olb;*"
$ IF F$SEARCH(old_library) .EQS. "" THEN GOTO old_library_done
$ DELETE 'old_library'
$ command_severity = $SEVERITY
$old_library_done:
$ CALL check_status "delete old object library" "''command_severity'"
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
$ IF $SEVERITY .NE. 1 THEN GOTO failed
$ CALL build_img2sixel
$ IF $SEVERITY .NE. 1 THEN GOTO failed
$
$ WRITE SYS$OUTPUT "OpenVMS bootstrap build complete"
$ WRITE SYS$OUTPUT "static library: [.openvms.obj]libsixel.olb"
$ WRITE SYS$OUTPUT "smoke output:   [.openvms.obj]sixel_smoke.six"
$ WRITE SYS$OUTPUT "img2sixel:      [.openvms.obj]img2sixel.exe"
$ WRITE SYS$OUTPUT "img2sixel out:  [.openvms.obj]img2sixel_smoke.six"
$ EXIT 1
$
$compile_source: SUBROUTINE
$ source = P1
$ dot = F$LOCATE(".", source)
$ object_name = F$EXTRACT(0, dot, source) + ".obj"
$ object = objdir + object_name
$ source_file = srcdir + source
$ CC 'ccflags' 'ccdefs' 'ccincs' /OBJECT='object' 'source_file'
$ command_severity = $SEVERITY
$ CALL check_status "compile ''source'" "''command_severity'" "ALLOW_WARNING"
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
$     command_severity = $SEVERITY
$     created_library == "1"
$ ELSE
$     LIBRARY 'libfile' 'object'
$     command_severity = $SEVERITY
$ ENDIF
$ CALL check_status "archive ''object_name'" "''command_severity'"
$ ENDSUBROUTINE
$
$build_smoke: SUBROUTINE
$ smoke_obj = objdir + "smoke_sixel.obj"
$ smoke_exe = objdir + "smoke_sixel.exe"
$ smoke_source = vmsdir + "smoke_sixel.c"
$ smoke_output = objdir + "sixel_smoke.six"
$ CC 'ccflags' 'ccdefs' 'ccincs' /OBJECT='smoke_obj' 'smoke_source'
$ command_severity = $SEVERITY
$ CALL check_status "compile smoke_sixel.c" "''command_severity'" "ALLOW_WARNING"
$ IF F$SEARCH(smoke_exe) .EQS. "" THEN GOTO old_smoke_done
$ DELETE 'smoke_exe';*
$old_smoke_done:
$ LINK /EXECUTABLE='smoke_exe' 'smoke_obj','libfile'/LIBRARY
$ command_severity = $SEVERITY
$ CALL check_status "link smoke_sixel.exe" "''command_severity'"
$ IF F$SEARCH(smoke_output) .EQS. "" THEN GOTO old_smoke_out_done
$ DELETE 'smoke_output';*
$old_smoke_out_done:
$ RUN 'smoke_exe'
$ command_severity = $SEVERITY
$ CALL check_status "run smoke_sixel.exe" "''command_severity'"
$ IF F$SEARCH(smoke_output) .NES. "" THEN GOTO smoke_output_ready
$ WRITE SYS$OUTPUT "failed: smoke_sixel.exe did not create sixel output"
$ EXIT 2
$smoke_output_ready:
$ EXIT 1
$ ENDSUBROUTINE
$
$build_img2sixel: SUBROUTINE
$ img2sixel_exe = objdir + "img2sixel.exe"
$ link_options = objdir + "img2sixel.opt"
$ sixel_input = objdir + "sixel_smoke.six"
$ sixel_output = objdir + "img2sixel_smoke.six"
$ img2sixel_cmd == "$" + img2sixel_exe
$ CALL compile_converter "img2sixel.c" "img2sixel.obj"
$ CALL compile_converter "completion_utils.c" "img2sixel_completion_utils.obj"
$ CALL compile_converter "compat.c" "img2sixel_compat.obj"
$ CALL compile_converter "path.c" "img2sixel_path.obj"
$ CALL compile_converter "malloc_stub.c" "img2sixel_malloc_stub.obj"
$ CALL compile_converter "cli.c" "img2sixel_cli.obj"
$ CALL compile_converter "aborttrace.c" "img2sixel_aborttrace.obj"
$ CALL write_img2sixel_link_options "''link_options'"
$ IF F$SEARCH(img2sixel_exe) .EQS. "" THEN GOTO old_img2sixel_done
$ DELETE 'img2sixel_exe';*
$old_img2sixel_done:
$ LINK /EXECUTABLE='img2sixel_exe' 'link_options'/OPTIONS
$ command_severity = $SEVERITY
$ CALL check_status "link img2sixel.exe" "''command_severity'"
$ IF F$SEARCH(sixel_output) .EQS. "" THEN GOTO old_img2sixel_out_done
$ DELETE 'sixel_output';*
$old_img2sixel_out_done:
$ img2sixel_cmd --outfile='sixel_output' 'sixel_input'
$ command_severity = $SEVERITY
$ CALL check_status "run img2sixel smoke conversion" "''command_severity'"
$ IF F$SEARCH(sixel_output) .NES. "" THEN GOTO img2sixel_output_ready
$ WRITE SYS$OUTPUT "failed: img2sixel.exe did not create sixel output"
$ EXIT 2
$img2sixel_output_ready:
$ EXIT 1
$ ENDSUBROUTINE
$
$compile_converter: SUBROUTINE
$ source = P1
$ object_name = P2
$ object = objdir + object_name
$ source_file = convdir + source
$ CC 'ccflags' 'imgdefs' 'ccincs' /OBJECT='object' 'source_file'
$ command_severity = $SEVERITY
$ CALL check_status "compile converters/''source'" "''command_severity'" "ALLOW_WARNING"
$ ENDSUBROUTINE
$
$write_img2sixel_link_options: SUBROUTINE
$ link_options = P1
$ IF F$SEARCH(link_options) .EQS. "" THEN GOTO old_img2sixel_opt_done
$ DELETE 'link_options';*
$old_img2sixel_opt_done:
$ OPEN /WRITE opt 'link_options'
$ WRITE opt objdir + "img2sixel.obj"
$ WRITE opt objdir + "img2sixel_completion_utils.obj"
$ WRITE opt objdir + "img2sixel_compat.obj"
$ WRITE opt objdir + "img2sixel_path.obj"
$ WRITE opt objdir + "img2sixel_malloc_stub.obj"
$ WRITE opt objdir + "img2sixel_cli.obj"
$ WRITE opt objdir + "img2sixel_aborttrace.obj"
$ WRITE opt libfile + "/LIBRARY"
$ CLOSE opt
$ command_severity = $SEVERITY
$ CALL check_status "write img2sixel link options" "''command_severity'"
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
$ checked_severity = P2
$ allow_warning = P3
$ IF checked_severity .EQS. "" THEN checked_severity = $SEVERITY
$ checked_severity = F$INTEGER(checked_severity)
$ IF checked_severity .EQ. 0 .AND. allow_warning .EQS. "ALLOW_WARNING" THEN EXIT 1
$ IF checked_severity .NE. 1 .AND. checked_severity .NE. 3
$ THEN
$     WRITE SYS$OUTPUT "failed: ''P1' (severity ''checked_severity')"
$     EXIT 2
$ ENDIF
$ ENDSUBROUTINE
$
$failed:
$ WRITE SYS$OUTPUT "OpenVMS bootstrap build failed"
$ EXIT 2
