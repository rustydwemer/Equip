void skseEventListener( SKSE::MessagingInterface::Message* );

namespace Papyrus
{
    bool RegisterPapyrusFunctions( RE::BSScript::IVirtualMachine* );
}


SKSEPluginLoad( const SKSE::LoadInterface* _skse )
{
    SKSE::Init( _skse );

    SKSE::GetMessagingInterface()->RegisterListener( skseEventListener );
    SKSE::GetPapyrusInterface()->Register( Papyrus::RegisterPapyrusFunctions );

    return true;
}



namespace Papyrus
{
    bool RegisterPapyrusFunctions( RE::BSScript::IVirtualMachine* _vm )
    {
        return true;
    }
}

namespace Settings
{
}

namespace GameDataCache
{
}

namespace Widgets
{
}

namespace GameUICache
{
}

namespace Queues
{
}

class KeyEventSink: public RE::BSTEventSink<RE::InputEvent*>
{
public:
    RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* _event
        ,   RE::BSTEventSource<RE::InputEvent*>*
    ) override
    {
        if ( !_event || !( *_event ) )
            return RE::BSEventNotifyControl::kContinue;

        RE::ButtonEvent* buttonEvent = ( *_event )->AsButtonEvent();
        if ( !buttonEvent )
            return RE::BSEventNotifyControl::kContinue;

        if (
                    buttonEvent->device != RE::INPUT_DEVICES::kKeyboard
                ||  buttonEvent->GetEventType() != RE::INPUT_EVENT_TYPE::kButton
            )
            return RE::BSEventNotifyControl::kContinue;

        //DO STAFF HERE;

        return RE::BSEventNotifyControl::kContinue;
    }
};

KeyEventSink KESink;

KeyEventSink* GetKeyEventSink()
{
    return &KESink;
}

void skseEventListener( SKSE::MessagingInterface::Message* _msg )
{
    if ( _msg->type == SKSE::MessagingInterface::kDataLoaded )
    {
        RE::BSInputDeviceManager* idm = RE::BSInputDeviceManager::GetSingleton();
        idm->AddEventSink( GetKeyEventSink() );
    }
}