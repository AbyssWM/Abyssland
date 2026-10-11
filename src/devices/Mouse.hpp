#pragma once

#include "IPointer.hpp"

class CMouse : public IPointer {
  public:
    static SP<CMouse>                create(SP<Abyssal::IPointer> mouse);

    virtual bool                     isVirtual();
    virtual SP<Abyssal::IPointer> aq();

  private:
    CMouse(SP<Abyssal::IPointer> mouse);

    WP<Abyssal::IPointer> m_mouse;

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
