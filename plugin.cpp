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
    const uint32_t Key1 = 0x02; // corresponds to key "1" on the keyboard
    const uint32_t Key2 = 0x03; // corresponds to key "2" on the keyboard
    const uint32_t Key3 = 0x04; // corresponds to key "3" on the keyboard
    const uint32_t Key4 = 0x05; // corresponds to key "4" on the keyboard
    const uint32_t Key5 = 0x06; // corresponds to key "5" on the keyboard
    const uint32_t Key6 = 0x07; // corresponds to key "6" on the keyboard

    const uint32_t KeyF1 = 0x3b; // corresponds to key "F1" on the keyboard
    const uint32_t KeyF2 = 0x3c; // corresponds to key "F2" on the keyboard


    uint32_t RightHandQueueRotationKey      = Key1;
    uint32_t LeftHandQueueRotationKey       = Key2;
    uint32_t ShoutPowerQueueRotationKey     = Key3;
    uint32_t AmmoQueueRotationKey           = Key4;
    uint32_t PotionQueue1RotationKey        = KeyF1;
    uint32_t PotionQueue2RotationKey        = KeyF2;

    uint32_t PotionQueue1UseKey = Key5;
    uint32_t PotionQueue2UseKey = Key6;
}

namespace GameDataCache
{
    RE::PlayerCharacter*    Player          = nullptr;
    RE::TESDataHandler*     DataHandler     = nullptr;
    RE::ActorEquipManager*  EquipManager    = nullptr;
    RE::SkyrimVM*           VM              = nullptr;

    RE::BGSEquipSlot* LeftHand  = nullptr;
    RE::BGSEquipSlot* RightHand = nullptr;
    RE::BGSEquipSlot* BothHands = nullptr;      //2 hands: i.e. 2H weapon, master spells
    RE::BGSEquipSlot* Shield    = nullptr;

    RE::TESQuest* PoisonWeaponDialogQuest = nullptr;

    RE::BSFixedString ShowEquippedUnpoisonedWeaponEventName;
    RE::BSFixedString WidgetDataUpdatedEventName;

    //Array keeps perks that modify amount of hits that poison keeps working and the amount of hits that perk gives
    RE::BSTArray<std::pair<RE::BGSPerk*, int>> ModPoisonDosePerks;

    int DefaultDoseCount = 1;


    void Init()
    {
        Player          = RE::PlayerCharacter::     GetSingleton();
        DataHandler     = RE::TESDataHandler::      GetSingleton();
        EquipManager    = RE::ActorEquipManager::   GetSingleton();
        VM              = RE::SkyrimVM::            GetSingleton();

        LeftHand    = DataHandler->LookupForm<RE::BGSEquipSlot>( 0x013f43, "Skyrim.esm" );
        RightHand   = DataHandler->LookupForm<RE::BGSEquipSlot>( 0x013f42, "Skyrim.esm" );
        BothHands   = DataHandler->LookupForm<RE::BGSEquipSlot>( 0x013f45, "Skyrim.esm" );
        Shield      = DataHandler->LookupForm<RE::BGSEquipSlot>( 0x0141e8, "Skyrim.esm" );

        PoisonWeaponDialogQuest = DataHandler->LookupForm<RE::TESQuest>( 0x000d62, "Equip.esp" );

        ShowEquippedUnpoisonedWeaponEventName = "OnShowEquippedUnpoisonedWeapon";
        WidgetDataUpdatedEventName = "OnWidgetDataUpdated";

        RE::BGSPerk* perk1 = DataHandler->LookupForm<RE::BGSPerk>( 0x105f2f, "Skyrim.esm" );
        RE::BGSPerk* perk2 = DataHandler->LookupForm<RE::BGSPerk>( 0x3120f8, "Vokrii - Minimalistic Perks of Skyrim.esp" );
        RE::BGSPerk* perk3 = DataHandler->LookupForm<RE::BGSPerk>( 0x3120f9, "Vokrii - Minimalistic Perks of Skyrim.esp" );

        if( perk1 ) ModPoisonDosePerks.push_back( std::make_pair( perk1, 2 ) );
        if( perk2 ) ModPoisonDosePerks.push_back( std::make_pair( perk2, 4 ) );
        if( perk3 ) ModPoisonDosePerks.push_back( std::make_pair( perk3, 6 ) );
    }

    int GetCurrentPoisonDoseCount()
    {
        int result = DefaultDoseCount;

        for ( auto& it : ModPoisonDosePerks )
        {
            if ( Player->HasPerk( it.first ) )
            {
                RE::BGSPerk* perk = it.first;
                int iterationResult = it.second;
                //this loop is not optimized, let it be as for now
                while ( perk->nextPerk && Player->HasPerk( perk->nextPerk ) )
                {
                    auto searchResult = std::find_if(
                            ModPoisonDosePerks.begin()
                        ,   ModPoisonDosePerks.end()
                        ,   [ perk ]( auto& _it ) { return _it.first == perk; } 
                    );

                    if ( searchResult != ModPoisonDosePerks.end() )
                        iterationResult = searchResult->second;
                }

                result += iterationResult;
            }
        }

        return result;
    }
}

namespace Widgets
{
}

namespace GameUICache
{
    RE::UI* UI = nullptr;

    uint32_t DefaultMenusCount = 3;


    void Init()
    {
        UI = RE::UI::GetSingleton();
    }

    bool IsMenuOpened( const std::string_view& _menu )
    {
        return UI->IsMenuOpen( _menu );
    }

    template <typename TMenu> 
    TMenu* GetOpenedMenu( const std::string_view _menu )
    {
        if ( !IsMenuOpened( _menu ) )
            return nullptr;

        RE::IMenu* menu = UI->GetMenu( _menu ).get();

        return static_cast<TMenu*>( menu );
    }

    RE::TESBoundObject* GetSelectedItem( RE::ItemList* _itemList )
    {
        if ( !_itemList )
            return nullptr;

        RE::ItemList::Item* item = _itemList->GetSelectedItem();
        if ( !item )
            return nullptr;

        RE::InventoryEntryData* entryData = item->data.objDesc;
        if ( !entryData )
            return nullptr;

        return entryData->GetObject();
    }
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

        GameDataCache::Init();
        GameUICache::Init();

        RE::BSInputDeviceManager* idm = RE::BSInputDeviceManager::GetSingleton();
        idm->AddEventSink( GetKeyEventSink() );
    }
}