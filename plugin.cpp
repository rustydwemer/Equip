void skseEventListener( SKSE::MessagingInterface::Message* );

namespace Papyrus
{
    bool RegisterPapyrusFunctions( RE::BSScript::IVirtualMachine* );
}


namespace Utils
{
    bool IsWeaponTwoHanded( const RE::TESObjectWEAP* );
    bool IsTwoHanded( const RE::TESForm* );
    bool IsShield( const RE::TESForm* );
    bool IsEquipableItem( const RE::TESForm* );
    bool IsEquipableSpell( const RE::TESForm* );
    bool IsEquipableShout( const RE::TESForm* );

    RE::BGSEquipSlot* GetSlotForRightHand( const RE::TESForm* );
    RE::BGSEquipSlot* GetSlotForLeftHand( const RE::TESForm* );

    void EquipItem( RE::TESForm*, const RE::BGSEquipSlot* );
    void EquipSpell( RE::TESForm*, const RE::BGSEquipSlot* );
    void EquipShout( RE::TESForm*, const RE::BGSEquipSlot* );

    void PoisonWeapon( RE::TESObjectWEAP* _weapon, RE::AlchemyItem* );
    void PoisonEquippedWeapon( RE::AlchemyItem* );
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
    const uint32_t Potion1QueueID       = 4;
    const uint32_t Potion2QueueID       = 5;


    #define DECLARE_DEFAULT_COORDINATE(COORDINATE, VALUE)\
        int COORDINATE = VALUE;

