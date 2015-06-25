# 
all: html

confcheck:
	@if test "x$(PHPDOC)" = "x"; then echo PHPDOC not set; exit 3; fi

manual.xml: manual.xml.in
	sed -e's:@PHPDOC@:$(PHPDOC):g' < manual.xml.in > manual.xml

html: confcheck manual.xml
	rm -rf html; mkdir html
	SP_ENCODING=XML SP_CHARSET_FIXED=YES openjade -D $(PHPDOC) -wno-idref -c $(PHPDOC)/docbook/docbook-dsssl/catalog -c $(PHPDOC)/phpbook/phpbook-dsssl/defaults/catalog -d $(PHPDOC)/phpbook/phpbook-dsssl/html.dsl -V use-output-dir -t sgml $(PHPDOC)/phpbook/phpbook-xml/phpdocxml.dcl manual.xml

bightml: confcheck manual.xml
	rm -rf html; mkdir html
	SP_ENCODING=XML SP_CHARSET_FIXED=YES openjade -D $(PHPDOC) -wno-idref -c $(PHPDOC)/docbook/docbook-dsssl/catalog -c $(PHPDOC)/phpbook/phpbook-dsssl/defaults/catalog -d $(PHPDOC)/phpbook/phpbook-dsssl/html.dsl -V nochunks -t sgml $(PHPDOC)/phpbook/phpbook-xml/phpdocxml.dcl manual.xml > manual.html

tex: manual.tex

manual.tex: confcheck manual.xml
	SP_ENCODING=XML SP_CHARSET_FIXED=YES openjade -D $(PHPDOC) -wno-idref -c $(PHPDOC)/docbook/docbook-dsssl/catalog -c $(PHPDOC)/phpbook/phpbook-dsssl/defaults/catalog -d $(PHPDOC)/phpbook/phpbook-dsssl/print.dsl -t tex $(PHPDOC)/phpbook/phpbook-xml/phpdocxml.dcl manual.xml

pdf: manual.tex
	pdfjadetex manual.tex && pdfjadetex manual.tex && pdfjadetex manual.tex
