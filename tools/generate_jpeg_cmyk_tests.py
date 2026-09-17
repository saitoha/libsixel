#!/usr/bin/env python3
"""Metadata-only JPEG controls and an independent affine CMYK oracle."""
from decimal import Decimal as D, localcontext
from pathlib import Path
import base64
import hashlib
import json
import itertools
import struct

from generate_lcms_input_tests import array, srgb
from generate_tiff_fixtures import apply, destination_matrix, inverse
from generate_tiff_icc_tests import png_rgb8

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'tests/data/colormgmt/input/custom/cmyk_mft2_valid.jpg'
# Frozen JCS_CMYK/JDCT_ISLOW samples from libjpeg-turbo 3.2.0, before any CMM.
# verify_jpeg_cmyk_source.c independently emits this raster on stdout.
RAW = base64.b85decode(
    b'|NpP+_y4c!@Bgps=>M<l-~X@c*8i{T&i}9L$N#VEzW=Z5wg0c{t^cp<rT?$%o&T@vmH)5n'
    b'jQ_9eg#WMWeE+ZObpNmGYyYq7WB;$~TmP@?Q~$5)OaHIyLjSMpI{&ZhGXJmZD*vzRA^)%I'
    b'8UL^A5&y623IDI_0ROM+|M#!!_xG>s@At3k==ZPd-}kTU*7vXL&iAkD$M>)6zW1-|wfC><'
    b't@p3%rT4Gvo%gTnmG`gfjQ6kWg!ixOeD|;Gboa08Yxl3~WB0G?TlcT)Q}?gyOZTtqLiexh'
    b'I`^;ZGWW0RD)+DJA@{HA8TYU25%;g_3HPt-0Qax!|L?Es_wTRk@9(ec=<l!V-|w&M*6*+D'
    b'&hM}5$M3J}zVEN=wePR%t?#evrSGrno$s&fmG7_XjPI}OgzvBGeDAO8bnmb0Ywxe?WACr)'
    b'Tko&yQ}3_qOYg7iLhrBZI`6ORGVibJD(|oBA@8s28Sk&_5$~_-3Gc7#0PnBs|L3pk_vf$c'
    b'@8_@U=;yEN-{-IE*5|M5&gZY|$LFu>zUQy&wdb$vt>>@nrRT5fo#(IXmFKVPjOVZGgy*m8'
    b'eCMz0bmy<@Yv-@)W9P5yTj#IqQ|GViOXsiaLg%mRI_IzJGUu=BD(A23A?L5_8RxI-5$CV#'
    b'3Foit0Ozmk|KG3c_usGU@87TM=-;pF-`}t6*59w|&fl-=$KS8(zTdCwwcoGnt>3TfrQfgX'
    b'o!_tPmEW)HjNh;8gx|00eBZC@bl<P*Yu~TyW8bgqTi>tiQ{S)aOW&{SLf^0JI^VDBGT*Q3'
    b'D&Mc`A>Xg-8Q-t#5#O)t3E!{l0N=0c|JJYT_tvlL@7AyD=+>|6-`20|*4D4<&epH%$JVdw'
    b'zSghnwbrlet=6yWrPi<Ooz}1GmDaE8jMlH~gx0U?eAch)bk?uyYu2ypW7e<hTh_1ZQ`WER'
    b'OV+RJLe{VAI@Yi2GS;u_D%P*-A=a<!8P>1s5!SEk3D&Rc0M@VT|IV-L_s*~D@6NC5=+3X}'
    b'-_Eb=*3Pf%&d#sv$Ih?ozRs`fwa%~Wt<JCOrOvPGozAc8mCmp0jLxs?gwC()e9o`ybk48q'
    b'YtFChW6rPZTh6cRQ_ipJOU|$BLe8)2I?k`_GS08-D$cL#A<nPs8P2ck5zepc3C^$U0M4)L'
    b'|HiND_r|a5@5Zm|=*F+>-^Q=&*2b^v&c?6n$HuSgzQ(WXwZ^aOt;VnGrN*!8oyM>0mBz2@'
    b'jK;6)gvPJye8#WqbjGjiYsRnZW5%!RTgI>JQ^v3BOUAG3LdLJ_I>xW-GRCj#D#owtA;z!k'
    b'8OE>c5yr3U3C6GM0LHKD|Guy5_r9;|@4m0==)SM(-@dQw*1oUn&c3hf$G)%YzP_*PwZ5<G'
    b't-i18rM|E0oxZQ@mA<d*jJ~hygubuqe7>*ibiS|aYre1RW4^EJTfVRBQ@*e3OTMq`LcXu-'
    b'I=-*#GQO|tD!#AlA-=Ec8NRRU5x%eM3BIrE0KTv5|Fy5{_qDI<@3pV%=(Vrw-?gvn*0rze'
    b'&b6=W$F;BPzO}FGwY9J7t+lV~rM0i?owcv)m9?+yjJ2=pgtf2he6_FZbhWSRYqhWIW3{jA'
    b'TeYw2Q?;+_OSP}-Lbb2!I<>FsGPSSkDz&fcA+@jT8MUwL5w)-D3AL~50JX2{|E;g<_pPt%'
    b'@2#)v=&i5o->t9f)~&DW&aJQO$E~mHzOAq8wXLt~t*x)?rLC{)ovp9ym94MqjIFQhgsrdZ'
    b'e66qRbgi%JYpt*AW38|2Tdl9_Q?0M-ORcZ#LandsI<2qkGOe%cDy^^UA+4|L8LhAD5v{N5'
    b'39YZ|0Ijd<|E90&_olDw@20Qo=%%mh-=?qY)~2uP&Ze*H$EL6AzNWA1wWhD@t){Q*rKYdz'
    b'ou;qrm8P%jjHa*agr=|Se5SAKbf&NCYo@R3W2Ud`Tc)q;Q>L%$OQx^uLZ+|lI;OAdGN!NV'
    b'DyFaNA*QeE8K$r65vH%}38t^>0H&|&|DCVv_noin@13vf=$)_Y-<_}P)}62G&YiF8$DOb1'
    b'zMZe@wVki)t(~vyrJb+qot>}im7TBajGeFRgq^SJe4VfBbe*s3Yn`v_W1X+-Tb-}#Q=PBt'
    b'OP#OlLY=ScI-RfUGM%sMDxI(EA)T-58J(}|5uLB=37xO&0G+Sv|CO)n_m!{f@0G9X=#{VQ'
    b'-<7ZH)|Id8&Xuq0$Ca<^zLl@*wUw{yt(C9qrIoMiot3Zam6fmSjFqqJgq5%Be3h^3bd|5`'
    b'Yn89-W0kM#Ta~ZtQ<bmlOO>zdLY1%UI+d^MGL^6EDwVJ6A(gM|8I`Z=5tXm&36-zw0F|%n'
    b'|BSEe_l&RW?~JeO=!~!H-;A&8){L*~&Wx|?$BeJ*zKpNywT!Rpt&FehrHrrZos6&Rm5i_J'
    b'jEt}Agp9B2e2lN_bd0a-YmBe!V~nrsTa2&kQ;e_cON_7ULX5BLI*hODGK{b5DvYn|A&jr<'
    b'8H}&%5sa_v35>7n0F1Be|AepW_k^$O?}V@G=!CE9--NI0)`YL?&V;Y)$AquzzJ#yqwS=$h'
    b't%R@ZrG&5RorJIJm4vVBjD)Z2goLl_e1xy-bcC<#YlN@sV}!5kTZFIcQ-rVUON6iMLWHmD'
    b'I)tz5GK8<|Dul1=A%w5%8HBIv5rnVn352if0EDmW|9r3O_k6GG?|iT8=zOp1-+Zs@)_kw)'
    b'&U~-y$9%8rzI?CiwS2GZt$eTRrF^gJoqVtBm3*)3jC`-_gnX~-e0;C#bbPPtYkaTkV|=gc'
    b'TYRtUQ+%)MOMI{ELVU05I()C|GJLP=Dtxc&A$+gv8GNtn5qz)f34E{X0DQ0O|8%eG_jIr8'
    b'?{u&0=yb2^-*m6*)^xAy&UCNq$8@jjzI3nawRErRt#q&JrF5_Bopi73m2|J`jC8N-gmka#'
    b'd~~ntbab!lYjm&cV|1_UTXe7MQ*^KEOLVX6LUga|I&`n=GIX!&Ds->wA#|_n8Fa7f5p=KX'
    b'33RXP0CcbG|7)-7_iL}~?`yB?=xeX*-)pby)@!fp&TFsh$7`?azH6`RwQH~It!uCArE9P2'
    b'oolb_m20o-jBBs!gln(sd~2`kbZf8cYiqCTV{5PLTWhcDQ){p5OKY#|LTj(<I%}`%GHb8v'
    b'Dr>LnA#1Pe8EdcW5o@pO32U$G0Bf)7|6{M~_hYZ??_;m)=wq+z-(#=q)?=^h&SS6Z$78SS'
    b'zGJWJwPUaAtz)n2rDLz_onx=-m1D2#jAO6sgk!Jkd}FWcbYrjUYh$nLV`H!DTVt>5Q)92|'
    b'OJlF=LSwJ%I%BWvGGnjnDr2wfA!D!W8Dp>O5o53G31hG80AsJ~|68x?_gk;)?_00y=v%Mr'
    b'-&?Qi)?2UZ&RehR$6K%KzFV*BwOg<2ty{0_rCYD-om;Q#m0Pdtj9ahkgj=ucd|R*UbX%|M'
    b'Yg@1DV_UE5TU)Q|Q(Ld=OIxq&LR+uvI$N*nGFz|fDqFAXAzQEO8C$RG5nHe830tr009&u?'
    b'|5LB)_fxOy?^Cbq=u@xj-&3#a)>E(R&Qq`J$5XHCzEiL3wNtO_ty8b-rBko#ol~#tl~b?l'
    b'j8m`cgj28Ud{eLMbW^YEYg4c5V^go|TT`#=Q&X?&OH;4wLQ}8nI#aLfGE=YXDpRlPAycpG'
    b'8B?$85mT@02~)4@08_8)|4Xmy_e-zq?@O=i=u5Bb-%GFS)=RJJ&P%WB$4js4zDuv`wM(z-'
    b'txK=#rAx2tolCFll}oSdj7zWUgiEjMd`qwEbW5-6YfG=|V@t2=TT8F&Q%kSwOG~foLQAjf'
    b'I!mwXGE1-PDod~HAxp388B4G05lgS@2}`f*086jy|3a_p_d>7h??SKZ=t8gS-$JkJ)<UoA'
    b'&O)#2$3m~`zCy3-wL-7!twOKsr9!XkokFkcl|rxUj6$#LghH?Dd_u45bV9G|YeKK<V?wX%'
    b'TSBkvQ$nxnOG2;fLPD?WIzq4OGD5HGDnhU8AwsX~8A7k?5kjx)2|};y079?p|2nVh_d2iZ'
    b'?>evR=sK_K-#V}B);h22&N{E_$2za;zB;e#wK}istvavkr8=+cojR}Ul{&BMj5@FDggUS5'
    b'd^)e|bULr=YdWv%V>++vTRN}nQ#!BfOFFOXLOQSOIy$fGGCHs8Dmt(0Av&+?89J})5jwBy'
    b'2|BOq06MSh|1z)Z_cE{R?=r9J=rXVC-!iZ3)-tc_&N8p-$1<<$zA~@twKA{ktun9cr82MU'
    b'oieZMl`^mEj54q5gfg$|d@`@=bTY5&Ycj9vV=}MnTQaZfQ!=mXOERzPLNc%GIx?^8GBU60'
    b'Dl)I@Au_M)88WZy5i+mq2{Nzi05Y%Z|0=KR_bRXJ?<%kB=qj)4-zu-`)+(>-&ML3#$11Pu'
    b'zACTlwJNXcttzkUr7ExMohq;El`606j4H3|getG=d@8T&bSkgwYbvknV=AxfTPm;XQ!20P'
    b'ODeDHLMpH8Ix4U0GAgg@Dk`t*Au6xy87i;q5h}0i2`aDa04lHR|01vJ_ad+B?;@}3=pwJ{'
    b'-y*N;)*`R#&LXet$0D!mz9O&dwIZ+Uts<}Mr6RBEog%O6l_Ia}j3Te=gd(r&d?K&wbRw_o'
    b'Ya*}fV<NBXTOzOPQzEbHOCqo9LL#s0IwG&@G9s_*Dk87zAtJBq86vOi5hAba2_moS03xsJ'
    b'{}`|9_ZYA1?-;M^=oqi--x#m!))=qr&KR%j#~82cz8J6TwHUAKtr)NCr5La4ofxm{l^Cz<'
    b'j2N%$gcz^ud>F6mbQrJeYZ$NVV;HaNTNtnFQy8!7OBk=~LKv^>IvB6(G8nJxDj2WpAsDag'
    b'85pnY5g4!Q2^g>I02r_9{}He2_Ytq_?-8%-=n=2$-x06t))BAk&JnNc#}TjVz7enMwGprD'
    b'tr4&5r4g^|oe{6=l@YJ&j1jNvgb}and=anfbP=!XYZ0&OV-c_GTM@78QxUK0OA)W@LJ_a)'
    b'IuWnyG7+!qDiN>iArY_Z84<7R5fQKJ2@$XB01>b2{|T?__X)4-?+LH#=n1du-wChl)(Nlc'
    b'&IzyU#|f|Nz6r1EwF$55tqHH|r3tU=oe8h&l?kuwj0vyngbA<fd<n1XbP2EPYYDIGV+pV8'
    b'TM4i0Qwgu@O9`**LJ6<yItj1qG6}EiDhaRaAqlVR840iJ5ecvB2??+3012<_{{XM+_W-Z!'
    b'?*Oms=m4+l-vF=c)&Q^T&H%6L#{jSEz5uW5wE(Z{tpKm<r2wz%odB=vl>o2ni~z6egaEJW'
    b'd;qWObO5jGYXGn7V*sz~TL7=?Qvk2)O8~FyLIAJpIsmWhG61jZDgdwRApo!I833>A5dg32'
    b'2>`F_006J+'
)


