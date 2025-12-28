#include "../game.h"
#include "systems.h"
#include <raylib.h>

void KillActor(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
               int idx) {
  if (!eng->em.alive[idx])
    return;

  eng->em.alive[idx] = 0;

  EntityType_t type = eng->actors.types[idx];

  switch (type) {

  case ENTITY_PLAYER:
    printf("PLAYER died!\n");
    EnableCursor();
    TriggerMessage(gs, "You died :C");
    gs->state = STATE_MAINMENU;
    break;

  case ENTITY_TURRET:
    printf("Turret destroyed\n");
    // SpawnExplosion(eng, eng->actors.positions[idx]);
    break;

  case ENTITY_DESTRUCT:
    printf("Destructible object destroyed\n");
    Vector3 pos =
        *(Vector3 *)getComponent(&eng->actors, idx, gs->compReg.cid_Positions);
    eng->actors.modelCollections[idx].isActive[0] = false;
    eng->actors.modelCollections[idx].isActive[1] = true;

    spawnParticle(eng, pos, 5, 2);

    QueueSound(soundSys, SOUND_EXPLOSION, pos, 10.0f, 1.0f);
    break;

  case ENTITY_WALL:
    printf("WALL object destroyed\n");
    // SpawnExplosion(eng, eng->actors.positions[idx]);
    break;

  case ENTITY_TANK:
    printf("Actor object destroyed\n");
    // SpawnExplosion(eng, eng->actors.positions[idx]);
    break;

  case ENTITY_MECH:
    printf("Mech destroyed\n");
    // SpawnExplosion(eng, eng->actors.positions[idx]);
    break;

  default:
    break;
  }
}

void KillEntity(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                entity_t id) {
  EntityCategory_t cat = GetEntityCategory(id);
  int idx = GetEntityIndex(id);

  switch (cat) {

  case ET_ACTOR:
    KillActor(gs, eng, soundSys, idx);
    break;

  case ET_STATIC:
    // KillStatic(gs, eng, idx);
    break;

  case ET_PROJECTILE:
    // normally already despawned elsewhere, but just in case:
    eng->projectiles.active[idx] = false;
    break;
  }
}
