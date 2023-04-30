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

    const uint32_t RightHandQueueID     = 0;
    const uint32_t LeftHandQueueID      = 1;
    const uint32_t ShoutPowerQueueID    = 2;
    const uint32_t AmmoQueueID          = 3;
    const uint32_t PotionQueue1ID       = 4;
    const uint32_t PotionQueue2ID       = 5;

    constexpr const uint32_t GetQueuesCount()
    {
        return 6;
    }

    constexpr const uint32_t GetPotionQueuesCount()
    {
        return 2;
    }

    uint32_t QueuesRotationKeys[ GetQueuesCount() ];

    uint32_t QueuesUseKeys[ GetPotionQueuesCount() ];

    void Init()
    {
        QueuesRotationKeys[ RightHandQueueID ]  = Key1;
        QueuesRotationKeys[ LeftHandQueueID ]   = Key2;
        QueuesRotationKeys[ ShoutPowerQueueID ] = Key3;
        QueuesRotationKeys[ AmmoQueueID ]       = Key4;
        QueuesRotationKeys[ PotionQueue1ID ]    = KeyF1;
        QueuesRotationKeys[ PotionQueue2ID ]    = KeyF2;

        QueuesUseKeys[ 0 ] = Key5;
        QueuesUseKeys[ 1 ] = Key6;
    }
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

namespace Utils
{
    bool IsWeaponTwoHanded( RE::TESObjectWEAP* _weapon )
    {
        return !(
                        _weapon->IsOneHandedAxe()
                    ||  _weapon->IsOneHandedDagger()
                    ||  _weapon->IsOneHandedMace()
                    ||  _weapon->IsOneHandedSword()
                    ||  _weapon->IsStaff() 
        );
    }

    bool IsShield( RE::TESForm* _object )
    {
        return _object->IsArmor() && _object->As<RE::TESObjectARMO>()->IsShield();
    }

    bool IsEquipableItem( RE::TESForm* _object )
    {
        return  _object->formType == RE::FormType::Weapon
            ||  IsShield( _object )
            ||  _object->formType == RE::FormType::Ammo
            ||  _object->formType == RE::FormType::Scroll;
    }

    bool IsEquipableSpell( RE::TESForm* _object )
    {
        if ( !_object->Is( RE::FormType::Spell ) )
            return false;

        RE::SpellItem* spell = _object->As<RE::SpellItem>();
        RE::MagicSystem::SpellType spellType = spell->GetSpellType();
        if ( spellType != RE::MagicSystem::SpellType::kSpell && spellType != RE::MagicSystem::SpellType::kLesserPower )
            return false;

        return true;
    }

    bool IsEquipableShout( RE::TESForm* _object )
    {
        return _object->Is( RE::FormType::Shout );
    }

    RE::BGSEquipSlot* GetSlotForRightHand( RE::TESForm* _object )
    {
        if ( _object->IsWeapon() )
        {
            RE::TESObjectWEAP* weapon = _object->As<RE::TESObjectWEAP>();
            return !Utils::IsWeaponTwoHanded( weapon ) ? GameDataCache::BothHands : GameDataCache::RightHand;
        }
        else if ( _object->Is( RE::FormType::Scroll ) )
        {
            RE::ScrollItem* scroll = _object->As<RE::ScrollItem>();
            return scroll->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::RightHand;
        }
        else if ( _object->Is( RE::FormType::Spell ) )
        {
            RE::SpellItem* spell = _object->As<RE::SpellItem>();
            if ( spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell )
                return spell->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::RightHand;
        }
        return nullptr;
    }

    RE::BGSEquipSlot* GetSlotForLeftHand( RE::TESForm* _object )
    {
        if ( _object->IsWeapon() )
        {
            RE::TESObjectWEAP* weapon = _object->As<RE::TESObjectWEAP>();
            return !Utils::IsWeaponTwoHanded( weapon ) ? GameDataCache::BothHands : GameDataCache::LeftHand;
        }
        else if ( _object->IsArmor() )
        {
            RE::TESObjectARMO* armor = _object->As<RE::TESObjectARMO>();
            return armor->IsShield() ? GameDataCache::Shield : nullptr;
        }
        else if ( _object->Is( RE::FormType::Scroll ) )
        {
            RE::ScrollItem* scroll = _object->As<RE::ScrollItem>();
            return scroll->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::LeftHand;
        }
        else if ( _object->Is( RE::FormType::Spell ) )
        {
            RE::SpellItem* spell = _object->As<RE::SpellItem>();
            if ( spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell )
                return spell->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::LeftHand;
        }
        return nullptr;
    }

