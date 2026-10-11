#pragma once

#include "IPointer.hpp"

class CVirtualPointerV1Resource;

class CVirtualPointer : public IPointer {
  public:
    static SP<CVirtualPointer>       create(SP<CVirtualPointerV1Resource> resource);

    virtual bool                     isVirtual();
    virtual SP<Abyssal::IPointer> aq();

  private:
    CVirtualPointer(SP<CVirtualPointerV1Resource>);

    WP<CVirtualPointerV1Resource> m_pointer;

    struct {
        CAbyssSignalListener destroy;

        CAbyssSignalListener motion;
        CAbyssSignalListener motionAbsolute;
        CAbyssSignalListener button;
        CAbyssSignalListener axis;
        CAbyssSignalListener frame;

        CAbyssSignalListener swipeBegin;
        CAbyssSignalListener swipeEnd;
        CAbyssSignalListener swipeUpdate;

        CAbyssSignalListener pinchBegin;
        CAbyssSignalListener pinchEnd;
        CAbyssSignalListener pinchUpdate;

        CAbyssSignalListener holdBegin;
        CAbyssSignalListener holdEnd;
    } m_listeners;
};