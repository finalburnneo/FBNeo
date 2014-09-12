#ifndef QRUBYVIEWPORT_H
#define QRUBYVIEWPORT_H

#include <QWidget>
#include <nall/traits.hpp>

class QRubyViewport : public QWidget
{
    Q_OBJECT
    explicit QRubyViewport(QWidget *parent = 0);
    static QRubyViewport *m_onlyInstance;
public:
    ~QRubyViewport();
    static QRubyViewport *get(QWidget *parent = nullptr);
    uintptr_t id();
protected:
    virtual QPaintEngine *paintEngine() const;
    virtual void paintEvent(QPaintEvent *);
};

#endif // QRUBYVIEWPORT_H
