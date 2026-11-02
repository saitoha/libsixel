# SHA-256 on byte strings using exact integer arithmetic available in POSIX awk.
# Only reported exceptions need a digest; the ordinary clean scan hashes nothing.
function bitand(a, b,    value, bit, i) {
    value = 0; bit = 1
    for (i = 0; i < 32; i++) {
        if (a % 2 && b % 2) value += bit
        a = int(a / 2); b = int(b / 2); bit *= 2
    }
    return value
}
function bitxor(a, b) { return a + b - 2 * bitand(a, b) }
function xor3(a, b, c) { return bitxor(bitxor(a, b), c) }
function rotate(a, n,    power) {
    power = 2 ^ n
    return int(a / power) + (a % power) * (2 ^ (32 - n))
}
function hexword(value,    result, i, digit) {
    result = ""
    for (i = 0; i < 8; i++) {
        digit = value % 16; value = int(value / 16)
        result = substr("0123456789abcdef", digit + 1, 1) result
    }
    return result
}
function sha256(text,    bytes, size, total, i, j, offset, w, h, a, b, c, d, e, f, g, v, s0, s1, t1, t2, result) {
    size = length(text)
    for (i = 1; i <= size; i++) bytes[i - 1] = byte_value[substr(text, i, 1)]
    bytes[size] = 128
    total = int((size + 9 + 63) / 64) * 64
    for (i = size + 1; i < total; i++) bytes[i] = 0
    v = size * 8
    for (i = total - 1; i >= total - 8; i--) { bytes[i] = v % 256; v = int(v / 256) }
    split("1779033703 3144134277 1013904242 2773480762 1359893119 2600822924 528734635 1541459225", h, " ")
    for (offset = 0; offset < total; offset += 64) {
        for (i = 0; i < 16; i++) {
            w[i] = 0
            for (j = 0; j < 4; j++) w[i] = w[i] * 256 + bytes[offset + i * 4 + j]
        }
        for (i = 16; i < 64; i++) {
            s0 = xor3(rotate(w[i - 15], 7), rotate(w[i - 15], 18), int(w[i - 15] / 8))
            s1 = xor3(rotate(w[i - 2], 17), rotate(w[i - 2], 19), int(w[i - 2] / 1024))
            w[i] = (w[i - 16] + s0 + w[i - 7] + s1) % 4294967296
        }
        a = h[1]; b = h[2]; c = h[3]; d = h[4]; e = h[5]; f = h[6]; g = h[7]; v = h[8]
        for (i = 0; i < 64; i++) {
            s1 = xor3(rotate(e, 6), rotate(e, 11), rotate(e, 25))
            t1 = (v + s1 + bitxor(bitand(e, f), bitand(4294967295 - e, g)) + sha_k[i + 1] + w[i]) % 4294967296
            s0 = xor3(rotate(a, 2), rotate(a, 13), rotate(a, 22))
            t2 = (s0 + xor3(bitand(a, b), bitand(a, c), bitand(b, c))) % 4294967296
            v = g; g = f; f = e; e = (d + t1) % 4294967296
            d = c; c = b; b = a; a = (t1 + t2) % 4294967296
        }
        h[1] = (h[1] + a) % 4294967296; h[2] = (h[2] + b) % 4294967296
        h[3] = (h[3] + c) % 4294967296; h[4] = (h[4] + d) % 4294967296
        h[5] = (h[5] + e) % 4294967296; h[6] = (h[6] + f) % 4294967296
        h[7] = (h[7] + g) % 4294967296; h[8] = (h[8] + v) % 4294967296
    }
    for (i = 1; i <= 8; i++) result = result hexword(h[i])
    return result
}
BEGIN {
    for (sha_i = 0; sha_i < 256; sha_i++) byte_value[sprintf("%c", sha_i)] = sha_i
    split("1116352408 1899447441 3049323471 3921009573 961987163 1508970993 2453635748 2870763221 3624381080 310598401 607225278 1426881987 1925078388 2162078206 2614888103 3248222580 3835390401 4022224774 264347078 604807628 770255983 1249150122 1555081692 1996064986 2554220882 2821834349 2952996808 3210313671 3336571891 3584528711 113926993 338241895 666307205 773529912 1294757372 1396182291 1695183700 1986661051 2177026350 2456956037 2730485921 2820302411 3259730800 3345764771 3516065817 3600352804 4094571909 275423344 430227734 506948616 659060556 883997877 958139571 1322822218 1537002063 1747873779 1955562222 2024104815 2227730452 2361852424 2428436474 2756734187 3204031479 3329325298", sha_k, " ")
    if (sha256("") != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ||
        sha256("abc") != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" ||
        sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") != "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") {
        print "# SHA-256 known-answer control failed"; failed = 1
    }
}
