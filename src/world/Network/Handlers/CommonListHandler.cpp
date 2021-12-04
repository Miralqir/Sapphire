#include <Network/CommonNetwork.h>
#include <Network/GamePacket.h>
#include <Network/PacketContainer.h>
#include <Exd/ExdData.h>
#include <Service.h>
#include <Network/PacketDef/Zone/ClientZoneDef.h>
#include <Network/PacketDef/Zone/ServerZoneDef.h>
#include <Network/CommonActorControl.h>

#include "Manager/LinkshellMgr.h"
#include "Manager/PartyMgr.h"
#include "Territory/InstanceContent.h"

#include "Network/GameConnection.h"

#include "Actor/Player.h"

#include "Linkshell/Linkshell.h"

#include "Session.h"
#include "WorldServer.h"
#include "Forwards.h"

using namespace Sapphire::Common;
using namespace Sapphire::Network::Packets;
using namespace Sapphire::Network::Packets::WorldPackets::Server;
using namespace Sapphire::Network::Packets::WorldPackets::Client;
using namespace Sapphire::Network::Packets::WorldPackets;
using namespace Sapphire::Network::ActorControl;
using namespace Sapphire::World::Manager;

void Sapphire::Network::GameConnection::getCommonlistDetailHandler( const Packets::FFXIVARR_PACKET_RAW& inPacket,
                                                              Entity::Player& player )
{
  const auto packet = ZoneChannelPacket< Client::FFXIVIpcGetCommonlistDetail >( inPacket );
  auto& data = packet.data();

  auto& server = Common::Service< World::WorldServer >::ref();

  auto pPlayer = server.getPlayer( data.DetailCharacterID );

  if( !pPlayer )
    return;

  auto resultPacket = makeZonePacket< FFXIVIpcGetCommonlistDetailResult >( player.getId() );

  resultPacket->data().ListType = data.ListType;
  resultPacket->data().CommunityID = data.CommunityID;
  resultPacket->data().DetailCharacterID = data.DetailCharacterID;
  strcpy( resultPacket->data().FreeCompanyName, "ducks" ); // didn't work

  resultPacket->data().GrandCompanyRank[ 0 ] = pPlayer->getGcRankArray()[0];
  resultPacket->data().GrandCompanyRank[ 1 ] = pPlayer->getGcRankArray()[1];
  resultPacket->data().GrandCompanyRank[ 2 ] = pPlayer->getGcRankArray()[2];
  resultPacket->data().CrestID = 0x0001000100010001;
  strcpy( resultPacket->data().SearchComment, pPlayer->getSearchMessage() );
  resultPacket->data().SelectClassID = pPlayer->getSearchSelectClass(); // this is probably unused in retail

  // serialize class data to packet

  auto classDataArr = pPlayer->getClassArray();

  for( size_t i = 0; i < Common::CLASSJOB_TOTAL; ++i )
  {
    resultPacket->data().ClassData[ i ].id = static_cast< uint16_t >( i );
    resultPacket->data().ClassData[ i ].level = classDataArr[ i ];
  }

  queueOutPacket( resultPacket );
}

