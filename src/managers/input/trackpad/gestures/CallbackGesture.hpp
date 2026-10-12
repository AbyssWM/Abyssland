#pragma once

#include <functional>

#include <abyss/config.hpp>

#include "ITrackpadGesture.hpp"

// a gesture whose phases call back into the compiled user config (Config::gesture with a function or GestureCallbacks)
class CCallbackTrackpadGesture : public ITrackpadGesture {
  public:
    using Fn = std::function<void(const abyss::GestureEvent&)>;

    CCallbackTrackpadGesture(Fn start, Fn update, Fn end);
    virtual ~CCallbackTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    Fn m_start, m_update, m_end;
};
