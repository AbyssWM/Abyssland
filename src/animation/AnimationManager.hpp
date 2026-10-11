#pragma once

#include <abyssutils/animation/AnimationManager.hpp>
#include <abyssutils/animation/AnimatedVariable.hpp>

#include "../defines.hpp"
#include "../helpers/AnimatedVariable.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/time/Timer.hpp"
#include "../managers/eventLoop/EventLoopTimer.hpp"

namespace Animation {

    class CAbyssAnimationManager : public Abyssutils::Animation::CAnimationManager {
      public:
        CAbyssAnimationManager();

        void         tick();
        void         frameTick();
        virtual void scheduleTick();
        virtual void onTicked();

        // Reset tick state after session changes (suspend/wake, lock/unlock)
        void resetTickState();

        using SAnimationPropertyConfig = Abyssutils::Animation::SAnimationPropertyConfig;
        template <Animable VarType>
        void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, eAVarDamagePolicy policy) {
            constexpr const eAnimatedVarType EAVTYPE = typeToeAnimatedVarType<VarType>;
            pav                                      = makeUnique<CAnimatedVariable<VarType>>();

            pav->create2(EAVTYPE, sc<Abyssutils::Animation::CAnimationManager*>(this), pav, v);
            pav->setConfig(pConfig);
            pav->m_Context.eDamagePolicy = policy;
        }

        template <Animable VarType>
        void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, PHLWINDOW pWindow, eAVarDamagePolicy policy) {
            createAnimation(v, pav, pConfig, policy);
            pav->m_Context.pWindow = pWindow;
        }
        template <Animable VarType>
        void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, PHLWORKSPACE pWorkspace, eAVarDamagePolicy policy) {
            createAnimation(v, pav, pConfig, policy);
            pav->m_Context.pWorkspace = pWorkspace;
        }
        template <Animable VarType>
        void createAnimation(const VarType& v, PHLANIMVAR<VarType>& pav, SP<SAnimationPropertyConfig> pConfig, PHLLS pLayer, eAVarDamagePolicy policy) {
            createAnimation(v, pav, pConfig, policy);
            pav->m_Context.pLayer = pLayer;
        }

        std::string         styleValidInConfigVar(const std::string&, const std::string&);

        SP<CEventLoopTimer> m_animationTimer;

        float               m_lastTickTimeMs;

      private:
        bool   m_tickScheduled = false;
        bool   m_lastTickValid = false;
        CTimer m_lastTickTimer;
    };

    UP<CAbyssAnimationManager>& mgr();
}
