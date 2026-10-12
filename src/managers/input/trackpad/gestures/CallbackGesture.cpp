#include "CallbackGesture.hpp"

#include "../TrackpadGestures.hpp"

static std::string dirString(eTrackpadGestureDirection dir) {
    return dir == TRACKPAD_GESTURE_DIR_NONE ? "NONE" : g_pTrackpadGestures->stringForDir(dir);
}

template <typename E>
static abyss::GestureEvent toEvent(abyss::GestureEvent::Phase phase, const E& e) {
    abyss::GestureEvent ev;
    ev.phase     = phase;
    ev.direction = dirString(e.direction);
    if (e.swipe) {
        ev.type    = "swipe";
        ev.timeMs  = e.swipe->timeMs;
        ev.fingers = sc<int>(e.swipe->fingers);
        ev.delta   = {sc<float>(e.swipe->delta.x), sc<float>(e.swipe->delta.y)};
    } else if (e.pinch) {
        ev.type     = "pinch";
        ev.timeMs   = e.pinch->timeMs;
        ev.fingers  = sc<int>(e.pinch->fingers);
        ev.delta    = {sc<float>(e.pinch->delta.x), sc<float>(e.pinch->delta.y)};
        ev.scale    = e.pinch->scale;
        ev.rotation = e.pinch->rotation;
    }
    return ev;
}

static abyss::GestureEvent toEndEvent(const ITrackpadGesture::STrackpadGestureEnd& e) {
    abyss::GestureEvent ev;
    ev.phase     = abyss::GestureEvent::Phase::End;
    ev.direction = dirString(e.direction);
    if (e.swipe) {
        ev.type      = "swipe";
        ev.timeMs    = e.swipe->timeMs;
        ev.cancelled = e.swipe->cancelled;
    } else if (e.pinch) {
        ev.type      = "pinch";
        ev.timeMs    = e.pinch->timeMs;
        ev.cancelled = e.pinch->cancelled;
    }
    return ev;
}

CCallbackTrackpadGesture::CCallbackTrackpadGesture(Fn start, Fn update, Fn end) : m_start(std::move(start)), m_update(std::move(update)), m_end(std::move(end)) {
    ;
}

void CCallbackTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e); // sets m_scale

    if (m_start)
        m_start(toEvent(abyss::GestureEvent::Phase::Start, e));
}

void CCallbackTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (m_update)
        m_update(toEvent(abyss::GestureEvent::Phase::Update, e));
}

void CCallbackTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (m_end)
        m_end(toEndEvent(e));
}
