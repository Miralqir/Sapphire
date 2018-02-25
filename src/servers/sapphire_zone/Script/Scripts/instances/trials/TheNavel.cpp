#include <ScriptObject.h>
#include <Zone/InstanceContent.h>

class TheNavel : public InstanceContentScript
{
public:
   TheNavel() : InstanceContentScript( 20002 )
   { }

   void onInit( InstanceContentPtr instance ) override
   {
      instance->registerEObj( "Exit", EXIT_OBJECT, 0, EXIT_OBJECT_STATE, { 0, 0, -10 } );
      instance->registerEObj( "Entrance", START_CIRCLE, START_CIRCLE_MAPLINK, START_CIRCLE_STATE, { 0, 0, 24 } );
   }

   void onUpdate( InstanceContentPtr instance, uint32_t currTime ) override
   {

   }

   void onEnterTerritory( Entity::Player &player, uint32_t eventId, uint16_t param1, uint16_t param2 ) override
   {

   }

private:
   static constexpr auto EXIT_OBJECT = 2000139;
   static constexpr auto EXIT_OBJECT_STATE = 4;

   static constexpr auto START_CIRCLE = 2000182;
   static constexpr auto START_CIRCLE_MAPLINK = 4236868;
   static constexpr auto START_CIRCLE_STATE = 5;
};