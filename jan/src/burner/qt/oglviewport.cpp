#include <GL/glew.h>
#include <QDebug>
#include <QOpenGLContext>
#include "oglviewport.h"

extern void OGLSetContext(void *ptr);
extern void OGLSetSwapBuffers(void (*f)(void*));
extern void OGLSetMakeCurrent(void (*f)(void*));
extern void OGLSetDoneCurrent(void (*f)(void*));
extern void OGLResize(int w, int h);

static void swapBuffers(void *ptr)
{
    OGLViewport* view = static_cast<OGLViewport*>(ptr);
    view->swapBuffers();
}

static void doneCurrent(void *ptr)
{
    OGLViewport* view = static_cast<OGLViewport*>(ptr);
    view->doneCurrent();
}

static void makeCurrent(void *ptr)
{
    OGLViewport* view = static_cast<OGLViewport*>(ptr);
    view->makeCurrent();
}

OGLViewport::OGLViewport(QWidget *parent) :
    QGLWidget(parent)
{
    QGLFormat format;
    format.setVersion(3, 3);
    format.setProfile(QGLFormat::CoreProfile);
    setFormat(format);

    OGLSetContext(this);
    OGLSetSwapBuffers(::swapBuffers);
    OGLSetMakeCurrent(::makeCurrent);
    OGLSetDoneCurrent(::doneCurrent);
}

OGLViewport::~OGLViewport()
{

}

void OGLViewport::initializeGL()
{
}

void OGLViewport::paintGL()
{
}

void OGLViewport::resizeGL(int w, int h)
{
    OGLResize(w, h);
}

