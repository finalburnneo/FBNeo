#ifndef OGLVIEWPORT_H
#define OGLVIEWPORT_H

#include <QGLWidget>
#include <QWidget>

class OGLViewport : public QGLWidget
{
    Q_OBJECT

public:
    OGLViewport(QWidget* parent);
    ~OGLViewport();

    virtual void initializeGL();
    virtual void paintGL();
    virtual void resizeGL(int w, int h);
};

#endif // OGLVIEWPORT_H
