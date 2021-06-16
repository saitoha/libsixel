echo '[start]'

test -z "$1"; HAVE_PNG=$?
test -z "$2"; HAVE_CURL=$?
test ! -z "$3"; BE_SILENT=$?

echo '[test1] invalid option handling'
mkdir -p ${BUILDDIR}/tmp/
touch ${BUILDDIR}/tmp/testfile
chmod -r ${BUILDDIR}/tmp/testfile
test ! $(${BUILDDIR}/img2sixel ${BUILDDIR}/tmp/testfile)
test ! $(${BUILDDIR}/img2sixel ${BUILDDIR}/tmp/invalid_filename)
test ! $(${BUILDDIR}/img2sixel .)
test ! $(${BUILDDIR}/img2sixel -d invalid_option)
test ! $(${BUILDDIR}/img2sixel -r invalid_option)
test ! $(${BUILDDIR}/img2sixel -s invalid_option)
test ! $(${BUILDDIR}/img2sixel -t invalid_option)
test ! $(${BUILDDIR}/img2sixel -w invalid_option)
test ! $(${BUILDDIR}/img2sixel -h invalid_option)
test ! $(${BUILDDIR}/img2sixel -f invalid_option)
test ! $(${BUILDDIR}/img2sixel -q invalid_option)
test ! $(${BUILDDIR}/img2sixel -l invalid_option)
test ! $(${BUILDDIR}/img2sixel -b invalid_option)
test ! $(${BUILDDIR}/img2sixel -E invalid_option)
test ! $(${BUILDDIR}/img2sixel -B invalid_option)
test ! $(${BUILDDIR}/img2sixel -B \#ffff ${TOP_SRCDIR}/images/map8.png)
test ! $(${BUILDDIR}/img2sixel -B \#0000000000000 ${TOP_SRCDIR}/images/map8.png)
test ! $(${BUILDDIR}/img2sixel -B \#00G)
test ! $(${BUILDDIR}/img2sixel -B test)
test ! $(${BUILDDIR}/img2sixel -B rgb:11/11)
test ! $(${BUILDDIR}/img2sixel -%)
test ! $(${BUILDDIR}/img2sixel -m ${BUILDDIR}/tmp/invalid_filename ${TOP_SRCDIR}/images/snake.jpg)
test ! $(${BUILDDIR}/img2sixel -p16 -e ${TOP_SRCDIR}/images/snake.jpg)
test ! $(${BUILDDIR}/img2sixel -I -C0 ${TOP_SRCDIR}/images/snake.png)
test ! $(${BUILDDIR}/img2sixel -I -p8 ${TOP_SRCDIR}/images/snake.png)
test ! $(${BUILDDIR}/img2sixel -p64 -bxterm256 ${TOP_SRCDIR}/images/snake.png)
test ! $(${BUILDDIR}/img2sixel -8 -P ${TOP_SRCDIR}/images/snake.png)

echo '[test2] STDIN handling'
test ! $(echo -n a | ${BUILDDIR}/img2sixel)

echo '[test3] print information'
${BUILDDIR}/img2sixel -H
${BUILDDIR}/img2sixel -V

if [[ $BE_SILENT -eq 0 ]]; then
    SILENT='> /dev/null'
else
    SILENT=''
fi

echo '[test4] conversion options'
eval "${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/snake.jpg -datkinson -flum -saverage | ${BUILDDIR}/img2sixel | tee ${BUILDDIR}/tmp/snake.sixel $SILENT"
eval "${BUILDDIR}/img2sixel -w50% -h150% -dfs -Bblue -thls -shistogram < ${TOP_SRCDIR}/images/snake.jpg | tee ${BUILDDIR}/tmp/snake2.sixel $SILENT"
eval ${BUILDDIR}/img2sixel -w210 -h210 -djajuni -bxterm256 -o ${BUILDDIR}/tmp/snake3.sixel < ${TOP_SRCDIR}/images/snake.jpg $SILENT
eval ${BUILDDIR}/img2sixel -w105% -h100 -B\#000000000 -rnearest < ${TOP_SRCDIR}/images/snake.gif $SILENT
eval ${BUILDDIR}/img2sixel -7 -sauto -w100 -rgaussian -qauto -dburkes -tauto ${TOP_SRCDIR}/images/snake.tga $SILENT
eval ${BUILDDIR}/img2sixel -p200 -8 -scenter -Brgb:0/f/A -h100 -qfull -rhanning -dstucki -thls ${TOP_SRCDIR}/images/snake.tiff $SILENT
eval ${BUILDDIR}/img2sixel -8 -qauto -thls -e ${TOP_SRCDIR}/images/snake.pgm $SILENT
eval ${BUILDDIR}/img2sixel -8 -m ${TOP_SRCDIR}/images/map8-palette.png -Esize ${TOP_SRCDIR}/images/snake.ppm $SILENT
eval ${BUILDDIR}/img2sixel -7 -m ${TOP_SRCDIR}/images/map16-palette.png -Efast ${TOP_SRCDIR}/images/snake.jpg $SILENT
eval ${BUILDDIR}/img2sixel -7 -w300 ${TOP_SRCDIR}/images/snake-palette.png $SILENT
eval ${BUILDDIR}/img2sixel -7 -w100 -h100 -bxterm16 -B\#aB3 -B\#aB3 ${TOP_SRCDIR}/images/snake.pbm $SILENT
eval ${BUILDDIR}/img2sixel -I -dstucki -thls -B\#a0B030 ${TOP_SRCDIR}/images/snake.ppm $SILENT
eval ${BUILDDIR}/img2sixel -bvt340color ${TOP_SRCDIR}/images/snake.ppm $SILENT
eval ${BUILDDIR}/img2sixel -bvt340mono ${TOP_SRCDIR}/images/snake.tga $SILENT
eval ${BUILDDIR}/img2sixel -bgray1 -w120 ${TOP_SRCDIR}/images/snake.tga $SILENT
eval ${BUILDDIR}/img2sixel -bgray2 -w120 ${TOP_SRCDIR}/images/snake.tga $SILENT
eval ${BUILDDIR}/img2sixel -bgray4 -w120 ${TOP_SRCDIR}/images/snake.tga $SILENT
eval ${BUILDDIR}/img2sixel -bgray8 -w120 ${TOP_SRCDIR}/images/snake.tga $SILENT
eval ${BUILDDIR}/img2sixel -I -8 -dburkes -B\#ffffffffffff ${TOP_SRCDIR}/images/snake-ascii.ppm $SILENT
eval ${BUILDDIR}/img2sixel -I -C10 -djajuni ${TOP_SRCDIR}/images/snake.png $SILENT
eval ${BUILDDIR}/img2sixel -I -Eauto ${TOP_SRCDIR}/images/snake-ascii.pgm $SILENT
eval ${BUILDDIR}/img2sixel -I -datkinson ${TOP_SRCDIR}/images/snake-ascii.pbm $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/snake-grayscale.png $SILENT
eval ${BUILDDIR}/img2sixel -m ${TOP_SRCDIR}/images/map8-palette.png ${TOP_SRCDIR}/images/snake-grayscale.png $SILENT
eval ${BUILDDIR}/img2sixel -m ${TOP_SRCDIR}/images/snake-grayscale.png ${TOP_SRCDIR}/images/snake.png $SILENT
eval ${BUILDDIR}/img2sixel -c200x200+100+100 -dx_dither ${TOP_SRCDIR}/images/snake-grayscale.png $SILENT
eval ${BUILDDIR}/img2sixel -c200x200+100+100 -w400 -da_dither ${TOP_SRCDIR}/images/snake-grayscale.png $SILENT
eval ${BUILDDIR}/img2sixel -I ${TOP_SRCDIR}/images/snake-grayscale.png $SILENT
eval ${BUILDDIR}/img2sixel -I ${TOP_SRCDIR}/images/snake-grayscale.jpg $SILENT
eval ${BUILDDIR}/img2sixel -m ${TOP_SRCDIR}/images/map8.six -m ${TOP_SRCDIR}/images/map8.six ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -w200 -p8 ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -c200x200+2000+2000 ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -bxterm16 ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -e ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -I ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -I -da_dither -w100 ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -I -dx_dither -h100 ${TOP_SRCDIR}/images/snake.six $SILENT
eval ${BUILDDIR}/img2sixel -I -c2000x100+40+20 -wauto -h200 -qhigh -dfs -rbilinear -trgb ${TOP_SRCDIR}/images/snake.ppm $SILENT
eval ${BUILDDIR}/img2sixel -I -v -w200 -hauto -c100x1000+40+20 -qlow -dnone -rhamming -thls ${TOP_SRCDIR}/images/snake.bmp $SILENT
eval ${BUILDDIR}/img2sixel -m ${TOP_SRCDIR}/images/map8.png -w200 -fauto -rwelsh ${TOP_SRCDIR}/images/egret.jpg $SILENT
eval ${BUILDDIR}/img2sixel -m ${TOP_SRCDIR}/images/map16.png -w100 -hauto -rbicubic -dauto ${TOP_SRCDIR}/images/snake.ppm $SILENT
eval ${BUILDDIR}/img2sixel -p 16 -C3 -h100 -fnorm -rlanczos2 ${TOP_SRCDIR}/images/snake.jpg $SILENT
eval ${BUILDDIR}/img2sixel -v -p 8 -h200 -fnorm -rlanczos2 -dnone ${TOP_SRCDIR}/images/snake.jpg $SILENT
eval ${BUILDDIR}/img2sixel -p 2 -h100 -wauto -rlanczos3 ${TOP_SRCDIR}/images/snake.jpg $SILENT
eval ${BUILDDIR}/img2sixel -p 1 -h100 -n1 ${TOP_SRCDIR}/images/snake.jpg && printf '\033[*1z' $SILENT
eval ${BUILDDIR}/img2sixel -e -h140 -rlanczos4 -P ${TOP_SRCDIR}/images/snake.jpg $SILENT
eval ${BUILDDIR}/img2sixel -e -i -P ${TOP_SRCDIR}/images/snake.jpg > /dev/null $SILENT
${BUILDDIR}/img2sixel -w204 -h204 ${TOP_SRCDIR}/images/snake.png | ${BUILDDIR}/img2sixel > /dev/null

echo '[test5] DCS arguments handling'
seq 0 10 | while read i; do \
    seq 0 2 | while read j; do \
            ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/map8.png | \
            sed "s/Pq/P${i};;${j}q/" | \
            eval ${BUILDDIR}/img2sixel $SILENT; \
    done; \
done

echo
echo '[test6] DCS format variations'
eval "${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/snake.png| sed 's/C/C:/g'| tr : '\t'| ${BUILDDIR}/img2sixel $SILENT"
eval "${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/snake.png| sed 's/\"1;1;600;450/\"1;1;700;500/'| ${BUILDDIR}/img2sixel $SILENT"

echo
echo '[test7] animation'
eval ${BUILDDIR}/img2sixel -ldisable -dnone -u -lauto ${TOP_SRCDIR}/images/seq2gif.gif $SILENT
eval ${BUILDDIR}/img2sixel -ldisable -dnone -g ${TOP_SRCDIR}/images/seq2gif.gif $SILENT
eval ${BUILDDIR}/img2sixel -ldisable -dnone -u -g ${TOP_SRCDIR}/images/seq2gif.gif $SILENT
eval ${BUILDDIR}/img2sixel -S -datkinson ${TOP_SRCDIR}/images/seq2gif.gif $SILENT

echo
echo '[test8] progressive jpeg'
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/snake-progressive.jpg $SILENT
if test HAVE_PNG; then
echo
echo '[test9] various PNG'
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn0g01.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn0g02.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn0g04.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn0g08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn0g16.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn3p01.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn3p02.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn3p04.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn3p08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/basic/basn6a16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g01.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g02.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g04.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p01.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p02.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p04.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 ${TOP_SRCDIR}/images/pngsuite/basic/basn6a16.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g01.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g02.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g04.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g08.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn0g16.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p01.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p02.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p04.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn3p08.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn4a08.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn4a16.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn6a08.png $SILENT
eval ${BUILDDIR}/img2sixel -c16x16+8+8 ${TOP_SRCDIR}/images/pngsuite/basic/basn6a16.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgai4a08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgai4a16.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgan6a16.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bggn4a16.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png $SILENT
eval ${BUILDDIR}/img2sixel ${TOP_SRCDIR}/images/pngsuite/background/bgyn6a16.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgai4a08.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgai4a16.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgan6a16.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bggn4a16.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png $SILENT
eval ${BUILDDIR}/img2sixel -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgyn6a16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgai4a08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgai4a16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgan6a08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgan6a16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgbn4a08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bggn4a16.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgwn6a08.png $SILENT
eval ${BUILDDIR}/img2sixel -w32 -B\#fff ${TOP_SRCDIR}/images/pngsuite/background/bgyn6a16.png $SILENT
fi
if test HAVE_CURL; then
echo
echo '[test10] curl'
test ! $(${BUILDDIR}/img2sixel file:///test)
test ! $(${BUILDDIR}/img2sixel https:///test)
eval ${BUILDDIR}/img2sixel file:///$(pwd)/${TOP_SRCDIR}/images/snake.jpg $SILENT
if test $(which openssl) && test $(which python2); then \
    openssl genrsa | openssl rsa > ${BUILDDIR}/tmp/server.key; \
    openssl req -new -key ${BUILDDIR}/tmp/server.key -subj "/CN=localhost" | openssl x509 -req -signkey ${BUILDDIR}/tmp/server.key > ${BUILDDIR}/tmp/server.crt; \
    echo "import BaseHTTPServer as bs, SimpleHTTPServer as ss, ssl" > ${BUILDDIR}/tmp/server.py; \
    echo "httpd = bs.HTTPServer(('localhost', 4443), ss.SimpleHTTPRequestHandler)" >> ${BUILDDIR}/tmp/server.py; \
    echo "httpd.socket = ssl.wrap_socket(httpd.socket, certfile='${BUILDDIR}/tmp/server.crt', keyfile='${BUILDDIR}/tmp/server.key', server_side=True)" >> ${BUILDDIR}/tmp/server.py; \
    echo "httpd.handle_request()" >> ${BUILDDIR}/tmp/server.py; \
    echo "httpd.handle_request()" >> ${BUILDDIR}/tmp/server.py; \
    python2 ${BUILDDIR}/tmp/server.py & \
    sleep 1; \
    ! ${BUILDDIR}/img2sixel 'https://localhost:4443/${BUILDDIR}/tmp/snake.sixel'; \
    sleep 1; \
    eval ${BUILDDIR}/img2sixel -k 'https://localhost:4443/${BUILDDIR}/tmp/snake.sixel' $SILENT; \
fi

fi