    void EquipItem( RE::TESForm* _object, RE::BGSEquipSlot* _slot )
    {
        auto filter = []( RE::TESForm& _obj )
        {
            if ( _obj.formType == RE::FormType::Weapon )
            {
                return true;
            }
            else if ( _obj.formType == RE::FormType::Armor )
            {
                RE::TESObjectARMO* armor = _obj.As<RE::TESObjectARMO>();
                return armor->IsShield();
            }
            else if ( _obj.formType == RE::FormType::Ammo )
                return true;
            else if ( _obj.formType == RE::FormType::Scroll )
                return true;
            else
                return false;
        };

        RE::TESObjectREFR::InventoryItemMap objectsInInventory = GameDataCache::Player->GetInventory( filter );
        auto it = objectsInInventory.find( _object->As<RE::TESBoundObject>() );

        if ( it == objectsInInventory.end() )
            return;

        int32_t objectsInInventoryCount = it->second.first;
        RE::InventoryEntryData* entryData = it->second.second.get();
        bool isObjectWorn = entryData->IsWorn();

        if ( objectsInInventoryCount == 1 && isObjectWorn )
            return;

        if ( objectsInInventoryCount < 1 )
            return;

        GameDataCache::EquipManager->EquipObject( GameDataCache::Player, _object->As<RE::TESBoundObject>(), nullptr, 1U, _slot );
    }

    void EquipSpell( RE::TESForm* _object, RE::BGSEquipSlot* _slot )
    {
        RE::SpellItem* spell = _object->As<RE::SpellItem>();
        RE::BSTSmallArray<RE::SpellItem*>& playerSpells = GameDataCache::Player->GetActorRuntimeData().addedSpells;
        if ( !std::find( playerSpells.begin(), playerSpells.end(), spell ) )
            return;

        GameDataCache::EquipManager->EquipSpell( GameDataCache::Player, spell, _slot );
    }

    void EquipShout( RE::TESForm* _object, RE::BGSEquipSlot* _slot )
    {
        RE::TESShout* shout = _object->As<RE::TESShout>();
        RE::TESWordOfPower* word = shout->variations[ 0 ].word;

        if ( !word->GetKnown() )
            return;

        GameDataCache::EquipManager->EquipShout( GameDataCache::Player, shout );
    }


    void PoisonWeapon( RE::TESObjectWEAP* _weapon, RE::AlchemyItem* _potion )
    {
        if ( !_potion || !_weapon )
            return;

        auto alchFilter = [ _potion ]( RE::TESForm& _obj ) { return _obj.formType == RE::FormType::AlchemyItem && &_obj == _potion; };
        RE::TESObjectREFR::InventoryItemMap potionInInventory = GameDataCache::Player->GetInventory( alchFilter );

        if ( potionInInventory.empty() )
            return;

        auto weapFilter = [ _weapon ]( RE::TESForm& _obj ) { return _obj.formType == RE::FormType::Weapon && &_obj == _weapon; };
        RE::TESObjectREFR::InventoryItemMap weaponInInventory = GameDataCache::Player->GetInventory( weapFilter );

        if ( weaponInInventory.empty() )
            return;

        RE::InventoryEntryData* data = weaponInInventory.begin()->second.second.get();
        if ( !data )
            return;

        data->PoisonObject( _potion, GameDataCache::GetCurrentPoisonDoseCount() );
        GameDataCache::Player->RemoveItem( _potion, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr );
    }

