## The high-level conversion API

The high-livel API provides File-to-File conversion features.

### Encoder

The sixel encoder object and related functions provides almost same features as `img2sixel`.

```C
/* create encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_new(
    sixel_encoder_t     /* out */ **ppencoder, /* encoder object to be created */
    sixel_allocator_t   /* in */  *allocator); /* allocator, null if you use
                                                  default allocator */

/* increase reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_ref(sixel_encoder_t /* in */ *encoder);

/* decrease reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_unref(sixel_encoder_t /* in */ *encoder);

/* set cancel state flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_set_cancel_flag(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ *cancel_flag);

/* set an option flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *optarg);

/* load source data from specified file and encode it to SIXEL format */
SIXELAPI SIXELSTATUS
sixel_encoder_encode(
    sixel_encoder_t /* in */ *encoder,
    char const      /* in */ *filename);
```

### Decoder

The sixel decoder object and related functions provides almost same features as `sixel2png`.

```C
/* create decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_new(
    sixel_decoder_t    /* out */ **ppdecoder,  /* decoder object to be created */
    sixel_allocator_t  /* in */  *allocator);  /* allocator, null if you use
                                                  default allocator */

/* increase reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_ref(sixel_decoder_t *decoder);

/* decrease reference count of decoder object (thread-unsafe) */
SIXELAPI void
sixel_decoder_unref(sixel_decoder_t *decoder);

/* set an option flag to decoder object */
SIXELAPI SIXELSTATUS
sixel_decoder_setopt(
    sixel_decoder_t /* in */ *decoder,  /* decoder object */
    int             /* in */ arg,       /* one of SIXEL_OPTFLAG_*** */
    char const      /* in */ *optarg);  /* null or an argument of optflag */

/* load source data from stdin or the file specified with
   SIXEL_OPTFLAG_INPUT flag, and decode it */
SIXELAPI SIXELSTATUS
sixel_decoder_decode(
    sixel_decoder_t /* in */ *decoder);
```


## The low-level conversion API

The low-level API provides bytes-to-bytes conversion features.

