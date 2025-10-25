f="$(dirname "${0}")"/../images/autumn.png
crop=360x320+1200+500
sample=400%
fontface="Verdana-Bold"
fontsize=90
fontcolor="#563d1c"
strokecolor="#ffe0ec"
strokewidth=5
textmargin=30
textoptions="
 +repage -antialias -font ${fontface} \
 -pointsize ${fontsize} -stroke ${strokecolor} \
 -strokewidth ${strokewidth} \
 -fill ${fontcolor} -gravity southeast \
"

outfile="${1}"

test -n "${outfile}" || {
    echo "usage: ${0} <output file>"
    exit 2;
}

for n in 256 128 64 32 16 8 4; do
    cat ${f} |
    tee >(magick - -crop "${crop}" -sample "${sample}" ${textoptions} \
                   -draw "text ${textmargin},${textmargin} 'original'" \
                   /tmp/original.png
    ) |
    converters/img2sixel -p"${n}" -dfs |
    tee >(converters/sixel2png -dnone |
          tee >(magick - -crop "${crop}" -sample "${sample}" ${textoptions} \
                         -draw "text  ${textmargin},${textmargin} 'sixelized, ${n} colors'" \
                         /tmp/sixelized.png
          ) |
          tee >(magick - -type Palette -define png:color-type=3 -define png:bit-depth=8 png:- |
                undither /dev/stdin /dev/stdout |
                magick - -crop "${crop}" -sample "${sample}" ${textoptions} \
                         -draw "text ${textmargin},${textmargin} 'kornelski/undither'" \
                         /tmp/undither.png
          ) |
          magick - -selective-blur 3x1+20% \
                   -crop "${crop}" -sample "${sample}" ${textoptions} \
                   -draw "text ${textmargin},${textmargin} 'ImageMagick\n-selective-blur 3x1+20%'" \
                   /tmp/selectiveblur-20.png
    ) |
    converters/sixel2png -dk_undither+ |
    magick - -crop "${crop}" -sample "${sample}" ${textoptions} \
             -draw "text ${textmargin},${textmargin} 'sixel2png -dk_undither+'" \
             /tmp/dequantized.png || break

    magick /tmp/{original,sixelized,selectiveblur-20,undither,dequantized}.png \
           +append /tmp/n-"${n}".png || break
done || exit 1

magick /tmp/n-{256,128,64,32,16,8,4}.png \
    -strip -interlace none -define png:compression-level=9 -define png:filter=5 -append "${outfile}"