    void PoisonEquippedWeapon( RE::AlchemyItem* _potion )
    {
        auto filter = []( RE::TESForm& _obj ) { return _obj.formType == RE::FormType::Weapon; };
        RE::TESObjectREFR::InventoryItemMap objectsInInventory = GameDataCache::Player->GetInventory( filter );

        RE::BSTArray<RE::TESObjectWEAP*> weapons;

        for ( auto& it : objectsInInventory )
        {
            RE::InventoryEntryData* entryData = it.second.second.get();
            if ( entryData->IsWorn() && !entryData->IsPoisoned() )
                weapons.push_back( it.first->As<RE::TESObjectWEAP>() );
        }

        if ( weapons.empty() )
            return;

        if ( weapons.size() == 1 )
        {
            PoisonWeapon( weapons[ 0 ], _potion );
            return;
        }

        RE::VMHandle handle = GameDataCache::VM->handlePolicy.GetHandleForObject(
                static_cast<RE::VMTypeID>( GameDataCache::PoisonWeaponDialogQuest->GetFormType() )
            ,   GameDataCache::PoisonWeaponDialogQuest
        );

        GameDataCache::VM->SendAndRelayEvent(
                handle
            ,   &GameDataCache::ShowEquippedUnpoisonedWeaponEventName
            ,   RE::MakeFunctionArguments( (RE::BSTArray<RE::TESObjectWEAP*>) weapons, (RE::AlchemyItem*) _potion )
            ,   nullptr
        );
    }
}

namespace Queues
{
    class Queue
    {
    public:
        enum class QueueType
        {
                kInvalid = 0
            ,   kRightHand
            ,   kLeftHand
            ,   kShoutOrPower
            ,   kPotion
            ,   kAmmo
        };

    public:

        void setType( QueueType _type )
        {
            m_type = _type;
        }
        QueueType getType() const
        {
            return m_type;
        }

        uint32_t getSize() const
        {
            return m_queue.size();
        }

        RE::TESForm* at( uint32_t _index ) const
        {
            if ( _index >= m_queue.size() )
                return nullptr;

            return m_queue[ _index ];
        }

        void queueUnqueue( RE::TESForm* _object )
        {
            if ( !isObjectValidForQueueing( _object ) )
                return;

            bool isObjectQueued = isQueued( _object, true );
            if ( isObjectQueued && m_markedObject )
            {
                m_queue.erase( m_markedObject );
                if ( m_markedObjectIndex <= m_currentIndex )
                    --m_currentIndex;

                if ( m_queue.empty() )
                    m_wasEmpty = true;
            }
            else if ( !isObjectQueued )
            {
                m_queue.push_back( _object );
            }

            m_markedObject = nullptr;
            m_markedObjectIndex = 0;
        }

        bool has( RE::TESForm* _object )
        {
            return isQueued( _object, false );
        }

        RE::TESForm* getNext() const
        {
            if ( m_queue.empty() )
                return nullptr;

            uint32_t nextIndex = m_currentIndex + 1;
            if ( nextIndex >= m_queue.size() )
                return m_queue[ 0 ];

            return m_queue[ nextIndex ];
        }

        RE::TESForm* getCurrent() const
        {
            if ( m_queue.empty() )
                return nullptr;

            return m_queue[ m_currentIndex ];
        }

        void equipCurrent()
        {
            if ( m_queue.empty() )
                return;

            RE::TESForm* object = m_queue[ m_currentIndex ];
            RE::BGSEquipSlot* slot = getSlot( object );

            switch ( m_type )
            {
            case Queues::Queue::QueueType::kRightHand:
            case Queues::Queue::QueueType::kLeftHand:
                if ( Utils::IsEquipableItem( object ) )
                    Utils::EquipItem( object, slot );
                else if ( Utils::IsEquipableSpell( object ) )
                    Utils::EquipSpell( object, slot );
                break;
            case Queues::Queue::QueueType::kShoutOrPower:
                if ( Utils::IsEquipableSpell( object ) )
                    Utils::EquipSpell( object, slot );
                else if ( Utils::IsEquipableShout( object ) )
                    Utils::EquipShout( object, slot );
                break;
            case Queues::Queue::QueueType::kAmmo:
                Utils::EquipItem( object, slot );
                break;
            }
        }

        void unequipCurrent()
        {
            if ( m_queue.empty() )
                return;

            RE::TESBoundObject* item = m_queue[ m_currentIndex ]->As<RE::TESBoundObject>();
            if ( item )
                GameDataCache::EquipManager->UnequipObject( GameDataCache::Player, item );
        }

        void advance()
        {
            if ( ++m_currentIndex >= m_queue.size() ) m_currentIndex = 0;
        }

        void equipNext()
        {
            if ( m_wasEmpty )
            {
                m_wasEmpty = false;
                equipCurrent();
                return;
            }

            advance();
            equipCurrent();
        }

