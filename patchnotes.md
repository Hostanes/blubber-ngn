
refactored game.h into a more dynamic ECS style implementation

ADDED: EntityManager_t struct to track alive state, component masks, and counts
ADDED: struct as ECS style storage for all components instead of one monolithic
        struct
ADDED: ComponentMask_t and ComponentFlag_t for per entity component tracking
ADDED: entity_t typedef for entity indicies
ADDED: Component flags: C_POSITION, C_VELOCITY, C_MODEL, C_COLLISION, C_HITBOX, C_PLAYER_TAG.


and some other stuff
