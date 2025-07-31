echo '[start]'
echo '[test11] sixel2png'
test ! $(${BUILDDIR}/sixel2png -i ${BUILDDIR}/tmp/unknown.sixel)
test ! $(${BUILDDIR}/sixel2png -% < ${BUILDDIR}/tmp/snake.sixel)
test ! $(${BUILDDIR}/sixel2png invalid_filename < ${BUILDDIR}/tmp/snake.sixel)
${BUILDDIR}/sixel2png -H
${BUILDDIR}/sixel2png -V
${BUILDDIR}/sixel2png < ${BUILDDIR}/tmp/snake.sixel > ${BUILDDIR}/tmp/snake1.png
${BUILDDIR}/sixel2png < ${BUILDDIR}/tmp/snake2.sixel > ${BUILDDIR}/tmp/snake2.png
${BUILDDIR}/sixel2png - - < ${BUILDDIR}/tmp/snake3.sixel > ${BUILDDIR}/tmp/snake3.png
${BUILDDIR}/sixel2png -i ${BUILDDIR}/tmp/snake.sixel -o ${BUILDDIR}/tmp/snake4.png