        void usePotion()
        {
            if ( m_type != QueueType::kPotion )
                return;

            if ( m_queue.empty() )
                return;

            if ( m_currentIndex >= m_queue.size() )
                return; // m_currentIndex = 0; if the current index is out of range, this should be monitored by adding/remove things;

            RE::TESForm* form = m_queue[ m_currentIndex ];
            RE::AlchemyItem* potion = form->As<RE::AlchemyItem>();

            if ( !potion->IsPoison() )
                GameDataCache::Player->DrinkPotion( potion, nullptr );
            else
                Utils::PoisonEquippedWeapon( potion );
        }

        void removeAt( int _index )
        {
            if ( _index < 0 || m_queue.empty() )
                return;

            uint32_t uIndex = _index;

            if ( uIndex >= m_queue.size() )
                return;

            if ( uIndex != 0 && m_currentIndex <= uIndex )
                --m_currentIndex;

            m_queue.erase( m_queue.begin() + uIndex );

            if ( m_queue.empty() )
                m_wasEmpty = true;
        }

    private:

        bool isQueued( RE::TESForm* _object, bool _markForUnqueueing )
        {
            uint32_t count = 0;
            auto fn = [ _object, &count ]( RE::TESForm* it ) { ++count; return it == _object; };
            RE::TESForm** searchResult = std::find_if( m_queue.begin(), m_queue.end(), fn );

            bool result = searchResult != m_queue.end();
            if ( result && _markForUnqueueing )
            {
                m_markedObjectIndex = count - 1;
                m_markedObject = searchResult;
            }
            return result;
        }

        bool isObjectValidForQueueing( RE::TESForm* _object )
        {
            switch ( m_type )
            {
            case Queues::Queue::QueueType::kInvalid:
                return false;
            case Queues::Queue::QueueType::kLeftHand:
                if ( Utils::IsShield( _object ) )
                {
                    return true;
                }
                [[fallthrough]];
                //no break statement, 'cause the other objects we can equip in to left hand, can be also put into right hand
            case Queues::Queue::QueueType::kRightHand:
                if ( _object->IsWeapon() || _object->Is( RE::FormType::Scroll ) )
                {
                    return true;
                }
                else if ( _object->Is( RE::FormType::Spell ) )
                {
                    RE::SpellItem* spell = _object->As<RE::SpellItem>();
                    return spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell;
                }
                break;
            case Queues::Queue::QueueType::kShoutOrPower:
                if ( _object->Is( RE::FormType::Shout ) )
                {
                    RE::TESShout* shout = _object->As<RE::TESShout>();
                    RE::TESWordOfPower* word = shout->variations[ 0 ].word;
                    return word->GetKnown(); //if player knows first word of power from the shout, this shout can be equipped;
                }
                else if ( _object->Is( RE::FormType::Spell ) )
                {
                    RE::SpellItem* power = _object->As<RE::SpellItem>();
                    return power->GetSpellType() == RE::MagicSystem::SpellType::kLesserPower;
                }
                break;
            case Queues::Queue::QueueType::kPotion:
                return _object->Is( RE::FormType::AlchemyItem );
            case Queues::Queue::QueueType::kAmmo:
                return _object->IsAmmo();
            }
            return false;
        }

        RE::BGSEquipSlot* getSlot( RE::TESForm* _object )
        {
            switch ( m_type )
            {
            case Queues::Queue::QueueType::kRightHand:
                return Utils::GetSlotForRightHand( _object );
            case Queues::Queue::QueueType::kLeftHand:
                return Utils::GetSlotForLeftHand( _object );
            }
            return nullptr;
        }

        RE::BSTArray<RE::TESForm*> m_queue;
        RE::TESForm** m_markedObject = nullptr;
        //std::string m_name ""
        uint32_t m_markedObjectIndex = 0;
        uint32_t m_currentIndex = 0;
        QueueType m_type = QueueType::kInvalid;
        bool m_wasEmpty = true;
    };
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


namespace Papyrus
{
    bool RegisterPapyrusFunctions( RE::BSScript::IVirtualMachine* _vm )
    {
        return true;
    }
}


void skseEventListener( SKSE::MessagingInterface::Message* _msg )
{
    if ( _msg->type == SKSE::MessagingInterface::kDataLoaded )
    {

        GameDataCache::Init();
        GameUICache::Init();
        Settings::Init();

        RE::BSInputDeviceManager* idm = RE::BSInputDeviceManager::GetSingleton();
        idm->AddEventSink( GetKeyEventSink() );
    }
}