def segments(image):
    assert image[:2] == b'\xff\xd8'
    offset = 2
    while offset < len(image):
        assert image[offset] == 255
        marker = image[offset + 1]
        if marker == 0xda:
            yield marker, image[offset:]
            return
        size = struct.unpack_from('>H', image, offset + 2)[0]
        assert size >= 2 and offset + 2 + size <= len(image)
        yield marker, image[offset:offset + 2 + size]
        offset += size + 2
    raise ValueError('missing JPEG scan')


def rewrite(image, profile, adobe):
    output = image[:2]
    count = 0
    for marker, segment in segments(image):
        if marker == 0xe2 and segment[4:].startswith(b'ICC_PROFILE\0'):
            count += 1
            if profile is not None:
                payload = b'ICC_PROFILE\0\1\1' + profile
                output += b'\xff\xe2' + struct.pack('>H', len(payload) + 2) + payload
        elif marker != 0xee or adobe:
            output += segment
    assert count == 1
    return output


def coding(image):
    return b'\xff\xd8' + b''.join(segment for marker, segment in segments(image)
        if not 0xe0 <= marker <= 0xef)


def legacy_values(image, samples, inv):
    """Evaluate the historical incomplete mft2, not a fabricated valid ICC."""
    parts = [segment[18:] for marker, segment in segments(image)
             if marker == 0xe2 and segment[4:18] == b'ICC_PROFILE\0\1\1']
    assert len(parts) == 1
    profile = parts[0]
    assert len(profile) == 320
    assert profile[12:16] == bytes(4) and profile[36:40] == bytes(4)
    assert profile[16:24] == b'CMYKXYZ '
    assert struct.unpack_from('>I', profile, 128)[0] == 1
    tag, start, size = struct.unpack_from('>4sII', profile, 132)
    assert (tag, start, size) == (b'A2B0', 144, 176)
    lut = profile[start:start + size]
    assert lut[:12] == b'mft2' + bytes(4) + bytes([4, 3, 2, 0])
    assert lut[12:48] == bytes(36)
    assert struct.unpack_from('>HH', lut, 48) == (2, 2)
    assert lut[52:68] == struct.pack('>2H', 0, 65535) * 4
    assert lut[164:] == struct.pack('>2H', 0, 65535) * 3
    vertices = list(struct.iter_unpack('>3H', lut[68:164]))
    rows = []
    for sample in samples:
        unit = [D(255 - v) / 255 for v in sample]
        xyz = [D(0)] * 3
        for bits, vertex in zip(itertools.product((0, 1), repeat=4), vertices):
            weight = D(1)
            for channel, bit in enumerate(bits):
                weight *= unit[channel] if bit else 1 - unit[channel]
            for channel in range(3):
                xyz[channel] += weight * vertex[channel] / 32768
        rows.append([min(D(1), max(D(0), v)) for v in apply(inv, xyz)])
    return profile, rows


