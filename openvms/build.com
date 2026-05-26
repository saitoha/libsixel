$! OpenVMS native bootstrap build for libsixel.
$!
$! Run this procedure from the repository root:
$!
$!   $ @ [.openvms]build.com
$!
$ SET NOON
$ SET PROCESS /PARSE_STYLE=EXTENDED
$ SET DEFAULT SYS$SYSROOT:[SYSMGR]
$ ON CONTROL_Y THEN GOTO failed
$ DEFINE /NOLOG LIBSIXEL_OPENVMS_DIR SYS$SYSROOT:[SYSMGR.openvms]
$ DEFINE /NOLOG LIBSIXEL_INCLUDE_DIR SYS$SYSROOT:[SYSMGR.include]
$ DEFINE /NOLOG LIBSIXEL_SRC_DIR SYS$SYSROOT:[SYSMGR.src]
$ DEFINE /NOLOG LIBSIXEL_CONVERTERS_DIR SYS$SYSROOT:[SYSMGR.converters]
$
$ root = F$ENVIRONMENT("DEFAULT")
$ srcdir = "SYS$SYSROOT:[SYSMGR.src]"
$ incdir = "SYS$SYSROOT:[SYSMGR.include]"
$ convdir = "SYS$SYSROOT:[SYSMGR.converters]"
$ vmsdir = "SYS$SYSROOT:[SYSMGR.openvms]"
$ objdir = "SYS$SYSROOT:[SYSMGR.openvms.obj]"
$ libfile = objdir + "libsixel.olb"
$ shared_image = objdir + "libsixelshr.exe"
$ created_library == "0"
$
$ ccflags = "/STANDARD=RELAXED_ANSI/NAMES=(AS_IS,SHORTENED)/PREFIX=ALL"
$ ccdefs = "/DEFINE=(HAVE_CONFIG_H=1)"
$ imgdefs = "/DEFINE=(HAVE_CONFIG_H=1,BUILD_IMG2SIXEL=1)"
$ ccincs = "/INCLUDE_DIRECTORY=(LIBSIXEL_OPENVMS_DIR:,LIBSIXEL_INCLUDE_DIR:,LIBSIXEL_SRC_DIR:,LIBSIXEL_CONVERTERS_DIR:)"
$
$ WRITE SYS$OUTPUT "libsixel OpenVMS bootstrap build"
$ WRITE SYS$OUTPUT "source root: ''root'"
$
$ command_severity = 1
$ found_file = F$SEARCH("SYS$SYSROOT:[SYSMGR.openvms]obj.dir")
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .NE. 0 THEN GOTO objdir_ready
$ CREATE /DIRECTORY SYS$SYSROOT:[SYSMGR.openvms.obj]
$ command_severity = $SEVERITY
$objdir_ready:
$ WRITE SYS$OUTPUT "object directory ready"
$ checked_message = "create object directory"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$
$ WRITE SYS$OUTPUT "generating [.openvms]sixel.h"
$ GOSUB generate_sixel_h
$ command_severity = $SEVERITY
$ checked_message = "generate sixel.h"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ WRITE SYS$OUTPUT "generated [.openvms]sixel.h"
$
$ command_severity = 1
$ old_library = objdir + "libsixel.olb;*"
$ found_file = F$SEARCH(old_library)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_library_done
$ DELETE 'old_library'
$ command_severity = $SEVERITY
$old_library_done:
$ checked_message = "delete old object library"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ WRITE SYS$OUTPUT "building static library"
$
$ OPEN /READ srcs SYS$SYSROOT:[SYSMGR.openvms]sources.dat
$source_loop:
$ READ /END_OF_FILE=source_done srcs source
$ IF source .EQS. "" THEN GOTO source_loop
$ GOSUB compile_source
$ GOSUB add_object_to_library
$ GOTO source_loop
$
$source_done:
$ CLOSE srcs
$ WRITE SYS$OUTPUT "building shareable image"
$ GOSUB build_shareable_image
$ GOSUB build_smoke
$ GOSUB build_shared_smoke
$ GOSUB build_img2sixel
$
$ WRITE SYS$OUTPUT "OpenVMS bootstrap build complete"
$ WRITE SYS$OUTPUT "static library: [.openvms.obj]libsixel.olb"
$ WRITE SYS$OUTPUT "shareable image: [.openvms.obj]libsixelshr.exe"
$ WRITE SYS$OUTPUT "smoke output:   [.openvms.obj]sixel_smoke.six"
$ WRITE SYS$OUTPUT "shared smoke:   [.openvms.obj]smoke_sixel_shared.exe"
$ WRITE SYS$OUTPUT "img2sixel:      [.openvms.obj]img2sixel.exe"
$ WRITE SYS$OUTPUT "img2sixel out:  [.openvms.obj]img2sixel_smoke.six"
$ EXIT 1
$
$check_status:
$ IF checked_severity .EQS. "" THEN checked_severity = $SEVERITY
$ checked_severity = F$INTEGER(checked_severity)
$ IF checked_severity .EQ. 0 .AND. allow_warning .EQS. "ALLOW_WARNING" THEN RETURN
$ IF checked_severity .NE. 1 .AND. checked_severity .NE. 3
$ THEN
$     WRITE SYS$OUTPUT "failed: ''checked_message' (severity ''checked_severity')"
$     GOTO failed
$ ENDIF
$ RETURN
$
$compile_source:
$ dot = F$LOCATE(".", source)
$ object_name = F$EXTRACT(0, dot, source) + ".obj"
$ object = objdir + object_name
$ source_file = srcdir + source
$ CC 'ccflags' 'ccdefs' 'ccincs' /OBJECT='object' 'source_file'
$ command_severity = $SEVERITY
$ checked_message = "compile ''source'"
$ checked_severity = command_severity
$ allow_warning = "ALLOW_WARNING"
$ GOSUB check_status
$ RETURN
$
$add_object_to_library:
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
$ checked_message = "archive ''object_name'"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ RETURN
$
$build_shareable_image:
$ link_options = objdir + "libsixelshr.opt"
$ GOSUB write_shareable_link_options
$ found_file = F$SEARCH(shared_image)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_shared_done
$ DELETE 'shared_image';*
$old_shared_done:
$ LINK /SHAREABLE='shared_image' 'link_options'/OPTIONS
$ command_severity = $SEVERITY
$ checked_message = "link libsixelshr.exe"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ RETURN
$
$write_shareable_link_options:
$ found_file = F$SEARCH(link_options)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_shared_opt_done
$ DELETE 'link_options';*
$old_shared_opt_done:
$ OPEN /WRITE opt 'link_options'
$ OPEN /READ srcs SYS$SYSROOT:[SYSMGR.openvms]sources.dat
$share_object_loop:
$ READ /END_OF_FILE=share_object_done srcs source
$ IF source .EQS. "" THEN GOTO share_object_loop
$ dot = F$LOCATE(".", source)
$ object_name = F$EXTRACT(0, dot, source) + ".obj"
$ WRITE opt objdir + object_name
$ GOTO share_object_loop
$share_object_done:
$ CLOSE srcs
$ WRITE opt "! Bootstrap exports used by smoke_sixel.c."
$ WRITE opt "CASE_SENSITIVE=YES"
$ WRITE opt "SYMBOL_VECTOR=(sixel_output_new=PROCEDURE,sixel_dither_get=PROCEDURE,sixel_dither_set_pixelformat=PROCEDURE,sixel_encode=PROCEDURE,sixel_output_unref=PROCEDURE)"
$ WRITE opt "CASE_SENSITIVE=NO"
$ WRITE opt "GSMATCH=LEQUAL,1,0"
$ CLOSE opt
$ command_severity = $SEVERITY
$ checked_message = "write libsixelshr link options"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ RETURN
$
$build_smoke:
$ smoke_obj = objdir + "smoke_sixel.obj"
$ smoke_exe = objdir + "smoke_sixel.exe"
$ smoke_source = vmsdir + "smoke_sixel.c"
$ smoke_output = objdir + "sixel_smoke.six"
$ CC 'ccflags' 'ccdefs' 'ccincs' /OBJECT='smoke_obj' 'smoke_source'
$ command_severity = $SEVERITY
$ checked_message = "compile smoke_sixel.c"
$ checked_severity = command_severity
$ allow_warning = "ALLOW_WARNING"
$ GOSUB check_status
$ found_file = F$SEARCH(smoke_exe)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_smoke_done
$ DELETE 'smoke_exe';*
$old_smoke_done:
$ LINK /EXECUTABLE='smoke_exe' 'smoke_obj','libfile'/LIBRARY
$ command_severity = $SEVERITY
$ checked_message = "link smoke_sixel.exe"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ found_file = F$SEARCH(smoke_output)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_smoke_out_done
$ DELETE 'smoke_output';*
$old_smoke_out_done:
$ RUN 'smoke_exe'
$ command_severity = $SEVERITY
$ checked_message = "run smoke_sixel.exe"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ found_file = F$SEARCH(smoke_output)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .NE. 0 THEN GOTO smoke_output_ready
$ WRITE SYS$OUTPUT "failed: smoke_sixel.exe did not create sixel output"
$ GOTO failed
$smoke_output_ready:
$ RETURN
$
$build_shared_smoke:
$ smoke_obj = objdir + "smoke_sixel.obj"
$ smoke_exe = objdir + "smoke_sixel_shared.exe"
$ smoke_output = objdir + "sixel_smoke.six"
$ link_options = objdir + "smoke_sixel_shared.opt"
$ GOSUB write_shared_smoke_link_options
$ found_file = F$SEARCH(smoke_exe)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_shared_smoke_done
$ DELETE 'smoke_exe';*
$old_shared_smoke_done:
$ LINK /EXECUTABLE='smoke_exe' 'link_options'/OPTIONS
$ command_severity = $SEVERITY
$ checked_message = "link smoke_sixel_shared.exe"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ found_file = F$SEARCH(smoke_output)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_shared_smoke_out_done
$ DELETE 'smoke_output';*
$old_shared_smoke_out_done:
$ DEFINE /USER_MODE LIBSIXELSHR 'shared_image'
$ command_severity = $SEVERITY
$ checked_message = "define LIBSIXELSHR"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ RUN 'smoke_exe'
$ command_severity = $SEVERITY
$ checked_message = "run smoke_sixel_shared.exe"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ found_file = F$SEARCH(smoke_output)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .NE. 0 THEN GOTO shared_smoke_output_ready
$ WRITE SYS$OUTPUT "failed: smoke_sixel_shared.exe did not create sixel output"
$ GOTO failed
$shared_smoke_output_ready:
$ RETURN
$
$write_shared_smoke_link_options:
$ found_file = F$SEARCH(link_options)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_shared_smoke_opt_done
$ DELETE 'link_options';*
$old_shared_smoke_opt_done:
$ OPEN /WRITE opt 'link_options'
$ WRITE opt smoke_obj
$ WRITE opt shared_image + "/SHAREABLE"
$ CLOSE opt
$ command_severity = $SEVERITY
$ checked_message = "write shared smoke link options"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ RETURN
$
$build_img2sixel:
$ img2sixel_exe = objdir + "img2sixel.exe"
$ link_options = objdir + "img2sixel.opt"
$ sixel_input = objdir + "sixel_smoke.six"
$ sixel_output = objdir + "img2sixel_smoke.six"
$ img2sixel_cmd == "$" + img2sixel_exe
$ converter_source = "img2sixel.c"
$ converter_object = "img2sixel.obj"
$ GOSUB compile_converter
$ converter_source = "completion_utils.c"
$ converter_object = "img2sixel_completion_utils.obj"
$ GOSUB compile_converter
$ converter_source = "compat.c"
$ converter_object = "img2sixel_compat.obj"
$ GOSUB compile_converter
$ converter_source = "path.c"
$ converter_object = "img2sixel_path.obj"
$ GOSUB compile_converter
$ converter_source = "malloc_stub.c"
$ converter_object = "img2sixel_malloc_stub.obj"
$ GOSUB compile_converter
$ converter_source = "cli.c"
$ converter_object = "img2sixel_cli.obj"
$ GOSUB compile_converter
$ converter_source = "aborttrace.c"
$ converter_object = "img2sixel_aborttrace.obj"
$ GOSUB compile_converter
$ GOSUB write_img2sixel_link_options
$ found_file = F$SEARCH(img2sixel_exe)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_img2sixel_done
$ DELETE 'img2sixel_exe';*
$old_img2sixel_done:
$ LINK /EXECUTABLE='img2sixel_exe' 'link_options'/OPTIONS
$ command_severity = $SEVERITY
$ checked_message = "link img2sixel.exe"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ found_file = F$SEARCH(sixel_output)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_img2sixel_out_done
$ DELETE 'sixel_output';*
$old_img2sixel_out_done:
$ img2sixel_cmd --outfile='sixel_output' 'sixel_input'
$ command_severity = $SEVERITY
$ checked_message = "run img2sixel smoke conversion"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ found_file = F$SEARCH(sixel_output)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .NE. 0 THEN GOTO img2sixel_output_ready
$ WRITE SYS$OUTPUT "failed: img2sixel.exe did not create sixel output"
$ GOTO failed
$img2sixel_output_ready:
$ RETURN
$
$compile_converter:
$ source = converter_source
$ object_name = converter_object
$ object = objdir + object_name
$ source_file = convdir + source
$ CC 'ccflags' 'imgdefs' 'ccincs' /OBJECT='object' 'source_file'
$ command_severity = $SEVERITY
$ checked_message = "compile converters/''source'"
$ checked_severity = command_severity
$ allow_warning = "ALLOW_WARNING"
$ GOSUB check_status
$ RETURN
$
$write_img2sixel_link_options:
$ found_file = F$SEARCH(link_options)
$ found_len = F$LENGTH("''found_file'")
$ IF found_len .EQ. 0 THEN GOTO old_img2sixel_opt_done
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
$ checked_message = "write img2sixel link options"
$ checked_severity = command_severity
$ allow_warning = ""
$ GOSUB check_status
$ RETURN
$
$generate_sixel_h:
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
$ RETURN
$
$failed:
$ WRITE SYS$OUTPUT "OpenVMS bootstrap build failed"
$ EXIT 2