The whole API is described [in its header](https://github.com/libsixel/libsixel/blob/master/include/sixel.h.in).

### Bitmap to SIXEL

`sixel_encode` function converts bitmap array into SIXEL format.

```C
/* convert pixels into sixel format and write it to output context */
SIXELAPI SIXELSTATUS
sixel_encode(
    unsigned char  /* in */ *pixels,     /* pixel bytes */
    int            /* in */  width,      /* image width */
    int            /* in */  height,     /* image height */
    int            /* in */  depth,      /* color depth: now unused */
    sixel_dither_t /* in */ *dither,     /* dither context */
    sixel_output_t /* in */ *context);   /* output context */
```
To use this function, you have to initialize two objects,

- `sixel_dither_t` (dithering context object)
- `sixel_output_t` (output context object)

#### Dithering context

Here is a part of APIs for dithering context manipulation.

```C
/* create dither context object */
SIXELAPI SIXELSTATUS
sixel_dither_new(
    sixel_dither_t      /* out */   **ppdither,  /* dither object to be created */
    int                 /* in */    ncolors,     /* required colors */
    sixel_allocator_t   /* in */    *allocator); /* allocator, null if you use
                                                    default allocator */

/* get built-in dither context object */
SIXELAPI sixel_dither_t *
sixel_dither_get(int builtin_dither); /* ID of built-in dither object */

/* destroy dither context object */
SIXELAPI void
sixel_dither_destroy(sixel_dither_t *dither); /* dither context object */

/* increase reference count of dither context object (thread-unsafe) */
SIXELAPI void
sixel_dither_ref(sixel_dither_t *dither); /* dither context object */

/* decrease reference count of dither context object (thread-unsafe) */
SIXELAPI void
sixel_dither_unref(sixel_dither_t *dither); /* dither context object */

/* initialize internal palette from specified pixel buffer */
SIXELAPI SIXELSTATUS
sixel_dither_initialize(
    sixel_dither_t *dither,          /* dither context object */
    unsigned char /* in */ *data,    /* sample image */
    int /* in */ width,              /* image width */
    int /* in */ height,             /* image height */
    int /* in */ pixelformat,        /* one of enum pixelFormat */
    int /* in */ method_for_largest, /* method for finding the largest dimension */
    int /* in */ method_for_rep,     /* method for choosing a color from the box */
    int /* in */ quality_mode);      /* quality of histogram processing */

/* set diffusion type, choose from enum methodForDiffuse */
SIXELAPI void
sixel_dither_set_diffusion_type(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int /* in */ method_for_diffuse);  /* one of enum methodForDiffuse */

/* get number of palette colors */
SIXELAPI int
sixel_dither_get_num_of_palette_colors(
    sixel_dither_t /* in */ *dither);  /* dither context object */

/* get number of histogram colors */
SIXELAPI int
sixel_dither_get_num_of_histogram_colors(
    sixel_dither_t /* in */ *dither);  /* dither context object */

/* get palette */
SIXELAPI unsigned char *
sixel_dither_get_palette(
    sixel_dither_t /* in */ *dither);  /* dither context object */

/* set palette */
SIXELAPI void
sixel_dither_set_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    unsigned char  /* in */ *palette);

SIXELAPI void
sixel_dither_set_complexion_score(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ score);    /* complexion score (>= 1) */

SIXELAPI void
sixel_dither_set_body_only(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ bodyonly); /* 0: output palette section(default)
                                          1: do not output palette section */
SIXELAPI void
sixel_dither_set_optimize_palette(
    sixel_dither_t /* in */ *dither,   /* dither context object */
    int            /* in */ do_opt);   /* 0: optimize palette size
                                          1: don't optimize palette size */
/* set pixelformat */
SIXELAPI void
sixel_dither_set_pixelformat(
    sixel_dither_t /* in */ *dither,      /* dither context object */
    int            /* in */ pixelformat); /* one of enum pixelFormat */

/* set transparent */
SIXELAPI void
sixel_dither_set_transparent(
    sixel_dither_t /* in */ *dither,      /* dither context object */
    int            /* in */ transparent); /* transparent color index */
```

#### Output context

Here is a part of APIs for output context manipulation.

```C
/* create output context object */
SIXELAPI SIXELSTATUS
sixel_output_new(
    sixel_output_t          /* out */ **output,     /* output object to be created */
    sixel_write_function    /* in */  fn_write,     /* callback for output sixel */
    void                    /* in */ *priv,         /* private data given as
                                                       3rd argument of fn_write */
    sixel_allocator_t       /* in */  *allocator);  /* allocator, null if you use
                                                       default allocator */

/* destroy output context object */
SIXELAPI void
sixel_output_destroy(sixel_output_t /* in */ *output); /* output context */

/* increase reference count of output context object (thread-unsafe) */
SIXELAPI void
sixel_output_ref(sixel_output_t /* in */ *output);     /* output context */

/* decrease reference count of output context object (thread-unsafe) */
SIXELAPI void
sixel_output_unref(sixel_output_t /* in */ *output);   /* output context */

/* set 8bit output mode which indicates whether it uses C1 control characters */
SIXELAPI int
sixel_output_get_8bit_availability(
    sixel_output_t /* in */ *output);   /* output context */

/* get 8bit output mode state */
SIXELAPI void
sixel_output_set_8bit_availability(
    sixel_output_t /* in */ *output,       /* output context */
    int            /* in */ availability); /* 0: do not use 8bit characters
                                              1: use 8bit characters */

/* set GNU Screen penetration feature enable or disable */
SIXELAPI void
sixel_output_set_penetrate_multiplexer(
    sixel_output_t /* in */ *output,    /* output context */
    int            /* in */ penetrate); /* 0: penetrate GNU Screen
                                           1: do not penetrate GNU Screen */

/* set whether we skip DCS envelope */
SIXELAPI void
sixel_output_set_skip_dcs_envelope(
    sixel_output_t /* in */ *output,   /* output context */
    int            /* in */ skip);     /* 0: output DCS envelope
                                          1: do not output DCS envelope */

SIXELAPI void
sixel_output_set_palette_type(
    sixel_output_t /* in */ *output,      /* output context */
    int            /* in */ palettetype); /* PALETTETYPE_RGB: RGB palette
                                             PALETTETYPE_HLS: HLS palette */

SIXELAPI void
sixel_output_set_encode_policy(
    sixel_output_t /* in */ *output,    /* output context */
    int            /* in */ encode_policy);
```

### SIXEL to indexed bitmap

`sixel_decode` function converts SIXEL into indexed bitmap bytes with its palette.

```
/* convert sixel data into indexed pixel bytes and palette data */
SIXELAPI SIXELSTATUS
sixel_decode_raw(
    unsigned char       /* in */  *p,           /* sixel bytes */
    int                 /* in */  len,          /* size of sixel bytes */
    unsigned char       /* out */ **pixels,     /* decoded pixels */
    int                 /* out */ *pwidth,      /* image width */
    int                 /* out */ *pheight,     /* image height */
    unsigned char       /* out */ **palette,    /* ARGB palette */
    int                 /* out */ *ncolors,     /* palette size (<= 256) */
    sixel_allocator_t   /* in */  *allocator);  /* allocator object */
```