def exact_sixel(rows):
    """Encode independently rounded RGB101 values with no palette reducer."""
    pixels = [tuple(int(srgb(v) * 100 + D('.5')) for v in row) for row in rows]
    palette = list(dict.fromkeys(pixels))
    assert len(pixels) == 1024 and len(palette) <= 256
    output = '\x1bP0;0;0q"1;1;32;32'
    for index, rgb in enumerate(palette):
        output += f'#{index};2;' + ';'.join(map(str, rgb))
    for y in range(0, 32, 6):
        if y:
            output += '-'
        colors = [rgb for rgb in palette if rgb in pixels[y * 32:(y + 6) * 32]]
        for plane, rgb in enumerate(colors):
            if plane:
                output += '$'
            output += '#' + str(palette.index(rgb))
            for x in range(32):
                bits = sum(1 << dy for dy in range(min(6, 32 - y))
                           if pixels[(y + dy) * 32 + x] == rgb)
                output += chr(63 + bits)
    return (output + '\x1b\\').encode('ascii')


def build(profile, record_bytes):
    with localcontext() as context:
        context.prec = 80
        return values(profile, json.loads(record_bytes))


def values(profile, record):
    image = (ROOT / SOURCE).read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    assert digest == '6f0a6de6d0194c50907ef0b9ccf0e1bca8046f99a697139e4965d5fe9f0ae536'
    assert len(RAW) == 32 * 32 * 4
    samples = list(map(list, struct.iter_unpack('4B', RAW)))
    inv = inverse(destination_matrix())
    outputs = {}
    # A valid input profile need not contain a relative-colorimetric slot.
    # Keep A2B0, rename only A2B1 to A2B2; strict relative apply must fail in
    # the builtin facade after a transform has actually been created.
    missing = bytearray(profile)
    count = struct.unpack_from('>I', missing, 128)[0]
    matches = [i for i in range(132, 132 + count * 12, 12)
               if missing[i:i + 4] == b'A2B1']
    assert len(matches) == 1
    missing[matches[0]:matches[0] + 4] = b'A2B2'
    linear = []
    for adobe in (0, 1):
        suffix = 'adobe' if adobe else 'plain'
        for kind, icc in [('affine', profile), ('absent', None),
                          ('rejected', b'not an ICC profile'),
                          ('missing-intent', bytes(missing))]:
            name = f'jpeg-cmyk-{kind}-{suffix}.jpg'
            outputs[name] = rewrite(image, icc, adobe)
            assert coding(outputs[name]) == coding(image)
        rows = []
        for sample in samples:
            units = [D(255 - v if adobe else v) / 255 for v in sample]
            xyz = [(D(record['base_u1_15'][j]) - sum(
                units[k] * record['weights_u1_15'][k][j] for k in range(4)))
                / 32768 for j in range(3)]
            rows.append(apply(inv, xyz))
        linear.append(rows)
        pixels = bytes(int(min(D(1), max(D(0), srgb(v))) * 255 + D('.5'))
                       for row in rows for v in row)
        outputs[f'jpeg-cmyk-{suffix}-reference.png'] = png_rgb8(32, 32, pixels)
    # The established non-ICC fallback is a separate compatibility oracle,
    # not a claim that this product approximates a physical CMYK device.
    gamma = [[D(v) * sample[3] / (255 * 255) for v in sample[:3]]
             for sample in samples]
    decode = lambda v: (v / D('12.92') if v <= D('.04045') else
                         ((v + D('.055')) / D('1.055')) ** D('2.4'))
    fallback = [gamma, [[decode(v) for v in row] for row in gamma]]
    header = '/* Generated independent device samples and Decimal values. */\n'
    header += array('jc_raw', 'unsigned char', samples)
    header += array('jc_linear', 'double', linear)
    header += array('jc_fallback', 'double', fallback)
    legacy_profile, legacy = legacy_values(image, samples, inv)
    header += array('jc_legacy', 'double', legacy)
    outputs['../colormgmt/reference/custom/cmyk_mft2_valid_builtin.six'] = exact_sixel(legacy)
    header += array('jc_pcs_bound', 'double', record['lut_precision_budget']['linear_rgb'])
    header += 'static double const jc_rounding = 64.0 / 16777216.0;\n'
    outputs['jpeg-cmyk.h'] = header.encode()
    manifest = dict(generator='tools/generate_jpeg_cmyk_tests.py', input=SOURCE,
        input_sha256=digest, coding_sha256=hashlib.sha256(coding(image)).hexdigest(),
        raw_sha256=hashlib.sha256(RAW).hexdigest(), width=32, height=32,
        source_probe='tools/verify_jpeg_cmyk_source.c',
        source_decoder='libjpeg-turbo 3.2.0; JCS_CMYK; JDCT_ISLOW',
        arithmetic='80-digit Decimal; affine PCS and independently derived D50 sRGB inverse',
        profile_sha256=hashlib.sha256(profile).hexdigest(),
        marker='Adobe APP14 transform 0; only APP2 replacement or APP14 removal',
        bounds=dict(rounding='64u32 relative for float evaluation and sRGB libm roundtrip',
            external_PCS='additional two u1.15 PCS bins propagated through inverse D50 sRGB',
            external_linear=record['lut_precision_budget']['linear_rgb']),
        fallback='Historical c*k, m*k, y*k; CMS-off gamma, CMS-on sRGB EOTF',
        legacy_profile=dict(sha256=hashlib.sha256(legacy_profile).hexdigest(),
            status='Historical incomplete ICC, builtin default legacy acceptance only',
            reference='Direct RGB101 SIXEL; Decimal mft2 and Adobe complement; no CMM or encoder',
            clipping='Existing bounded legacy transform; not the affine profile contract'),
        missing_intent='Complete profile with A2B1 renamed to A2B2; A2B0 unchanged',
        files={name: hashlib.sha256(data).hexdigest() for name, data in outputs.items()})
    outputs['jpeg-cmyk.json'] = (json.dumps(manifest, indent=2) + '\n').encode()
    return outputs