    #define DECLARE_DEFAULT_POSITION(QUEUE, IMAGE_POS_X, IMAGE_POS_Y, TEXT_POS_X, TEXT_POS_Y)\
        DECLARE_DEFAULT_COORDINATE( QUEUE##ImagePosX,   IMAGE_POS_X )\
        DECLARE_DEFAULT_COORDINATE( QUEUE##ImagePosY,   IMAGE_POS_Y )\
        DECLARE_DEFAULT_COORDINATE( QUEUE##TextPosX,    TEXT_POS_X  )\
        DECLARE_DEFAULT_COORDINATE( QUEUE##TextPosY,    TEXT_POS_Y  )

    DECLARE_DEFAULT_POSITION( RightHand,  640, 360, 640, 480 )
    DECLARE_DEFAULT_POSITION( LeftHand,   540, 360, 540, 480 )
    DECLARE_DEFAULT_POSITION( ShoutPower, 590, 500, 640, 620 )
    DECLARE_DEFAULT_POSITION( Ammo, 590, 500, 640, 620 )
    DECLARE_DEFAULT_POSITION( Potion1, 590, 500, 640, 620 )
    DECLARE_DEFAULT_POSITION( Potion2, 590, 500, 640, 620 )

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
        QueuesRotationKeys[ Potion1QueueID ]    = KeyF1;
        QueuesRotationKeys[ Potion2QueueID ]    = KeyF2;

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
    struct WidgetData
    {
        int iWantWidgetImageID;
        int iWantWidgetTextID;

        int ImagePosX;
        int ImagePosY;

        int TextPosX;
        int TextPosY;

        std::string ImageName;
        std::string Text;
    };

    WidgetData WidgetDatas[ Settings::GetQueuesCount() ];

    #define INIT_WIDGETDATA(QUEUE)\
        WidgetDatas[ Settings::QUEUE##QueueID ].Text = "Empty";\
        WidgetDatas[ Settings::QUEUE##QueueID ].ImageName = "";\
        WidgetDatas[ Settings::QUEUE##QueueID ].ImagePosX = Settings::QUEUE##ImagePosX;\
        WidgetDatas[ Settings::QUEUE##QueueID ].ImagePosY = Settings::QUEUE##ImagePosY;\
        WidgetDatas[ Settings::QUEUE##QueueID ].TextPosX = Settings::QUEUE##TextPosX;\
        WidgetDatas[ Settings::QUEUE##QueueID ].TextPosY = Settings::QUEUE##TextPosY;


    void Init()
    {
        INIT_WIDGETDATA( RightHand )
        INIT_WIDGETDATA( LeftHand  )
        INIT_WIDGETDATA( ShoutPower )
        INIT_WIDGETDATA( Ammo )
        INIT_WIDGETDATA( Potion1 )
        INIT_WIDGETDATA( Potion1 )
    }

    const char* GetWeaponImageName( const RE::TESObjectWEAP* _weapon )
    {
        if ( _weapon->IsOneHandedSword() )
            return "equip/gladius.dds";

        if ( _weapon->IsTwoHandedSword() )
            return "equip/broadsword.dds";

        if ( _weapon->IsOneHandedDagger() )
            return "equip/broad-dagger.dds";

        if ( _weapon->IsOneHandedAxe() )
            return "equip/fire-axe.dds";

        if ( _weapon->IsOneHandedMace() )
            return "equip/flanged-mace.dds";

        if ( _weapon->IsTwoHandedAxe() )
        {
            if ( strstr( _weapon->GetName(), "Warhammer" ) )
                return "equip/warhammer.dds";

            return "equip/battle-axe.dds";
        }

        if ( _weapon->IsBow() )
            return "equip/bow.dds";

        if ( _weapon->IsCrossbow() )
            return "equip/crossbow.dds";

        return "equip/wood-axe.dds";
    }
    
    const char* GetImageName( const RE::TESForm* _object )
    {
        if ( _object->IsWeapon() )
            return GetWeaponImageName( _object->As<RE::TESObjectWEAP>() );

        if ( _object->IsArmor() )
            return Utils::IsShield( _object ) ? "equip/round-shield.dds" : "equip/cube.dds";

        return "equip/cube.dds";
    }

    void SendEvent( int _queueId )
    {
        RE::VMTypeID id = static_cast<RE::VMTypeID>( GameDataCache::Player->GetFormType() );
        RE::VMHandle handle = GameDataCache::VM->handlePolicy.GetHandleForObject( id, GameDataCache::Player );

        GameDataCache::VM->SendAndRelayEvent(
                handle
            ,   &GameDataCache::WidgetDataUpdatedEventName
            ,   RE::MakeFunctionArguments( (int) _queueId )
            ,   nullptr
        );
    }
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
    bool IsWeaponTwoHanded( const RE::TESObjectWEAP* _weapon )
    {
        return !(
                        _weapon->IsOneHandedAxe()
                    ||  _weapon->IsOneHandedDagger()
                    ||  _weapon->IsOneHandedMace()
                    ||  _weapon->IsOneHandedSword()
                    ||  _weapon->IsStaff() 
        );
    }

    bool IsTwoHanded( const RE::TESForm* _object )
    {
        if ( _object->IsWeapon() )
            return IsWeaponTwoHanded( _object->As<RE::TESObjectWEAP>() );

        if ( _object->Is( RE::FormType::Spell ) )
            return _object->As<RE::SpellItem>()->IsTwoHanded();

        if ( _object->Is( RE::FormType::Scroll ) )
            return  _object->As<RE::ScrollItem>()->IsTwoHanded();

        return false;
    }

    bool IsShield( const RE::TESForm* _object )
    {
        return _object->IsArmor() && _object->As<RE::TESObjectARMO>()->IsShield();
    }

    bool IsEquipableItem( const RE::TESForm* _object )
    {
        return  _object->formType == RE::FormType::Weapon
            ||  IsShield( _object )
            ||  _object->formType == RE::FormType::Ammo
            ||  _object->formType == RE::FormType::Scroll;
    }

    bool IsEquipableSpell( const RE::TESForm* _object )
    {
        if ( !_object->Is( RE::FormType::Spell ) )
            return false;

        const RE::SpellItem* spell = _object->As<RE::SpellItem>();
        RE::MagicSystem::SpellType spellType = spell->GetSpellType();
        if ( spellType != RE::MagicSystem::SpellType::kSpell && spellType != RE::MagicSystem::SpellType::kLesserPower )
            return false;

        return true;
    }

    bool IsEquipableShout( const RE::TESForm* _object )
    {
        return _object->Is( RE::FormType::Shout );
    }

    RE::BGSEquipSlot* GetSlotForRightHand( const RE::TESForm* _object )
    {
        if ( _object->IsWeapon() )
        {
            const RE::TESObjectWEAP* weapon = _object->As<RE::TESObjectWEAP>();
            return !Utils::IsWeaponTwoHanded( weapon ) ? GameDataCache::BothHands : GameDataCache::RightHand;
        }
        else if ( _object->Is( RE::FormType::Scroll ) )
        {
            const RE::ScrollItem* scroll = _object->As<RE::ScrollItem>();
            return scroll->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::RightHand;
        }
        else if ( _object->Is( RE::FormType::Spell ) )
        {
            const RE::SpellItem* spell = _object->As<RE::SpellItem>();
            if ( spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell )
                return spell->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::RightHand;
        }
        return nullptr;
    }

    RE::BGSEquipSlot* GetSlotForLeftHand( const RE::TESForm* _object )
    {
        if ( _object->IsWeapon() )
        {
            const RE::TESObjectWEAP* weapon = _object->As<RE::TESObjectWEAP>();
            return !Utils::IsWeaponTwoHanded( weapon ) ? GameDataCache::BothHands : GameDataCache::LeftHand;
        }
        else if ( _object->IsArmor() )
        {
            const RE::TESObjectARMO* armor = _object->As<RE::TESObjectARMO>();
            return armor->IsShield() ? GameDataCache::Shield : nullptr;
        }
        else if ( _object->Is( RE::FormType::Scroll ) )
        {
            const RE::ScrollItem* scroll = _object->As<RE::ScrollItem>();
            return scroll->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::LeftHand;
        }
        else if ( _object->Is( RE::FormType::Spell ) )
        {
            const RE::SpellItem* spell = _object->As<RE::SpellItem>();
            if ( spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell )
                return spell->IsTwoHanded() ? GameDataCache::BothHands : GameDataCache::LeftHand;
        }
        return nullptr;
    }

    void EquipItem( RE::TESForm* _object, const RE::BGSEquipSlot* _slot )
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

    void EquipSpell( RE::TESForm* _object, const RE::BGSEquipSlot* _slot )
    {
        RE::SpellItem* spell = _object->As<RE::SpellItem>();
        RE::BSTSmallArray<RE::SpellItem*>& playerSpells = GameDataCache::Player->GetActorRuntimeData().addedSpells;
        if ( !std::find( playerSpells.begin(), playerSpells.end(), spell ) )
            return;

        GameDataCache::EquipManager->EquipSpell( GameDataCache::Player, spell, _slot );
    }

    void EquipShout( RE::TESForm* _object, const RE::BGSEquipSlot* _slot )
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

        void setID( uint32_t _id )
        {
            m_queueID = _id;
        }
        uint32_t getID() const
        {
            return m_queueID;
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
        uint32_t m_queueID;
        QueueType m_type = QueueType::kInvalid;
        bool m_wasEmpty = true;
    };

    Queue Queues[ Settings::GetQueuesCount() ];

    void QueueUnqueueFromInventoryMenu( Queue& _queue )
    {
        RE::InventoryMenu* inventoryMenu = GameUICache::GetOpenedMenu<RE::InventoryMenu>( RE::InventoryMenu::MENU_NAME );
        if ( inventoryMenu )
        {
            RE::ItemList* itemList = inventoryMenu->GetRuntimeData().itemList;
            RE::TESBoundObject* selectedItem = GameUICache::GetSelectedItem( itemList );

            _queue.queueUnqueue( selectedItem );
        }
    }

    void QueueUnqueueFromMagicMenu( Queue& _queue )
    {
        RE::MagicMenu* magicMenu = GameUICache::GetOpenedMenu<RE::MagicMenu>( RE::MagicMenu::MENU_NAME );

        if ( magicMenu )
        {
            RE::GFxValue result;
            magicMenu->uiMovie->GetVariable( &result, "_root.Menu_mc.inventoryList.itemList.selectedEntry.formId" );
            if ( result.GetType() != RE::GFxValue::ValueType::kNumber )
                return;

            uint32_t selectedFormId = (uint32_t) result.GetNumber();
            RE::TESForm* form = RE::TESForm::LookupByID( selectedFormId );
            if ( !form )
                return;

            _queue.queueUnqueue( form );
        }
    }

    void Init()
    {
        Queues[ Settings::RightHandQueueID ].setType( Queue::QueueType::kRightHand );
        Queues[ Settings::RightHandQueueID ].setID( Settings::RightHandQueueID );

        Queues[ Settings::LeftHandQueueID ].setType( Queue::QueueType::kLeftHand );
        Queues[ Settings::LeftHandQueueID ].setID( Settings::LeftHandQueueID );

        Queues[ Settings::ShoutPowerQueueID ].setType( Queue::QueueType::kShoutOrPower );
        Queues[ Settings::ShoutPowerQueueID ].setID( Settings::ShoutPowerQueueID );

        Queues[ Settings::AmmoQueueID ].setType( Queue::QueueType::kAmmo );
        Queues[ Settings::AmmoQueueID ].setID( Settings::AmmoQueueID );

        Queues[ Settings::Potion1QueueID ].setType( Queue::QueueType::kPotion );
        Queues[ Settings::Potion1QueueID ].setID( Settings::Potion1QueueID );

        Queues[ Settings::Potion2QueueID ].setType( Queue::QueueType::kPotion );
        Queues[ Settings::Potion2QueueID ].setID( Settings::Potion2QueueID );
    }

    void OnHandQueue( Queue& _queue, Queue& _oppositeQueue )
    {
        if ( GameUICache::IsMenuOpened( RE::InventoryMenu::MENU_NAME ) )
        {
            Queues::QueueUnqueueFromInventoryMenu( _queue );
        }
        else if ( GameUICache::IsMenuOpened( RE::MagicMenu::MENU_NAME ) )
        {
            Queues::QueueUnqueueFromMagicMenu( _queue );
        }
        else if ( GameUICache::UI->menuStack.size() == GameUICache::DefaultMenusCount )
        {
            RE::TESForm* next = _queue.getNext();
            if ( !next )
                return;

            if ( Utils::IsTwoHanded( next ) )
                _oppositeQueue.unequipCurrent();

            _queue.equipNext();

            Widgets::WidgetDatas[ _queue.getID() ].ImageName = Widgets::GetImageName( _queue.getCurrent() );
            Widgets::WidgetDatas[ _queue.getID() ].Text = _queue.getCurrent()->GetName();
            Widgets::SendEvent( _queue.getID() );
        }
    }

    //void UnequipFromOppositeQueueIfEquippingTwoHanded( Queue& _queue, Queue& _opppositeQueue )
    //{
    //    RE::TESForm* form = _queue.getNext();

    //    if ( !form )
    //        return;

    //    if ( Utils::IsTwoHanded( form ) )
    //        _opppositeQueue.unequipCurrent();
    //}
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

        if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ Settings::RightHandQueueID ] && buttonEvent->IsDown() )
        {
            Queues::OnHandQueue(
                    Queues::Queues[ Settings::RightHandQueueID ]
                ,   Queues::Queues[ Settings::LeftHandQueueID  ]
            );
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ Settings::LeftHandQueueID ] && buttonEvent->IsDown() )
        {
            Queues::OnHandQueue(
                    Queues::Queues[ Settings::RightHandQueueID ]
                ,   Queues::Queues[ Settings::LeftHandQueueID  ]
            );
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ Settings::ShoutPowerQueueID ] && buttonEvent->IsDown() )
        {
            Queues::Queue& queue = Queues::Queues[ Settings::ShoutPowerQueueID ];
            if ( GameUICache::IsMenuOpened( RE::MagicMenu::MENU_NAME ) )
                Queues::QueueUnqueueFromMagicMenu( queue );
            else if ( GameUICache::UI->menuStack.size() == GameUICache::DefaultMenusCount )
                EquipAndUpdateWidgetData( queue );
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ Settings::AmmoQueueID ] && buttonEvent->IsDown() )
        {
            Queues::Queue& queue = Queues::Queues[ Settings::AmmoQueueID ];
            if ( GameUICache::IsMenuOpened( RE::InventoryMenu::MENU_NAME ) )
                Queues::QueueUnqueueFromInventoryMenu( queue );
            else if ( GameUICache::UI->menuStack.size() == GameUICache::DefaultMenusCount )
                EquipAndUpdateWidgetData( queue );
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ Settings::Potion1QueueID ] && buttonEvent->IsDown() )
        {
            Queues::Queue& queue = Queues::Queues[ Settings::Potion1QueueID ];
            if ( GameUICache::IsMenuOpened( RE::InventoryMenu::MENU_NAME ) )
                Queues::QueueUnqueueFromInventoryMenu( queue );
            else if ( GameUICache::UI->menuStack.size() == GameUICache::DefaultMenusCount )
                EquipAndUpdateWidgetData( queue );
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ Settings::Potion2QueueID ] && buttonEvent->IsDown() )
        {
            Queues::Queue& queue = Queues::Queues[ Settings::Potion2QueueID ];
            if ( GameUICache::IsMenuOpened( RE::InventoryMenu::MENU_NAME ) )
                Queues::QueueUnqueueFromInventoryMenu( queue );
            else if ( GameUICache::UI->menuStack.size() == GameUICache::DefaultMenusCount )
                EquipAndUpdateWidgetData( queue );
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ 0 ] && buttonEvent->IsDown() )
        {
            Queues::Queues[ Settings::Potion1QueueID ].usePotion();
        }
        else if ( buttonEvent->GetIDCode() == Settings::QueuesRotationKeys[ 1 ] && buttonEvent->IsDown() )
        {
            Queues::Queues[ Settings::Potion2QueueID ].usePotion();
        }

        return RE::BSEventNotifyControl::kContinue;
    }

private:

    void EquipAndUpdateWidgetData( Queues::Queue& _queue )
    {
        _queue.equipNext();
        Widgets::WidgetDatas[ _queue.getID() ].ImageName = Widgets::GetImageName( _queue.getCurrent() );
        Widgets::WidgetDatas[ _queue.getID() ].Text = _queue.getCurrent()->GetName();
        Widgets::SendEvent( _queue.getID() );
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
        Queues::Init();
        Widgets::Init();

        RE::BSInputDeviceManager* idm = RE::BSInputDeviceManager::GetSingleton();
        idm->AddEventSink( GetKeyEventSink() );
    }
}