void Sapphire::Network::GameConnection::getCommonlistHandler( const Packets::FFXIVARR_PACKET_RAW& inPacket,
                                                              Entity::Player& player )
{
  // TODO: possibly move lambda func to util
  auto& server = Common::Service< World::WorldServer >::ref();

  // this func paginates any commonlist entry, associating them with online player data and hierarchy ID (optional)
  auto generateEntries = [&]( const auto& idVec, size_t offset, const std::vector< Common::HierarchyData >& hierarchyVec ) -> std::vector< PlayerEntry >
  {
    std::vector< PlayerEntry > entries;

    const size_t itemsPerPage = 10;

    for( size_t i = offset; i < offset + 10; ++i )
    {
      if( idVec.size() <= offset + i )
      {
        break;
      }

      auto id = idVec[ i ];
      auto pPlayer = server.getPlayer( id );
      
      if( !pPlayer )
        continue;
        
      PlayerEntry entry{};
      memset( &entry, 0, sizeof( PlayerEntry ) );
      bool isConnected = server.getSession( pPlayer->getCharacterId() ) != nullptr;

      if( isConnected )
      {
        // todo: fix odd teri nullptr on login for friendlist etc
        auto pTeri = pPlayer->getCurrentTerritory();
        if( pTeri )
        {
          entry.TerritoryType = pPlayer->getCurrentTerritory()->getTerritoryTypeId();
          entry.TerritoryID = pPlayer->getCurrentTerritory()->getTerritoryTypeId();
        }

        entry.CurrentClassID = static_cast< uint8_t >( pPlayer->getClass() );
        entry.SelectClassID = static_cast< uint8_t >( pPlayer->getSearchSelectClass() );
          
        entry.CurrentLevel = pPlayer->getLevel();
        entry.SelectLevel = pPlayer->getLevel();
        entry.Identity = pPlayer->getGender();

        // GC icon
        entry.GrandCompanyID = pPlayer->getGc();
        // client language J = 0, E = 1, D = 2, F = 3
        entry.Region = 1;
        // user language settings flag J = 1, E = 2, D = 4, F = 8
        entry.SelectRegion = pPlayer->getSearchSelectRegion();
        entry.OnlineStatus = pPlayer->getOnlineStatusMask() | pPlayer->getOnlineStatusCustomMask();
      }

      entry.CharacterID = pPlayer->getCharacterId();
      strcpy( entry.CharacterName, pPlayer->getName().c_str() );

      if( !hierarchyVec.empty() )
      {
        auto hierarchy = hierarchyVec[ i ];

        entry.Timestamp = hierarchy.data.dateAdded;
        entry.HierarchyStatus = hierarchy.data.status;
        entry.HierarchyType = hierarchy.data.type;
        entry.HierarchyGroup = hierarchy.data.group;
        entry.HierarchyUnk = hierarchy.data.unk;
      }

      entries.emplace_back( entry );
    }
    
    return entries;
  };

  const auto packet = ZoneChannelPacket< Client::FFXIVIpcGetCommonlist >( inPacket );
  auto& data = packet.data();

  auto listPacket = makeZonePacket< FFXIVIpcGetCommonlistResult >( player.getId() );
  listPacket->data().ListType = data.ListType;
  listPacket->data().RequestKey = data.RequestKey;
  listPacket->data().RequestParam = data.RequestParam;
  memset( listPacket->data().entries, 0, sizeof( listPacket->data().entries ) );

  std::vector< PlayerEntry > page;
  int offset = data.NextIndex;
  bool isLast = false;

  if( data.ListType == 0x02 )
  { // party list

    // disable pagination
    offset = 0;
    isLast = true;

    if( player.getPartyId() != 0 )
    {
      auto& partyMgr = Common::Service< World::Manager::PartyMgr >::ref();
      // send party members
      auto pParty = partyMgr.getParty( player.getPartyId() );
      assert( pParty );
      
      page = generateEntries( pParty->MemberId, offset, {} );

      // ensure first entry is the player requesting packet
      for( int i = 0; i < 8; ++i )
      {
        if( page[ i ].CharacterID == player.getCharacterId() )
        {
          std::swap( page[ 0 ], page[ i ] );
          break;
        }
      }

    }
    else
    {
      std::vector< uint32_t > soloParty = { player.getId() };
      page = generateEntries( soloParty, offset, {} );
    }
  }
  else if( data.ListType == 0x0b )
  { // friend list
    auto& friendList = player.getFriendListID();
    auto& friendListData = player.getFriendListData();

    std::vector< Common::HierarchyData > hierarchyData( friendListData.begin(), friendListData.end() );

    page = generateEntries( friendList, offset, hierarchyData );
  }
  else if( data.ListType == 0x0c )
  { // linkshell
    auto& lsMgr = Common::Service< LinkshellMgr >::ref();

    auto lsId = data.CommunityID;

    auto lsPtr = lsMgr.getLinkshellById( lsId );
    if( lsPtr )
    {
      auto& memberSet = lsPtr->getMemberIdList();

      std::vector< uint64_t > memberVec;
      std::copy( memberSet.begin(), memberSet.end(), std::back_inserter( memberVec ) );

      page = generateEntries( memberVec, offset, {} );
    }
  }
  else if( data.ListType == 0x0e )
  { // player search result
    auto queryPlayers = player.getLastPcSearchResult();
    page = generateEntries( queryPlayers, offset, {} );
  }

  // if we didn't manually terminate pagination (party, etc), check if we need to do so
  if( !isLast )
    isLast = page.empty();

  memcpy( &listPacket->data().entries[ 0 ], page.data(), sizeof( PlayerEntry ) * page.size() );

  listPacket->data().Index = offset;
  listPacket->data().NextIndex = isLast ? 0 : data.NextIndex + 10;

  queueOutPacket( listPacket );
}