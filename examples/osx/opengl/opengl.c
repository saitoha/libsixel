#if !defined(__APPLE__) || !defined(__MACH__)
# error "Now this program works on only OSX"
#endif
# include <OpenGL/gl.h>
# include <OpenGL/glu.h>
# include <OpenGL/OpenGL.h>
#include <sys/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sixel.h>

static CGLPBufferObj pbuffer;
static CGLContextObj context;
static volatile int signaled = 0;

static void handler(int sig)
{
    signaled = sig;
}

static CGLError setup(int width, int height)
{
    CGLPixelFormatAttribute pfattr[] = { kCGLPFAPBuffer, (CGLPixelFormatAttribute)0 };
    CGLPixelFormatObj pixformat;
    GLint npixels;
    CGLError e;

    e = CGLChoosePixelFormat(pfattr, &pixformat, &npixels);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLChoosePixelFormat failed, err %d\n", e);
       return e;
    }
    e = CGLCreateContext(pixformat, 0, &context);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLChoosePixelFormat failed, err %d\n", e);
       return e;
    }
    e = CGLDestroyPixelFormat(pixformat);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLDestroyPixelFormat failed, err %d\n", e);
       return e;
    }
    e = CGLSetCurrentContext(context);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLSetCurrentContext failed, err %d\n", e);
       return e;
    }
    e = CGLCreatePBuffer(width, height, GL_TEXTURE_2D, GL_RGB, 0, &pbuffer);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLCreatePBuffer failed, err %d\n", e);
       return e;
    }
    e = CGLSetPBuffer(context, pbuffer, 0, 0, 0);
    if (e != kCGLNoError) {
       fprintf(stderr, "CGLSetPBuffer failed, err %d\n", e);
       return e;
    }
    return kCGLNoError;
}

static CGLError cleanup()
{
    (void)CGLDestroyContext(context);
    (void)CGLDestroyPBuffer(pbuffer);
    return kCGLNoError;
}

static int draw_scene()
{
    static GLfloat rot1, rot2;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glTranslatef(-1.5, 0, -6);
    glRotatef(rot1 += 1.5, 0, 1, 0);
    glBegin(GL_TRIANGLES);
      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  1,  0); glVertex3f(-1, -1,  1);
      glColor3f(0,  0,  1); glVertex3f(1, -1,  1);

      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  0,  1); glVertex3f(1, -1,  1);
      glColor3f(0,  1,  0); glVertex3f(1, -1, -1);

      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  1,  0); glVertex3f(1, -1, -1);
      glColor3f(0,  0,  1); glVertex3f(-1, -1, -1);

      glColor3f(1,  0,  0); glVertex3f(0,  1,  0);
      glColor3f(0,  0,  1); glVertex3f(-1, -1, -1);
      glColor3f(0,  1,  0); glVertex3f(-1, -1,  1);
    glEnd();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(1.5, 0, -6);
    glRotatef(rot2 += 1.8, 1, 0, 0);
    glBegin(GL_QUADS);
      glColor3f(0, 0, 0); glVertex3f(-1, -1, -1);
      glColor3f(0, 0, 1); glVertex3f(-1, -1,  1);
      glColor3f(0, 1, 1); glVertex3f(-1,  1,  1);
      glColor3f(0, 1, 0); glVertex3f(-1,  1, -1);

      glColor3f(1, 0, 0); glVertex3f( 1, -1, -1);
      glColor3f(1, 0, 1); glVertex3f( 1, -1,  1);
      glColor3f(1, 1, 1); glVertex3f( 1,  1,  1);
      glColor3f(1, 1, 0); glVertex3f( 1,  1, -1);

      glColor3f(0, 0, 0); glVertex3f(-1, -1, -1);
      glColor3f(0, 0, 1); glVertex3f(-1, -1,  1);
      glColor3f(1, 0, 1); glVertex3f( 1, -1,  1);
      glColor3f(1, 0, 0); glVertex3f( 1, -1, -1);

      glColor3f(0, 1, 0); glVertex3f(-1,  1, -1);
      glColor3f(0, 1, 1); glVertex3f(-1,  1,  1);
      glColor3f(1, 1, 1); glVertex3f( 1,  1,  1);
      glColor3f(1, 1, 0); glVertex3f( 1,  1, -1);

      glColor3f(0, 0, 0); glVertex3f(-1, -1, -1);
      glColor3f(0, 1, 0); glVertex3f(-1,  1, -1);
      glColor3f(1, 1, 0); glVertex3f( 1,  1, -1);
      glColor3f(1, 0, 0); glVertex3f( 1, -1, -1);

      glColor3f(0, 0, 1); glVertex3f(-1, -1,  1);
      glColor3f(0, 1, 1); glVertex3f(-1,  1,  1);
      glColor3f(1, 1, 1); glVertex3f( 1,  1,  1);
      glColor3f(1, 0, 1); glVertex3f( 1, -1,  1);
    glEnd();
    glPopMatrix();

    return 0;
}

static int output_sixel(unsigned char *pixbuf, int width, int height, int ncolors, int depth)
{
    LSImagePtr im;
    LSOutputContextPtr context;
    unsigned char *palette;
    int i;

    im = LSImage_create(width, height, 1, ncolors);
    palette = LSQ_MakePalette(pixbuf, width, height, depth, ncolors, &ncolors, NULL,
                              LARGE_NORM, REP_CENTER_BOX, QUALITY_HIGH);
    for (i = 0; i < ncolors; i++)
        LSImage_setpalette(im, i, palette[i * 3], palette[i * 3 + 1], palette[i * 3 + 2]);
    LSQ_ApplyPalette(pixbuf, width, height, depth,
                     palette, ncolors, DIFFUSE_FS, 1, NULL, im->pixels);

    context = LSOutputContext_create(putchar, printf);
    LibSixel_LSImageToSixel(im, context);
    LSQ_FreePalette(palette);
    LSOutputContext_destroy(context);
    LSImage_destroy(im);

    return 0;
}

int main(int argc, char** argv)
{
    int width = 400;
    int height = 300;
    int ncolors = 16;

    static char *pixbuf;

    if (signal(SIGINT, handler) == SIG_ERR)
       return (-1);
    if (setup(width, height) != kCGLNoError)
       return (-1);

    glShadeModel(GL_SMOOTH);
    glClearColor(0, 0, 0, 0);
    glClearDepth(1);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45, (GLfloat)width / (GLfloat)height, 0.1, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    pixbuf = malloc(width * height * 4);

    while (!signaled) {
        glLoadIdentity();
        glPushMatrix();
        glScalef(1, -1, 1);
        draw_scene();
        glPopMatrix();
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixbuf);

        if (signaled)
            break;

        printf("\e[3;3H");
        output_sixel((unsigned char *)pixbuf, width, height, ncolors, 3);
    }

    printf("\e\\");
    free(pixbuf);

    if (cleanup() != kCGLNoError)
       return (-1);
    return 0;
}

/* EOF */
