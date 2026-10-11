#pragma once

#include "IKeyboard.hpp"

class CKeyboard : public IKeyboard {
  public:
    static SP<CKeyboard>              create(SP<Abyssal::IKeyboard> keeb);

    virtual bool                      isVirtual();
    virtual SP<Abyssal::IKeyboard> aq();

  private:
    CKeyboard(SP<Abyssal::IKeyboard> keeb);

    WP<Abyssal::IKeyboard> m_keyboard;

    struct {
        CAbyssSignalListener destroy;
        CAbyssSignalListener key;
        CAbyssSignalListener modifiers;
    } m_listeners;
};