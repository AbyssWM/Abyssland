#pragma once

#include "ITouch.hpp"

class CTouchDevice : public ITouch {
  public:
    static SP<CTouchDevice>        create(SP<Abyssal::ITouch> touch);

    virtual bool                   isVirtual();
    virtual SP<Abyssal::ITouch> aq();

  private:
    CTouchDevice(SP<Abyssal::ITouch> touch);

    WP<Abyssal::ITouch> m_touch;

    struct {
        CAbyssSignalListener destroy;
        CAbyssSignalListener down;
        CAbyssSignalListener up;
        CAbyssSignalListener motion;
        CAbyssSignalListener cancel;
        CAbyssSignalListener frame;
    } m_listeners;
};