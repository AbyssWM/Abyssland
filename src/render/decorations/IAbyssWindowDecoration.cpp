#include "IAbyssWindowDecoration.hpp"

IAbyssWindowDecoration::IAbyssWindowDecoration(PHLWINDOW pWindow) : m_window(pWindow) {
    ;
}

bool IAbyssWindowDecoration::onInputOnDeco(const eInputType, const Vector2D&, std::any) {
    return false;
}

eDecorationLayer IAbyssWindowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t IAbyssWindowDecoration::getDecorationFlags() {
    return 0;
}

std::string IAbyssWindowDecoration::getDisplayName() {
    return "Unknown Decoration";
}
