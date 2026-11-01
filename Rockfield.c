// =============================================================================
//   ROCKFIELD - an Asteroids-style game built with vector.h
// =============================================================================
//   Controls:  Left / Right  rotate          Up or A   thrust
//              B             fire            X         hyperspace
//              Start         start / pause
// =============================================================================

#define VEC_MAX_LINES 1300

#include "video.h"
#include "math.h"
#include "time.h"
#include "input.h"
#include "audio.h"
#include "misc.h"
#include "memcard.h"
#include "vector.h"

// -----------------------------------------------------------------------------
//   COLORS  (Vircon32 colors are 0xAABBGGRR)
// -----------------------------------------------------------------------------

#define SHIP_CORE      0xFFFFF0B0
#define SHIP_GLOW      0xFFB07020
#define ROCK_CORE      0xFFF0D8E8
#define ROCK_GLOW      0xFF904870
#define ARMOR_CORE     0xFFFFD8A0
#define ARMOR_GLOW     0xFF805030
#define BULLET_CORE    0xFFA0FFFF
#define BULLET_GLOW    0xFF2090A0
#define SAUCER_CORE    0xFF80FF90
#define SAUCER_GLOW    0xFF206820
#define ENEMY_SHOT     0xFF8080FF
#define ENEMY_GLOW     0xFF2020A0
#define SPARK_CORE     0xFF60C0FF
#define FLAME_CORE     0xFF40B0FF
#define FLAME_GLOW     0xFF103060
#define TEXT_CORE      0xFFFFFFFF
#define TEXT_GLOW      0xFF705050
#define DIM_TEXT       0xFFC09696

// -----------------------------------------------------------------------------
//   SOUNDS (same order as in Rockfield.xml) AND CHANNELS
// -----------------------------------------------------------------------------

#define SND_FIRE           0
#define SND_BANG_LARGE     1
#define SND_BANG_MEDIUM    2
#define SND_BANG_SMALL     3
#define SND_SHIP_EXPLODE   4
#define SND_THRUST         5
#define SND_SAUCER_BIG     6
#define SND_SAUCER_SMALL   7
#define SND_SAUCER_FIRE    8
#define SND_EXTRA_LIFE     9
#define SND_BEAT_LOW       10
#define SND_BEAT_HIGH      11
#define SND_HYPERSPACE     12
#define SND_START          13
#define SND_WAVE_CLEAR     14
#define SND_PAUSE          15
#define SND_SHIELD         16
#define SND_POWERUP        17
#define SND_ARMOR          18

#define CH_THRUST     0
#define CH_SAUCER     1
#define CH_SHIELD     2
#define CH_FIRST_SFX  3

int next_sfx_channel = CH_FIRST_SFX;
bool thrust_sound_on = false;
bool saucer_sound_on = false;
bool shield_sound_on = false;

// one-shot effect on the next channel in rotation (2..15)
void sfx_pitched( int sound_id, float volume, float speed )
{
    select_channel( next_sfx_channel );
    set_channel_volume( volume );
    set_channel_speed( speed );
    play_sound_in_channel( sound_id, next_sfx_channel );

    next_sfx_channel++;
    if( next_sfx_channel >= sound_channels )
      next_sfx_channel = CH_FIRST_SFX;
}

void sfx( int sound_id, float volume )
{
    sfx_pitched( sound_id, volume, 1.0 );
}

void set_thrust_sound( bool on )
{
    if( on == thrust_sound_on ) return;
    thrust_sound_on = on;

    if( on )
    {
        select_channel( CH_THRUST );
        set_channel_volume( 0.7 );
        play_sound_in_channel( SND_THRUST, CH_THRUST );
    }
    else stop_channel( CH_THRUST );
}

void set_saucer_sound( bool on, bool small )
{
    if( on == saucer_sound_on ) return;
    saucer_sound_on = on;

    if( on )
    {
        select_channel( CH_SAUCER );
        set_channel_volume( 0.6 );
        if( small ) play_sound_in_channel( SND_SAUCER_SMALL, CH_SAUCER );
        else        play_sound_in_channel( SND_SAUCER_BIG, CH_SAUCER );
    }
    else stop_channel( CH_SAUCER );
}

void set_shield_sound( bool on )
{
    if( on == shield_sound_on ) return;
    shield_sound_on = on;

    if( on )
    {
        select_channel( CH_SHIELD );
        set_channel_volume( 0.6 );
        play_sound_in_channel( SND_SHIELD, CH_SHIELD );
    }
    else stop_channel( CH_SHIELD );
}

void init_sounds()
{
    select_sound( SND_SHIELD );       set_sound_loop( true );
    select_sound( SND_THRUST );       set_sound_loop( true );
    select_sound( SND_SAUCER_BIG );   set_sound_loop( true );
    select_sound( SND_SAUCER_SMALL ); set_sound_loop( true );
}

// -----------------------------------------------------------------------------
//   HELPERS
// -----------------------------------------------------------------------------

// Two independent generators (xorshift32), so recorded games replay exactly:
//  - gameplay: reseeded at the start of every game; only game logic uses it
//  - effects:  screen shake and flame flicker, which also run while paused
int rng_state = 1;
int fx_state = 1;

int xorshift( int x )
{
    x = x ^ ( x << 13 );
    x = x ^ ( ( x >> 17 ) & 0x7FFF );
    x = x ^ ( x << 5 );
    return x;
}

void gseed( int seed )
{
    if( seed == 0 ) seed = 1;
    rng_state = seed;
}

// non-negative random integer for gameplay
int grand()
{
    rng_state = xorshift( rng_state );
    return rng_state & 0x7FFFFFFF;
}

int fxrand()
{
    fx_state = xorshift( fx_state );
    return fx_state & 0x7FFFFFFF;
}

float fx_range( float a, float b )
{
    return a + ( b - a ) * ( ( fxrand() % 10000 ) / 10000.0 );
}

float frand()
{
    return ( grand() % 10000 ) / 10000.0;
}

float frand_range( float a, float b )
{
    return a + ( b - a ) * frand();
}

// wraps a position into [0, limit)
float wrap( float v, float limit )
{
    if( v < 0 ) return v + limit;
    if( v >= limit ) return v - limit;
    return v;
}

// -----------------------------------------------------------------------------
//   GAME STATE
// -----------------------------------------------------------------------------

#define STATE_TITLE     0
#define STATE_PLAYING   1
#define STATE_GAMEOVER  2
#define STATE_DEMO      3     // attract mode: replaying a recorded game
#define STATE_ENTRY     4     // entering initials for the high score table

int state = STATE_TITLE;
int state_timer = 0;
int frame = 0;
int anim = 0;           // animation clock: game-relative during play, so replays match
bool paused = false;

int score;
#define DEFAULT_HIGH_SCORE 5000
int high_score = DEFAULT_HIGH_SCORE;
bool new_high_score;
int lives;
int level;
int next_extra_life;
int wave_frames;
int wave_banner;
int* wave_rule_text = "";
int next_wave_timer;
int beat_timer;
bool beat_high;

// title screen cycles through these pages, then plays the demo
#define PAGE_LOGO    0
#define PAGE_SCORES  1
int title_page = PAGE_LOGO;
int highlight_rank = -1;       // table row to highlight after entering initials

// -----------------------------------------------------------------------------
//   INPUT (live or replayed) AND RECORDING
// -----------------------------------------------------------------------------
//   Game logic never reads the gamepad directly: each logic frame gets a
//   6-bit input word, either from the gamepad (and recorded) or from a
//   recording (demo). With the same seed and inputs the game replays exactly.

#define IN_LEFT    1
#define IN_RIGHT   2
#define IN_THRUST  4
#define IN_FIRE    8
#define IN_HYPER   16
#define IN_SHIELD  32

#define REC_MAX_FRAMES  3600      // 60 s of gameplay
#define REC_WORDS       720       // 5 frames of 6 bits packed per word
#define DEMO_MAX_PLAY   2700      // attract demo runs 45 s at most
#define DEMO_MIN_FRAMES 600       // shorter games never become the demo

int in_bits = 0;
int fire_frames = 0;       // how long fire has been held (1 = just pressed)
int hyper_frames = 0;
int game_frame = 0;        // logic frames since the game started
int game_seed = 1;

int[ REC_WORDS ] rec_data;     // the game being played now
int rec_frames = 0;

bool demo_mode = false;
int demo_pos = 0;
int demo_seed = 0;             // the demo: best game recorded so far
int demo_frames = 0;
int demo_score = 0;
int[ REC_WORDS ] demo_data;

int read_gamepad_bits()
{
    int bits = 0;
    if( gamepad_left() > 0 )  bits = bits | IN_LEFT;
    if( gamepad_right() > 0 ) bits = bits | IN_RIGHT;
    if( gamepad_up() > 0 || gamepad_button_a() > 0 ) bits = bits | IN_THRUST;
    if( gamepad_button_b() > 0 ) bits = bits | IN_FIRE;
    if( gamepad_button_x() > 0 ) bits = bits | IN_HYPER;
    if( gamepad_button_y() > 0 ) bits = bits | IN_SHIELD;
    return bits;
}

void rec_add( int bits )
{
    if( rec_frames >= REC_MAX_FRAMES ) return;
    rec_data[ rec_frames / 5 ] = rec_data[ rec_frames / 5 ] | ( bits << ( ( rec_frames % 5 ) * 6 ) );
    rec_frames++;
}

int rec_get( int* data, int position )
{
    return ( data[ position / 5 ] >> ( ( position % 5 ) * 6 ) ) & 63;
}

void apply_input( int bits )
{
    in_bits = bits;

    if( bits & IN_FIRE ) fire_frames++;
    else fire_frames = 0;

    if( bits & IN_HYPER ) hyper_frames++;
    else hyper_frames = 0;
}

#include "demo_builtin.h"

void load_builtin_demo()
{
    if( builtin_demo_frames <= 0 ) return;
    memcpy( demo_data, builtin_demo_data, REC_WORDS );
    demo_seed = builtin_demo_seed;
    demo_frames = builtin_demo_frames;
    demo_score = builtin_demo_score;
}

bool in_game()
{
    return state == STATE_PLAYING || state == STATE_DEMO;
}

// -----------------------------------------------------------------------------
//   HIGH SCORE TABLE
// -----------------------------------------------------------------------------

#define HS_COUNT 5

int[ HS_COUNT ] hs_score = { 5000, 4000, 3000, 2000, 1000 };
int[ HS_COUNT ] hs_name;        // 3 characters packed, 8 bits each

int pack_name( int* text )
{
    return text[ 0 ] | ( text[ 1 ] << 8 ) | ( text[ 2 ] << 16 );
}

void unpack_name( int name, int* text )
{
    text[ 0 ] = name & 255;
    text[ 1 ] = ( name >> 8 ) & 255;
    text[ 2 ] = ( name >> 16 ) & 255;
    text[ 3 ] = 0;
}

void init_table()
{
    hs_name[ 0 ] = pack_name( "VEC" );
    hs_name[ 1 ] = pack_name( "TOR" );
    hs_name[ 2 ] = pack_name( "ROK" );
    hs_name[ 3 ] = pack_name( "ZAP" );
    hs_name[ 4 ] = pack_name( "UFO" );
}

// position a score would take in the table, or -1
int table_rank( int value )
{
    int i;
    for( i = 0; i < HS_COUNT; i++ )
      if( value > hs_score[ i ] ) return i;
    return -1;
}

void table_insert( int value, int name, int rank )
{
    int i;
    if( rank < 0 || rank >= HS_COUNT ) return;

    for( i = HS_COUNT - 1; i > rank; i-- )
    {
        hs_score[ i ] = hs_score[ i - 1 ];
        hs_name[ i ] = hs_name[ i - 1 ];
    }

    hs_score[ rank ] = value;
    hs_name[ rank ] = name;
}

// adds an entry unless the exact same one is already there
void table_merge( int value, int name )
{
    int i;
    for( i = 0; i < HS_COUNT; i++ )
      if( hs_score[ i ] == value && hs_name[ i ] == name ) return;
    table_insert( value, name, table_rank( value ) );
}

// -----------------------------------------------------------------------------
//   SCREEN EFFECTS
// -----------------------------------------------------------------------------

float world_dim = 1.0;    // whole-world brightness (dimmed while paused)
float shake = 0;
float flash = 0;
int flash_color = 0xFFFFFFFF;

void add_shake( float amount )
{
    if( amount > shake ) shake = amount;
}

void add_flash( float amount, int color )
{
    if( amount > flash )
    {
        flash = amount;
        flash_color = color;
    }
}


// -----------------------------------------------------------------------------
//   MEMORY CARD SAVE
// -----------------------------------------------------------------------------
//   Card layout (word offsets):
//     0-19   game signature (every Vircon32 game that saves writes one)
//     20     format version (3; 2 had the same layout but recorded games
//            from older rules, so only its table is kept; 1 held one score)
//     21-25  table scores           26-30  table names
//     31-33  demo seed, length in frames, score
//     34     checksum of words 21-33
//     35     checksum of the demo data
//     40-    demo recording (REC_WORDS words)
//   A card belonging to another game is never written to.

#define CARD_NONE      0     // no card inserted
#define CARD_OURS      1     // has a Rockfield save
#define CARD_EMPTY     2     // blank card, we can claim it
#define CARD_OTHER     3     // used by a different game: hands off

#define SAVE_OFFSET     20
#define SAVE_HEADER     16
#define SAVE_DEMO_AT    40
#define SAVE_VERSION    3
#define SAVE_KEY        0x5F3A9C17

int* save_signature_text = "ROCKFIELD VECTOR V1 ";   // exactly 20 characters
int card_state = CARD_NONE;
bool card_loaded = false;      // loaded the card currently inserted
int save_message = 0;          // what to tell the player after a game
int[ SAVE_HEADER ] save_header;
int[ REC_WORDS ] card_demo;

#define SAVE_MSG_NONE      0
#define SAVE_MSG_SAVED     1
#define SAVE_MSG_NO_CARD   2
#define SAVE_MSG_OTHER     3

int checksum( int* data, int count )
{
    int h = SAVE_KEY, i;
    for( i = 0; i < count; i++ )
      h = ( ( h << 5 ) | ( ( h >> 27 ) & 31 ) ) ^ data[ i ];
    return h;
}

// version 1 saves: [ 1, score, checksum ]
void load_old_save()
{
    if( save_header[ 2 ] == ( ( 1 * 31 + save_header[ 1 ] ) ^ SAVE_KEY ) )
      table_merge( save_header[ 1 ], pack_name( "---" ) );
}

void load_save()
{
    int i;

    card_read_data( save_header, SAVE_OFFSET, SAVE_HEADER );

    if( save_header[ 0 ] == 1 )
    {
        load_old_save();
        return;
    }

    if( save_header[ 0 ] != SAVE_VERSION && save_header[ 0 ] != 2 ) return;
    if( save_header[ 14 ] != checksum( &save_header[ 1 ], 13 ) ) return;

    for( i = 0; i < HS_COUNT; i++ )
      table_merge( save_header[ 1 + i ], save_header[ 6 + i ] );

    // a version 2 demo was recorded under older rules: it would not replay
    if( save_header[ 0 ] == 2 ) return;

    // take the card's demo if it is a better game than ours
    if( save_header[ 13 ] > demo_score && save_header[ 12 ] > 0 && save_header[ 12 ] <= REC_MAX_FRAMES )
    {
        card_read_data( card_demo, SAVE_DEMO_AT, REC_WORDS );
        if( save_header[ 15 ] == checksum( card_demo, REC_WORDS ) )
        {
            memcpy( demo_data, card_demo, REC_WORDS );
            demo_seed = save_header[ 11 ];
            demo_frames = save_header[ 12 ];
            demo_score = save_header[ 13 ];
        }
    }
}

// checks what card (if any) is inserted and loads our save
// the first time we see it, merging it with what we have
void refresh_card()
{
    if( !card_is_connected() )
    {
        card_state = CARD_NONE;
        card_loaded = false;
        return;
    }

    if( card_signature_matches( (game_signature*)save_signature_text ) )
      card_state = CARD_OURS;
    else if( card_is_empty() )
      card_state = CARD_EMPTY;
    else
      card_state = CARD_OTHER;

    if( card_state == CARD_OURS && !card_loaded )
    {
        load_save();
        high_score = hs_score[ 0 ];
    }

    card_loaded = ( card_state != CARD_NONE );
}

// writes table and demo; returns one of the SAVE_MSG values
int save_all()
{
    int i;
    refresh_card();

    if( card_state == CARD_NONE ) return SAVE_MSG_NO_CARD;
    if( card_state == CARD_OTHER ) return SAVE_MSG_OTHER;

    if( card_state == CARD_EMPTY )
      card_write_signature( (game_signature*)save_signature_text );

    save_header[ 0 ] = SAVE_VERSION;
    for( i = 0; i < HS_COUNT; i++ )
    {
        save_header[ 1 + i ] = hs_score[ i ];
        save_header[ 6 + i ] = hs_name[ i ];
    }
    save_header[ 11 ] = demo_seed;
    save_header[ 12 ] = demo_frames;
    save_header[ 13 ] = demo_score;
    save_header[ 14 ] = checksum( &save_header[ 1 ], 13 );
    save_header[ 15 ] = checksum( demo_data, REC_WORDS );

    card_write_data( demo_data, SAVE_DEMO_AT, REC_WORDS );
    card_write_data( save_header, SAVE_OFFSET, SAVE_HEADER );

    card_state = CARD_OURS;
    return SAVE_MSG_SAVED;
}

// -----------------------------------------------------------------------------
//   SHIP
// -----------------------------------------------------------------------------

float[ 4 ] ship_model_x = { 13, -9, -5, -9 };
float[ 4 ] ship_model_y = {  0, -8,  0,  8 };

float ship_x, ship_y, ship_vx, ship_vy, ship_angle;
bool ship_alive;
bool ship_thrusting;
int ship_respawn_timer;
int ship_invulnerable;
int ship_hyperspace;
int fire_cooldown;

// shield: hold Y; drains while up, recharges while down
#define SHIELD_MAX       300.0     // 5 seconds of continuous use
#define SHIELD_RADIUS    21
float shield_energy;
bool shield_up;

// power-up effects (frames left)
#define POWER_TIME       720       // 12 seconds
int spread_timer;
int rapid_timer;

// -----------------------------------------------------------------------------
//   ROCKS
// -----------------------------------------------------------------------------

#define MAX_ROCKS     40
#define ROCK_SHAPES   8
#define ROCK_POINTS   9

float[ ROCK_SHAPES * ROCK_POINTS ] rock_shape_x;
float[ ROCK_SHAPES * ROCK_POINTS ] rock_shape_y;

bool[ MAX_ROCKS ] rock_active;
float[ MAX_ROCKS ] rock_x;
float[ MAX_ROCKS ] rock_y;
float[ MAX_ROCKS ] rock_vx;
float[ MAX_ROCKS ] rock_vy;
float[ MAX_ROCKS ] rock_angle;
float[ MAX_ROCKS ] rock_spin;
int[ MAX_ROCKS ] rock_size;      // 3 = large, 2 = medium, 1 = small
int[ MAX_ROCKS ] rock_shape;
int[ MAX_ROCKS ] rock_flash;     // frames left drawn bright (after splitting)
int[ MAX_ROCKS ] rock_hp;        // hits left: armored rocks start with 2
bool[ MAX_ROCKS ] rock_armored;  // worth double, drawn with plating

float rock_radius( int size )
{
    if( size == 3 ) return 34;
    if( size == 2 ) return 19;
    return 10;
}

void build_rock_shapes()
{
    int s, p;
    float a, r;

    for( s = 0; s < ROCK_SHAPES; s++ )
      for( p = 0; p < ROCK_POINTS; p++ )
      {
          a = p * 6.2831853 / ROCK_POINTS + frand_range( -0.15, 0.15 );
          r = frand_range( 0.62, 1.1 );
          rock_shape_x[ s * ROCK_POINTS + p ] = r * cos( a );
          rock_shape_y[ s * ROCK_POINTS + p ] = r * sin( a );
      }
}

int spawn_rock( float x, float y, int size, float speed_bonus )
{
    int i;
    float a, speed;

    for( i = 0; i < MAX_ROCKS; i++ )
      if( !rock_active[ i ] )
      {
          a = frand() * 6.2831853;
          speed = frand_range( 0.4, 1.0 ) * ( 4 - size ) * 0.6 * ( 1.0 + speed_bonus );
          rock_active[ i ] = true;
          rock_x[ i ] = wrap( x, screen_width );
          rock_y[ i ] = wrap( y, screen_height );
          rock_vx[ i ] = cos( a ) * speed;
          rock_vy[ i ] = sin( a ) * speed;
          rock_angle[ i ] = frand() * 6.2831853;
          rock_spin[ i ] = frand_range( -0.03, 0.03 );
          rock_size[ i ] = size;
          rock_shape[ i ] = grand() % ROCK_SHAPES;
          rock_flash[ i ] = 0;
          rock_hp[ i ] = 1;
          rock_armored[ i ] = false;
          return i;
      }

    return -1;
}

int count_rocks()
{
    int i, n = 0;
    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] ) n++;
    return n;
}

void clear_rocks()
{
    int i;
    for( i = 0; i < MAX_ROCKS; i++ ) rock_active[ i ] = false;
}

// large rocks along the edges, away from the centre
float wave_speed_bonus()
{
    return fmin( ( level - 1 ) * 0.08, 0.8 );
}

void spawn_wave_rocks( int amount )
{
    int i, k;
    float x, y;

    for( i = 0; i < amount; i++ )
    {
        if( grand() % 2 )
        {
            x = frand() * screen_width;
            y = frand_range( -40, 40 );
        }
        else
        {
            x = frand_range( -40, 40 );
            y = frand() * screen_height;
        }

        k = spawn_rock( x, y, 3, wave_speed_bonus() );

        // armored rocks from wave 4, more of them each wave
        if( k >= 0 && level >= 4 && grand() % 100 < min( 15 * ( level - 3 ), 60 ) )
        {
            rock_hp[ k ] = 2;
            rock_armored[ k ] = true;
        }
    }
}

// -----------------------------------------------------------------------------
//   PLAYER BULLETS
// -----------------------------------------------------------------------------

#define MAX_BULLETS 16

int[ MAX_BULLETS ] bullet_life;
float[ MAX_BULLETS ] bullet_x;
float[ MAX_BULLETS ] bullet_y;
float[ MAX_BULLETS ] bullet_vx;
float[ MAX_BULLETS ] bullet_vy;

int count_bullets()
{
    int i, n = 0;
    for( i = 0; i < MAX_BULLETS; i++ )
      if( bullet_life[ i ] > 0 ) n++;
    return n;
}

void fire_one_bullet( float angle )
{
    int i;
    float c = cos( angle ), s = sin( angle );

    for( i = 0; i < MAX_BULLETS; i++ )
      if( bullet_life[ i ] <= 0 )
      {
          bullet_life[ i ] = 48;
          bullet_x[ i ] = ship_x + c * 14;
          bullet_y[ i ] = ship_y + s * 14;
          bullet_vx[ i ] = ship_vx + c * 7.0;
          bullet_vy[ i ] = ship_vy + s * 7.0;
          return;
      }
}

void fire_bullet()
{
    // 8 bullets on screen normally; power-ups lift the limit
    if( spread_timer == 0 && rapid_timer == 0 && count_bullets() >= 8 ) return;

    fire_one_bullet( ship_angle );

    if( spread_timer > 0 )
    {
        fire_one_bullet( ship_angle - 0.2 );
        fire_one_bullet( ship_angle + 0.2 );
    }

    sfx_pitched( SND_FIRE, 0.5, frand_range( 0.95, 1.05 ) );
}

// -----------------------------------------------------------------------------
//   SAUCER AND ITS BULLETS
// -----------------------------------------------------------------------------

// model: hull outline (closed), middle line, dome (open)
float[ 6 ] saucer_hull_x = { -1.0, -0.45, 0.45, 1.0, 0.45, -0.45 };
float[ 6 ] saucer_hull_y = {  0.0, -0.32, -0.32, 0.0, 0.32, 0.32 };
float[ 4 ] saucer_dome_x = { -0.45, -0.25, 0.25, 0.45 };
float[ 4 ] saucer_dome_y = { -0.32, -0.62, -0.62, -0.32 };

bool saucer_active = false;
bool saucer_small;
float saucer_x, saucer_y, saucer_vx, saucer_vy;
int saucer_fire_timer;
int saucer_turn_timer;
int saucer_spawn_timer;

#define MAX_ENEMY_SHOTS 6

int[ MAX_ENEMY_SHOTS ] eshot_life;
float[ MAX_ENEMY_SHOTS ] eshot_x;
float[ MAX_ENEMY_SHOTS ] eshot_y;
float[ MAX_ENEMY_SHOTS ] eshot_vx;
float[ MAX_ENEMY_SHOTS ] eshot_vy;

float saucer_size()
{
    if( saucer_small ) return 11;
    return 20;
}

void reset_saucer_timer()
{
    saucer_spawn_timer = 600 + grand() % 600 - level * 30;
    if( saucer_spawn_timer < 360 ) saucer_spawn_timer = 360;
}

void spawn_saucer()
{
    float speed;

    // small saucers get more likely as the game goes on
    saucer_small = ( grand() % 100 ) < ( level * 12 + score / 500 );
    if( score < 3000 ) saucer_small = false;

    speed = 1.5;
    if( saucer_small ) speed = 2.1;

    saucer_active = true;
    saucer_y = frand_range( 50, screen_height - 50 );

    if( grand() % 2 )
    {
        saucer_x = -25;
        saucer_vx = speed;
    }
    else
    {
        saucer_x = screen_width + 25;
        saucer_vx = -speed;
    }

    saucer_vy = 0;
    saucer_fire_timer = 50;
    saucer_turn_timer = 80;
    set_saucer_sound( true, saucer_small );
}

void remove_saucer()
{
    saucer_active = false;
    set_saucer_sound( false, false );
    reset_saucer_timer();
}

void saucer_fire()
{
    int i;
    float a, dx, dy, error;

    for( i = 0; i < MAX_ENEMY_SHOTS; i++ )
      if( eshot_life[ i ] <= 0 )
      {
          if( saucer_small && ship_alive && ship_hyperspace == 0 )
          {
              // aimed shot, more accurate in later waves
              dx = ship_x - saucer_x;
              dy = ship_y - saucer_y;
              error = 0.5 - level * 0.06;
              if( error < 0.08 ) error = 0.08;
              a = atan2( dy, dx ) + frand_range( -error, error );
          }
          else a = frand() * 6.2831853;

          eshot_life[ i ] = 70;
          eshot_x[ i ] = saucer_x;
          eshot_y[ i ] = saucer_y;
          eshot_vx[ i ] = cos( a ) * 4.2;
          eshot_vy[ i ] = sin( a ) * 4.2;
          sfx( SND_SAUCER_FIRE, 0.45 );
          return;
      }
}

// -----------------------------------------------------------------------------
//   PARTICLES, SHOCKWAVE RINGS AND SCORE POPUPS
// -----------------------------------------------------------------------------

#define MAX_PARTICLES 110

int[ MAX_PARTICLES ] part_life;
int[ MAX_PARTICLES ] part_max_life;
float[ MAX_PARTICLES ] part_x;
float[ MAX_PARTICLES ] part_y;
float[ MAX_PARTICLES ] part_vx;
float[ MAX_PARTICLES ] part_vy;
float[ MAX_PARTICLES ] part_angle;
float[ MAX_PARTICLES ] part_spin;
float[ MAX_PARTICLES ] part_length;
int[ MAX_PARTICLES ] part_color;
int next_particle = 0;

void spawn_particle_v( float x, float y, float vx, float vy, float length, int life, int color )
{
    // round robin: when full, the oldest particles get recycled
    int i = next_particle;
    next_particle++;
    if( next_particle >= MAX_PARTICLES ) next_particle = 0;

    part_life[ i ] = life;
    part_max_life[ i ] = life;
    part_x[ i ] = x;
    part_y[ i ] = y;
    part_vx[ i ] = vx;
    part_vy[ i ] = vy;
    part_angle[ i ] = frand() * 6.2831853;
    part_spin[ i ] = frand_range( -0.2, 0.2 );
    part_length[ i ] = length;
    part_color[ i ] = color;
}

void explode( float x, float y, int amount, float speed, float length, int color )
{
    int i;
    float a, v;

    for( i = 0; i < amount; i++ )
    {
        a = frand() * 6.2831853;
        v = speed * frand_range( 0.3, 1.0 );
        spawn_particle_v( x, y, cos( a ) * v, sin( a ) * v, length, 30 + grand() % 30, color );
    }
}

#define MAX_RINGS 6

int[ MAX_RINGS ] ring_life;
int[ MAX_RINGS ] ring_max_life;
float[ MAX_RINGS ] ring_x;
float[ MAX_RINGS ] ring_y;
float[ MAX_RINGS ] ring_radius;
int[ MAX_RINGS ] ring_color;
int next_ring = 0;

void spawn_ring( float x, float y, float radius, int life, int color )
{
    int i = next_ring;
    next_ring++;
    if( next_ring >= MAX_RINGS ) next_ring = 0;

    ring_life[ i ] = life;
    ring_max_life[ i ] = life;
    ring_x[ i ] = x;
    ring_y[ i ] = y;
    ring_radius[ i ] = radius;
    ring_color[ i ] = color;
}

#define MAX_POPUPS 8

int[ MAX_POPUPS ] popup_life;
int[ MAX_POPUPS ] popup_value;
float[ MAX_POPUPS ] popup_x;
float[ MAX_POPUPS ] popup_y;
int next_popup = 0;

void spawn_popup( float x, float y, int value )
{
    int i = next_popup;
    next_popup++;
    if( next_popup >= MAX_POPUPS ) next_popup = 0;

    popup_life[ i ] = 50;
    popup_value[ i ] = value;
    popup_x[ i ] = x;
    popup_y[ i ] = y;
}

void clear_effects()
{
    int i;
    for( i = 0; i < MAX_PARTICLES; i++ ) part_life[ i ] = 0;
    for( i = 0; i < MAX_RINGS; i++ ) ring_life[ i ] = 0;
    for( i = 0; i < MAX_POPUPS; i++ ) popup_life[ i ] = 0;
    for( i = 0; i < MAX_BULLETS; i++ ) bullet_life[ i ] = 0;
    for( i = 0; i < MAX_ENEMY_SHOTS; i++ ) eshot_life[ i ] = 0;
    next_particle = 0;
    next_ring = 0;
    next_popup = 0;
}

// -----------------------------------------------------------------------------
//   POWER-UP PICKUPS
// -----------------------------------------------------------------------------

#define PU_SPREAD   0
#define PU_RAPID    1
#define PU_ENERGY   2

#define MAX_PICKUPS 3

float[ 4 ] pickup_shape_x = { 1, 0, -1, 0 };
float[ 4 ] pickup_shape_y = { 0, 1, 0, -1 };
#define PICKUP_LIFE 600            // 10 seconds before it fades away

bool[ MAX_PICKUPS ] pickup_active;
int[ MAX_PICKUPS ] pickup_type;
int[ MAX_PICKUPS ] pickup_life;
float[ MAX_PICKUPS ] pickup_x;
float[ MAX_PICKUPS ] pickup_y;
float[ MAX_PICKUPS ] pickup_vx;
float[ MAX_PICKUPS ] pickup_vy;

void clear_pickups()
{
    int i;
    for( i = 0; i < MAX_PICKUPS; i++ ) pickup_active[ i ] = false;
}

void spawn_pickup( float x, float y )
{
    int i, roll;
    float a;

    for( i = 0; i < MAX_PICKUPS; i++ )
      if( !pickup_active[ i ] )
      {
          roll = grand() % 100;
          pickup_type[ i ] = PU_ENERGY;
          if( roll < 40 ) pickup_type[ i ] = PU_SPREAD;
          else if( roll < 80 ) pickup_type[ i ] = PU_RAPID;

          a = frand() * 6.2831853;
          pickup_active[ i ] = true;
          pickup_life[ i ] = PICKUP_LIFE;
          pickup_x[ i ] = x;
          pickup_y[ i ] = y;
          pickup_vx[ i ] = cos( a ) * 0.5;
          pickup_vy[ i ] = sin( a ) * 0.5;
          return;
      }
}

int pickup_color( int type )
{
    if( type == PU_SPREAD ) return 0xFFFF90FF;     // pink
    if( type == PU_RAPID )  return 0xFF60F0FF;     // yellow
    return 0xFFFFE060;                             // cyan
}

void collect_pickup( int i )
{
    int type = pickup_type[ i ];
    pickup_active[ i ] = false;

    if( type == PU_SPREAD ) spread_timer = POWER_TIME;
    else if( type == PU_RAPID ) rapid_timer = POWER_TIME;
    else shield_energy = SHIELD_MAX;

    sfx( SND_POWERUP, 0.7 );
    spawn_ring( pickup_x[ i ], pickup_y[ i ], 40, 18, pickup_color( type ) );
    add_flash( 0.15, pickup_color( type ) );
}

void update_pickups()
{
    int i;
    float dx, dy;

    for( i = 0; i < MAX_PICKUPS; i++ )
      if( pickup_active[ i ] )
      {
          pickup_x[ i ] = wrap( pickup_x[ i ] + pickup_vx[ i ], screen_width );
          pickup_y[ i ] = wrap( pickup_y[ i ] + pickup_vy[ i ], screen_height );
          pickup_life[ i ]--;
          if( pickup_life[ i ] <= 0 )
          {
              pickup_active[ i ] = false;
              continue;
          }

          if( ship_alive && ship_hyperspace == 0 )
          {
              dx = pickup_x[ i ] - ship_x;
              dy = pickup_y[ i ] - ship_y;
              if( dx * dx + dy * dy < 20 * 20 ) collect_pickup( i );
          }
      }
}

// -----------------------------------------------------------------------------
//   SCORING
// -----------------------------------------------------------------------------

void add_score( int points, float x, float y )
{
    score += points;
    spawn_popup( x, y, points );

    if( !demo_mode && score > high_score )
    {
        high_score = score;
        new_high_score = true;
    }

    if( score >= next_extra_life )
    {
        next_extra_life += 10000;
        lives++;
        sfx( SND_EXTRA_LIFE, 0.6 );
        add_flash( 0.25, 0xFF80FFFF );
    }
}

// -----------------------------------------------------------------------------
//   DESTRUCTION
// -----------------------------------------------------------------------------

void destroy_rock( int i, bool by_player )
{
    int size = rock_size[ i ];
    int k, child, pieces, points;
    bool armored = rock_armored[ i ];
    float px = rock_x[ i ], py = rock_y[ i ];
    rock_active[ i ] = false;

    if( by_player )
    {
        // armored rocks are worth double
        points = 100;
        if( size == 3 ) points = 20;
        else if( size == 2 ) points = 50;
        if( armored ) points *= 2;
        add_score( points, px, py );

        // occasional power-up drop, bigger rocks more likely
        if( grand() % 100 < 1 + size * 2 ) spawn_pickup( px, py );
    }

    explode( rock_x[ i ], rock_y[ i ], 4 + size * 3, 1.0 + size * 0.5, 3 + size * 2, ROCK_CORE );

    if( size == 3 )
    {
        sfx_pitched( SND_BANG_LARGE, 0.8, frand_range( 0.9, 1.1 ) );
        spawn_ring( rock_x[ i ], rock_y[ i ], 60, 22, ROCK_CORE );
        add_shake( 5 );
    }
    else if( size == 2 )
    {
        sfx_pitched( SND_BANG_MEDIUM, 0.7, frand_range( 0.9, 1.1 ) );
        spawn_ring( rock_x[ i ], rock_y[ i ], 34, 16, ROCK_CORE );
        add_shake( 2.5 );
    }
    else
    {
        sfx_pitched( SND_BANG_SMALL, 0.6, frand_range( 0.9, 1.15 ) );
        add_shake( 1 );
    }

    // from wave 3, large rocks break into three
    pieces = 2;
    if( size == 3 && level >= 3 ) pieces = 3;

    if( size > 1 )
      for( k = 0; k < pieces; k++ )
      {
          // the pieces flash briefly as they break off
          child = spawn_rock( px, py, size - 1, wave_speed_bonus() );
          if( child >= 0 ) rock_flash[ child ] = 8;
      }
}

// a player's shot hitting a rock: armor absorbs the first hit
void hit_rock( int i, float push_x, float push_y )
{
    if( rock_hp[ i ] > 1 )
    {
        rock_hp[ i ]--;
        rock_flash[ i ] = -10;       // negative: sparks where the armor cracked
        rock_vx[ i ] += push_x * 0.04;
        rock_vy[ i ] += push_y * 0.04;
        explode( rock_x[ i ], rock_y[ i ], 6, 2.2, 3, 0xFFFFD0A0 );
        sfx_pitched( SND_ARMOR, 0.7, frand_range( 0.9, 1.1 ) );
        add_shake( 1.5 );
        return;
    }

    destroy_rock( i, true );
}

void destroy_saucer( bool by_player )
{
    if( by_player )
    {
        if( saucer_small ) add_score( 1000, saucer_x, saucer_y );
        else add_score( 200, saucer_x, saucer_y );
        spawn_pickup( saucer_x, saucer_y );      // saucers always drop one
    }

    explode( saucer_x, saucer_y, 18, 2.6, 7, SAUCER_CORE );
    spawn_ring( saucer_x, saucer_y, 50, 20, SAUCER_CORE );
    sfx( SND_BANG_MEDIUM, 0.8 );
    add_shake( 4 );
    remove_saucer();
}

void kill_ship()
{
    int i;
    float a;

    ship_alive = false;
    ship_respawn_timer = 110;
    shield_up = false;
    spread_timer = 0;
    rapid_timer = 0;
    set_thrust_sound( false );
    set_shield_sound( false );

    // debris: the ship's own outline flying apart, plus sparks
    for( i = 0; i < 6; i++ )
    {
        a = frand() * 6.2831853;
        spawn_particle_v( ship_x, ship_y, ship_vx * 0.5 + cos( a ) * 1.2, ship_vy * 0.5 + sin( a ) * 1.2,
                          12, 70 + grand() % 30, SHIP_CORE );
    }
    explode( ship_x, ship_y, 22, 3.0, 4, SPARK_CORE );
    spawn_ring( ship_x, ship_y, 90, 30, SHIP_CORE );

    sfx( SND_SHIP_EXPLODE, 1.0 );
    add_shake( 14 );
    add_flash( 0.45, 0xFFFFE0C0 );

    lives--;
}

// -----------------------------------------------------------------------------
//   GAME FLOW
// -----------------------------------------------------------------------------

void enter_state( int new_state )
{
    state = new_state;
    state_timer = 0;
}

void reset_ship()
{
    ship_x = screen_width / 2;
    ship_y = screen_height / 2;
    ship_vx = 0;
    ship_vy = 0;
    ship_angle = -1.5707963;
    ship_alive = true;
    ship_invulnerable = 120;
    ship_hyperspace = 0;
    shield_energy = SHIELD_MAX;
    shield_up = false;
}

void start_wave()
{
    spawn_wave_rocks( 3 + level );
    wave_frames = 0;
    wave_banner = 150;

    wave_rule_text = "";
    if( level == 3 ) wave_rule_text = "NEW - LARGE ROCKS SPLIT IN THREE";
    if( level == 4 ) wave_rule_text = "NEW - ARMORED ROCKS TAKE TWO HITS";
    if( level >= 4 ) wave_banner = 190;
    beat_timer = 60;
}

// -----------------------------------------------------------------------------
//   INITIALS ENTRY
// -----------------------------------------------------------------------------

void enter_title_page( int page );

int* entry_charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789- ";
#define ENTRY_CHARS 38

int[ 3 ] entry_index;
int entry_cursor;
int entry_rank;

void start_entry()
{
    entry_index[ 0 ] = 0;
    entry_index[ 1 ] = 0;
    entry_index[ 2 ] = 0;
    entry_cursor = 0;
    entry_rank = table_rank( score );
    sfx( SND_EXTRA_LIFE, 0.6 );
    enter_state( STATE_ENTRY );
}

// held buttons repeat after a short delay
bool pressed_or_repeat( int held_frames )
{
    return held_frames == 1 || ( held_frames > 18 && held_frames % 5 == 0 );
}

void finish_entry()
{
    int[ 4 ] name;
    name[ 0 ] = entry_charset[ entry_index[ 0 ] ];
    name[ 1 ] = entry_charset[ entry_index[ 1 ] ];
    name[ 2 ] = entry_charset[ entry_index[ 2 ] ];
    name[ 3 ] = 0;

    table_insert( score, pack_name( name ), entry_rank );
    high_score = hs_score[ 0 ];
    save_message = save_all();
    sfx( SND_START, 0.6 );

    highlight_rank = entry_rank;
    enter_title_page( PAGE_SCORES );
}

void update_entry()
{
    int i = entry_cursor;

    if( pressed_or_repeat( gamepad_up() ) )
    {
        entry_index[ i ]++;
        if( entry_index[ i ] >= ENTRY_CHARS ) entry_index[ i ] = 0;
        sfx( SND_FIRE, 0.25 );
    }

    if( pressed_or_repeat( gamepad_down() ) )
    {
        entry_index[ i ]--;
        if( entry_index[ i ] < 0 ) entry_index[ i ] = ENTRY_CHARS - 1;
        sfx( SND_FIRE, 0.25 );
    }

    if( gamepad_left() == 1 && entry_cursor > 0 ) entry_cursor--;
    if( gamepad_right() == 1 && entry_cursor < 2 ) entry_cursor++;

    if( gamepad_button_b() == 1 || gamepad_button_a() == 1 )
    {
        if( entry_cursor < 2 )
        {
            entry_cursor++;
            sfx( SND_PAUSE, 0.4 );
        }
        else
        {
            finish_entry();
            return;
        }
    }

    // Start confirms right away; after 30 s it confirms by itself
    if( gamepad_button_start() == 1 || state_timer > 1800 )
      finish_entry();
}

// resets every piece of gameplay state, then seeds the gameplay RNG:
// a live game and its replay must start out bit-for-bit identical
void begin_game( int seed, bool demo )
{
    int i;

    // forget the title screen's world without touching the RNG
    saucer_active = false;
    set_saucer_sound( false, false );
    set_thrust_sound( false );
    clear_rocks();
    clear_effects();

    game_seed = seed;
    gseed( seed );
    fx_state = seed ^ 0x2545F491;
    if( fx_state == 0 ) fx_state = 1;

    demo_mode = demo;
    demo_pos = 0;
    game_frame = 0;
    rec_frames = 0;
    for( i = 0; i < REC_WORDS; i++ ) rec_data[ i ] = 0;
    in_bits = 0;
    fire_frames = 0;
    hyper_frames = 0;
    spread_timer = 0;
    rapid_timer = 0;
    clear_pickups();
    set_shield_sound( false );

    score = 0;
    new_high_score = false;
    high_score = hs_score[ 0 ];
    lives = 3;
    level = 1;
    next_extra_life = 10000;
    next_wave_timer = 0;
    fire_cooldown = 0;
    paused = false;
    beat_high = false;
    ship_thrusting = false;
    ship_respawn_timer = 0;
    shake = 0;
    flash = 0;
    save_message = SAVE_MSG_NONE;

    reset_ship();
    reset_saucer_timer();
    start_wave();

    if( demo )
    {
        set_global_volume( 0 );        // the attract demo plays silently
        enter_state( STATE_DEMO );
    }
    else
    {
        set_global_volume( 0.85 );
        sfx( SND_START, 0.7 );
        enter_state( STATE_PLAYING );
    }
}

void new_game()
{
    // hardware RNG (seeded from the clock) picks the game's seed
    begin_game( rand() % 2000000000 + 1, false );
}

// title screen: a few rocks drifting in the background
void enter_title_page( int page )
{
    int i;

    clear_rocks();
    clear_effects();
    if( saucer_active ) remove_saucer();
    stop_all_channels();
    thrust_sound_on = false;
    saucer_sound_on = false;
    shield_sound_on = false;
    clear_pickups();
    set_global_volume( 0.85 );
    demo_mode = false;
    level = 1;
    shake = 0;
    flash = 0;

    for( i = 0; i < 7; i++ )
      spawn_rock( frand() * screen_width, frand() * screen_height, 1 + grand() % 3, 0 );

    title_page = page;
    enter_state( STATE_TITLE );
}

void enter_title()
{
    highlight_rank = -1;
    enter_title_page( PAGE_LOGO );
}

// a live game just ended: keep its recording if it's the best game
// so far, then either ask for initials or show the game over screen
void finish_game()
{
    bool demo_changed = false;

    if( saucer_active ) remove_saucer();
    set_thrust_sound( false );
    set_shield_sound( false );

    if( score > demo_score && rec_frames >= DEMO_MIN_FRAMES )
    {
        memcpy( demo_data, rec_data, REC_WORDS );
        demo_seed = game_seed;
        demo_frames = rec_frames;
        demo_score = score;
        demo_changed = true;
    }

    if( table_rank( score ) >= 0 )
    {
        start_entry();
        return;
    }

    save_message = SAVE_MSG_NONE;
    if( demo_changed ) save_message = save_all();
    enter_state( STATE_GAMEOVER );
}

// the centre must be clear before the ship comes back
bool spawn_area_clear()
{
    int i;
    float dx, dy, r;

    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
      {
          dx = rock_x[ i ] - screen_width / 2;
          dy = rock_y[ i ] - screen_height / 2;
          r = rock_radius( rock_size[ i ] ) + 60;
          if( dx * dx + dy * dy < r * r ) return false;
      }

    if( saucer_active )
    {
        dx = saucer_x - screen_width / 2;
        dy = saucer_y - screen_height / 2;
        if( dx * dx + dy * dy < 100 * 100 ) return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
//   UPDATE: SHIP
// -----------------------------------------------------------------------------

void update_ship()
{
    float speed, c, s;

    if( !ship_alive )
    {
        if( ship_respawn_timer > 0 ) ship_respawn_timer--;
        else if( lives > 0 && spawn_area_clear() ) reset_ship();
        return;
    }

    // hyperspace: vanish, then reappear somewhere random
    if( ship_hyperspace > 0 )
    {
        ship_hyperspace--;
        if( ship_hyperspace == 0 )
        {
            ship_x = frand_range( 40, screen_width - 40 );
            ship_y = frand_range( 40, screen_height - 40 );
            ship_vx = 0;
            ship_vy = 0;
            spawn_ring( ship_x, ship_y, 30, 14, 0xFFFFC080 );
            add_flash( 0.2, 0xFFFFA060 );
        }
        return;
    }

    if( hyper_frames == 1 )
    {
        ship_hyperspace = 40;
        shield_up = false;
        set_shield_sound( false );
        ship_thrusting = false;
        set_thrust_sound( false );
        spawn_ring( ship_x, ship_y, 30, 14, 0xFFFFC080 );
        explode( ship_x, ship_y, 8, 1.5, 3, 0xFFFFC080 );
        sfx( SND_HYPERSPACE, 0.6 );
        return;
    }

    if( in_bits & IN_LEFT )  ship_angle -= 0.075;
    if( in_bits & IN_RIGHT ) ship_angle += 0.075;

    ship_thrusting = ( in_bits & IN_THRUST ) != 0;
    set_thrust_sound( ship_thrusting );

    c = cos( ship_angle );
    s = sin( ship_angle );

    if( ship_thrusting )
    {
        ship_vx += c * 0.12;
        ship_vy += s * 0.12;

        // exhaust sparks
        spawn_particle_v( ship_x - c * 8, ship_y - s * 8,
                          ship_vx - c * frand_range( 1.5, 3.0 ) + frand_range( -0.4, 0.4 ),
                          ship_vy - s * frand_range( 1.5, 3.0 ) + frand_range( -0.4, 0.4 ),
                          2.5, 12 + grand() % 8, FLAME_CORE );
    }

    // limit speed, apply drag
    speed = sqrt( ship_vx * ship_vx + ship_vy * ship_vy );
    if( speed > 5.5 )
    {
        ship_vx *= 5.5 / speed;
        ship_vy *= 5.5 / speed;
    }
    ship_vx *= 0.992;
    ship_vy *= 0.992;

    ship_x = wrap( ship_x + ship_vx, screen_width );
    ship_y = wrap( ship_y + ship_vy, screen_height );

    // fire on press, auto-repeat while held (faster with rapid fire)
    if( fire_cooldown > 0 ) fire_cooldown--;
    if( fire_frames == 1 || ( fire_frames > 0 && fire_cooldown == 0 ) )
    {
        fire_bullet();
        fire_cooldown = 10;
        if( rapid_timer > 0 ) fire_cooldown = 4;
    }

    if( spread_timer > 0 ) spread_timer--;
    if( rapid_timer > 0 ) rapid_timer--;

    // shield: up while held and there is energy left
    shield_up = ( in_bits & IN_SHIELD ) != 0 && shield_energy > 0;
    if( shield_up ) shield_energy -= 1;
    else shield_energy = fmin( SHIELD_MAX, shield_energy + 0.35 );
    set_shield_sound( shield_up );

    if( ship_invulnerable > 0 ) ship_invulnerable--;
}

bool ship_vulnerable()
{
    return ship_alive && ship_invulnerable == 0 && ship_hyperspace == 0 && !shield_up;
}

// -----------------------------------------------------------------------------
//   UPDATE: SAUCER
// -----------------------------------------------------------------------------

void update_saucer()
{
    int i;
    float dx, dy, r;

    if( !saucer_active )
    {
        // only appear while the player is around to fight it
        if( in_game() && ship_alive && next_wave_timer == 0 )
        {
            saucer_spawn_timer--;
            if( saucer_spawn_timer <= 0 ) spawn_saucer();
        }
        return;
    }

    saucer_x += saucer_vx;
    saucer_y = wrap( saucer_y + saucer_vy, screen_height );

    // zig-zag
    saucer_turn_timer--;
    if( saucer_turn_timer <= 0 )
    {
        saucer_turn_timer = 50 + grand() % 70;
        saucer_vy = ( ( grand() % 3 ) - 1 ) * fabs( saucer_vx ) * 0.8;
    }

    saucer_fire_timer--;
    if( saucer_fire_timer <= 0 )
    {
        saucer_fire();
        saucer_fire_timer = 70;
        if( saucer_small ) saucer_fire_timer = 50;
    }

    // left the screen
    if( saucer_x < -40 || saucer_x > screen_width + 40 )
    {
        remove_saucer();
        return;
    }

    // collisions with rocks: both break, no points
    r = saucer_size() * 0.8;
    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
      {
          dx = saucer_x - rock_x[ i ];
          dy = saucer_y - rock_y[ i ];
          if( dx * dx + dy * dy < ( r + rock_radius( rock_size[ i ] ) ) * ( r + rock_radius( rock_size[ i ] ) ) )
          {
              destroy_rock( i, false );
              destroy_saucer( false );
              return;
          }
      }

    // collision with the ship
    if( ship_vulnerable() )
    {
        dx = saucer_x - ship_x;
        dy = saucer_y - ship_y;
        if( dx * dx + dy * dy < ( r + 8 ) * ( r + 8 ) )
        {
            destroy_saucer( true );
            kill_ship();
        }
    }
}

// -----------------------------------------------------------------------------
//   UPDATE: WORLD
// -----------------------------------------------------------------------------

void update_world()
{
    int i, b;
    float dx, dy, r;

    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
      {
          rock_x[ i ] = wrap( rock_x[ i ] + rock_vx[ i ], screen_width );
          rock_y[ i ] = wrap( rock_y[ i ] + rock_vy[ i ], screen_height );
          rock_angle[ i ] += rock_spin[ i ];
          if( rock_flash[ i ] > 0 ) rock_flash[ i ]--;
          if( rock_flash[ i ] < 0 ) rock_flash[ i ]++;
      }

    update_saucer();

    // player bullets
    for( b = 0; b < MAX_BULLETS; b++ )
      if( bullet_life[ b ] > 0 )
      {
          bullet_life[ b ]--;
          bullet_x[ b ] = wrap( bullet_x[ b ] + bullet_vx[ b ], screen_width );
          bullet_y[ b ] = wrap( bullet_y[ b ] + bullet_vy[ b ], screen_height );

          for( i = 0; i < MAX_ROCKS; i++ )
            if( rock_active[ i ] )
            {
                dx = bullet_x[ b ] - rock_x[ i ];
                dy = bullet_y[ b ] - rock_y[ i ];
                r = rock_radius( rock_size[ i ] );
                if( dx * dx + dy * dy < r * r )
                {
                    bullet_life[ b ] = 0;
                    hit_rock( i, bullet_vx[ b ], bullet_vy[ b ] );
                    break;
                }
            }

          if( bullet_life[ b ] > 0 && saucer_active )
          {
              dx = bullet_x[ b ] - saucer_x;
              dy = bullet_y[ b ] - saucer_y;
              r = saucer_size() * 0.8;
              if( dx * dx + dy * dy < r * r )
              {
                  bullet_life[ b ] = 0;
                  destroy_saucer( true );
              }
          }
      }

    // saucer bullets: hit the ship, and also break rocks
    for( b = 0; b < MAX_ENEMY_SHOTS; b++ )
      if( eshot_life[ b ] > 0 )
      {
          eshot_life[ b ]--;
          eshot_x[ b ] = wrap( eshot_x[ b ] + eshot_vx[ b ], screen_width );
          eshot_y[ b ] = wrap( eshot_y[ b ] + eshot_vy[ b ], screen_height );

          if( ship_vulnerable() )
          {
              dx = eshot_x[ b ] - ship_x;
              dy = eshot_y[ b ] - ship_y;
              if( dx * dx + dy * dy < 9 * 9 )
              {
                  eshot_life[ b ] = 0;
                  kill_ship();
                  continue;
              }
          }

          for( i = 0; i < MAX_ROCKS; i++ )
            if( rock_active[ i ] )
            {
                dx = eshot_x[ b ] - rock_x[ i ];
                dy = eshot_y[ b ] - rock_y[ i ];
                r = rock_radius( rock_size[ i ] );
                if( dx * dx + dy * dy < r * r )
                {
                    eshot_life[ b ] = 0;
                    destroy_rock( i, false );
                    break;
                }
            }
      }

    // the shield smashes rocks, swallows shots and rams saucers,
    // each costing energy
    if( ship_alive && shield_up )
    {
        r = SHIELD_RADIUS;
        for( i = 0; i < MAX_ROCKS; i++ )
          if( rock_active[ i ] )
          {
              dx = ship_x - rock_x[ i ];
              dy = ship_y - rock_y[ i ];
              if( dx * dx + dy * dy < ( r + rock_radius( rock_size[ i ] ) ) * ( r + rock_radius( rock_size[ i ] ) ) )
              {
                  destroy_rock( i, true );
                  shield_energy -= 20 + rock_size[ i ] * 15;
                  spawn_ring( ship_x, ship_y, 34, 12, 0xFFFFE060 );
              }
          }

        for( b = 0; b < MAX_ENEMY_SHOTS; b++ )
          if( eshot_life[ b ] > 0 )
          {
              dx = eshot_x[ b ] - ship_x;
              dy = eshot_y[ b ] - ship_y;
              if( dx * dx + dy * dy < ( r + 3 ) * ( r + 3 ) )
              {
                  eshot_life[ b ] = 0;
                  shield_energy -= 25;
                  spawn_ring( ship_x, ship_y, 30, 10, 0xFFFFE060 );
              }
          }

        if( saucer_active )
        {
            dx = saucer_x - ship_x;
            dy = saucer_y - ship_y;
            if( dx * dx + dy * dy < ( r + saucer_size() ) * ( r + saucer_size() ) )
            {
                destroy_saucer( true );
                shield_energy -= 60;
            }
        }

        if( shield_energy < 0 ) shield_energy = 0;
    }

    update_pickups();

    // ship vs rocks
    if( ship_vulnerable() )
      for( i = 0; i < MAX_ROCKS; i++ )
        if( rock_active[ i ] )
        {
            dx = ship_x - rock_x[ i ];
            dy = ship_y - rock_y[ i ];
            r = rock_radius( rock_size[ i ] ) + 7;
            if( dx * dx + dy * dy < r * r )
            {
                destroy_rock( i, true );
                kill_ship();
                break;
            }
        }

    // effects
    for( i = 0; i < MAX_PARTICLES; i++ )
      if( part_life[ i ] > 0 )
      {
          part_life[ i ]--;
          part_x[ i ] += part_vx[ i ];
          part_y[ i ] += part_vy[ i ];
          part_vx[ i ] *= 0.97;
          part_vy[ i ] *= 0.97;
          part_angle[ i ] += part_spin[ i ];
      }

    for( i = 0; i < MAX_RINGS; i++ )
      if( ring_life[ i ] > 0 ) ring_life[ i ]--;

    for( i = 0; i < MAX_POPUPS; i++ )
      if( popup_life[ i ] > 0 )
      {
          popup_life[ i ]--;
          popup_y[ i ] -= 0.5;
      }
}

// heartbeat that speeds up the longer a wave lasts
void update_beat()
{
    int interval;

    if( !ship_alive || next_wave_timer > 0 ) return;

    wave_frames++;
    beat_timer--;

    if( beat_timer <= 0 )
    {
        if( beat_high ) sfx( SND_BEAT_HIGH, 0.55 );
        else sfx( SND_BEAT_LOW, 0.55 );
        beat_high = !beat_high;

        interval = 58 - wave_frames / 75;
        if( interval < 16 ) interval = 16;
        beat_timer = interval;
    }
}

void update_playing()
{
    int bits;

    if( !demo_mode && gamepad_button_start() == 1 )
    {
        paused = !paused;

        // pause everything first, so the chime itself still plays
        if( paused ) pause_all_channels();
        else resume_all_channels();
        sfx( SND_PAUSE, 0.6 );
    }

    if( paused ) return;

    // this logic frame's input: replayed, or read live and recorded
    if( demo_mode )
    {
        bits = rec_get( demo_data, demo_pos );
        demo_pos++;
    }
    else
    {
        bits = read_gamepad_bits();
        rec_add( bits );
    }

    apply_input( bits );
    game_frame++;

    update_ship();
    update_world();
    update_beat();

    // wave cleared: short breather, then the next one
    if( count_rocks() == 0 && next_wave_timer == 0 )
    {
        next_wave_timer = 100;
        sfx( SND_WAVE_CLEAR, 0.6 );
    }

    if( next_wave_timer > 0 )
    {
        next_wave_timer--;
        if( next_wave_timer == 0 )
        {
            level++;
            start_wave();
        }
    }

    if( wave_banner > 0 ) wave_banner--;

    if( lives <= 0 && !ship_alive && ship_respawn_timer < 40 )
    {
        if( demo_mode ) enter_title();
        else finish_game();
    }
}

// -----------------------------------------------------------------------------
//   DRAWING
// -----------------------------------------------------------------------------

#define STARS 60
float[ STARS ] star_x;
float[ STARS ] star_y;
float[ STARS ] star_phase;

void draw_card_status();

// effect fades are relative to the world brightness
void world_intensity( float f )
{
    vec_intensity( f * world_dim );
}

void draw_stars()
{
    int i, g, bl;
    float b;

    vec_thickness( 1 );
    for( i = 0; i < STARS; i++ )
    {
        b = 0.55 + 0.45 * sin( anim * 0.03 + star_phase[ i ] );
        g = 50 + 50 * b;
        bl = 80 + 60 * b;
        vec_color( 0xFF000000 | ( bl << 16 ) | ( g << 8 ) | g, 0 );
        vec_line_polar( star_x[ i ], star_y[ i ], 1, 0 );
    }
}

void draw_saucer()
{
    int i;
    float size = saucer_size();
    float[ 6 ] hx;
    float[ 6 ] hy;

    vec_thickness( 2 );
    vec_glow_size( 16 );
    vec_color( SAUCER_CORE, SAUCER_GLOW );

    for( i = 0; i < 6; i++ )
    {
        hx[ i ] = saucer_x + saucer_hull_x[ i ] * size;
        hy[ i ] = saucer_y + saucer_hull_y[ i ] * size;
    }
    vec_polyline( hx, hy, 6, true );
    vec_line( saucer_x - size, saucer_y, saucer_x + size, saucer_y );

    for( i = 0; i < 4; i++ )
    {
        hx[ i ] = saucer_x + saucer_dome_x[ i ] * size;
        hy[ i ] = saucer_y + saucer_dome_y[ i ] * size;
    }
    vec_polyline( hx, hy, 4, false );

    // chasing running lights along the hull
    vec_thickness( 2.5 );
    for( i = 0; i < 3; i++ )
      if( ( anim / 6 + i ) % 3 == 0 )
      {
          vec_color( 0xFF40FFFF, 0xFF208080 );
          vec_line( saucer_x + ( i - 1 ) * size * 0.45 - 1, saucer_y + size * 0.16,
                    saucer_x + ( i - 1 ) * size * 0.45 + 1, saucer_y + size * 0.16 );
      }
}

void draw_world()
{
    int i;
    float c, s, len, fade, r;
    int pc;
    bool ship_visible;

    // rocks (newly split ones flash white)
    vec_thickness( 2 );
    vec_glow_size( 18 );
    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
      {
          r = rock_radius( rock_size[ i ] );

          if( rock_flash[ i ] > 0 ) vec_color( 0xFFFFFFFF, 0xFFC0A0B0 );
          else if( rock_flash[ i ] < 0 ) vec_color( 0xFF80D0FF, 0xFF205080 );   // armor cracking
          else if( rock_armored[ i ] ) vec_color( ARMOR_CORE, ARMOR_GLOW );
          else vec_color( ROCK_CORE, ROCK_GLOW );

          vec_shape_wrapped( &rock_shape_x[ rock_shape[ i ] * ROCK_POINTS ],
                             &rock_shape_y[ rock_shape[ i ] * ROCK_POINTS ],
                             ROCK_POINTS, rock_x[ i ], rock_y[ i ], rock_angle[ i ], r, r * 1.1 );

          // armor plating: an inner outline while it still holds,
          // a few broken fragments of it after the first hit
          if( rock_armored[ i ] && rock_flash[ i ] == 0 )
          {
              vec_thickness( 1.5 );
              if( rock_hp[ i ] > 1 )
                vec_shape( &rock_shape_x[ rock_shape[ i ] * ROCK_POINTS ],
                           &rock_shape_y[ rock_shape[ i ] * ROCK_POINTS ],
                           ROCK_POINTS, rock_x[ i ], rock_y[ i ], -rock_angle[ i ], r * 0.55 );
              else
                vec_arc( rock_x[ i ], rock_y[ i ], r * 0.45, rock_angle[ i ] * 2, rock_angle[ i ] * 2 + 2.2, 4 );
              vec_thickness( 2 );
          }
      }

    // power-ups: a spinning diamond around a letter, blinking as they expire
    for( i = 0; i < MAX_PICKUPS; i++ )
      if( pickup_active[ i ] && ( pickup_life[ i ] > 120 || ( anim / 5 ) % 2 == 0 ) )
      {
          pc = pickup_color( pickup_type[ i ] );
          vec_thickness( 2 );
          vec_glow_size( 18 );
          vec_color( pc, vec_scale_rgb( pc, 0.45 ) );
          vec_shape( pickup_shape_x, pickup_shape_y, 4, pickup_x[ i ], pickup_y[ i ], anim * 0.05, 13 );

          vec_thickness( 1.5 );
          vec_glow_size( 10 );
          if( pickup_type[ i ] == PU_SPREAD ) vec_char( '3', pickup_x[ i ] - 2.5, pickup_y[ i ] - 5, 5 );
          else if( pickup_type[ i ] == PU_RAPID ) vec_char( 'R', pickup_x[ i ] - 2.5, pickup_y[ i ] - 5, 5 );
          else vec_char( 'E', pickup_x[ i ] - 2.5, pickup_y[ i ] - 5, 5 );
      }

    if( saucer_active ) draw_saucer();

    // ship (blinks while invulnerable)
    ship_visible = ship_alive && ship_hyperspace == 0 && in_game();
    if( ship_invulnerable > 0 && ( anim / 4 ) % 2 == 0 ) ship_visible = false;

    if( ship_visible )
    {
        vec_thickness( 2 );
        vec_glow_size( 16 );
        vec_color( SHIP_CORE, SHIP_GLOW );
        vec_shape_wrapped( ship_model_x, ship_model_y, 4, ship_x, ship_y, ship_angle, 1.0, 14 );

        if( ship_thrusting && !paused )
        {
            c = cos( ship_angle );
            s = sin( ship_angle );
            len = 10 + ( fxrand() % 9 );
            vec_color( FLAME_CORE, FLAME_GLOW );
            vec_line( ship_x - c * 6 - s * 4, ship_y - s * 6 + c * 4, ship_x - c * len, ship_y - s * len );
            vec_line( ship_x - c * 6 + s * 4, ship_y - s * 6 - c * 4, ship_x - c * len, ship_y - s * len );
        }
    }

    // shield bubble: a pulsing double ring, flickering when nearly empty
    if( ship_alive && shield_up && ( shield_energy > 60 || ( anim / 3 ) % 2 == 0 ) )
    {
        vec_thickness( 2 );
        vec_glow_size( 20 );
        vec_color( 0xFFFFE060, 0xFF806020 );
        vec_circle( ship_x, ship_y, SHIELD_RADIUS + sin( anim * 0.3 ), 18 );
        world_intensity( 0.45 );
        vec_thickness( 1.5 );
        vec_arc( ship_x, ship_y, SHIELD_RADIUS - 4, anim * 0.12, anim * 0.12 + 2.5, 6 );
        vec_arc( ship_x, ship_y, SHIELD_RADIUS - 4, anim * 0.12 + 3.14, anim * 0.12 + 5.64, 6 );
        world_intensity( 1 );
    }

    // player bullets as tracers
    vec_thickness( 2.5 );
    vec_glow_size( 12 );
    vec_color( BULLET_CORE, BULLET_GLOW );
    for( i = 0; i < MAX_BULLETS; i++ )
      if( bullet_life[ i ] > 0 )
        vec_line( bullet_x[ i ], bullet_y[ i ],
                  bullet_x[ i ] - bullet_vx[ i ] * 0.6, bullet_y[ i ] - bullet_vy[ i ] * 0.6 );

    // saucer bullets: small pulsing diamonds
    vec_thickness( 2 );
    vec_color( ENEMY_SHOT, ENEMY_GLOW );
    for( i = 0; i < MAX_ENEMY_SHOTS; i++ )
      if( eshot_life[ i ] > 0 )
      {
          r = 2.5 + ( anim % 6 ) * 0.3;
          vec_line( eshot_x[ i ] - r, eshot_y[ i ], eshot_x[ i ], eshot_y[ i ] - r );
          vec_line( eshot_x[ i ], eshot_y[ i ] - r, eshot_x[ i ] + r, eshot_y[ i ] );
          vec_line( eshot_x[ i ] + r, eshot_y[ i ], eshot_x[ i ], eshot_y[ i ] + r );
          vec_line( eshot_x[ i ], eshot_y[ i ] + r, eshot_x[ i ] - r, eshot_y[ i ] );
      }

    // shockwave rings: grow and fade
    vec_thickness( 1.5 );
    vec_glow_size( 14 );
    // (fades are applied to each color directly: changing the global
    // intensity per item is much slower)
    for( i = 0; i < MAX_RINGS; i++ )
      if( ring_life[ i ] > 0 )
      {
          fade = ring_life[ i ] / ( ring_max_life[ i ] * 1.0 ) * 0.8;
          vec_color( vec_scale_alpha( ring_color[ i ], fade ), vec_scale_rgb( ring_color[ i ], 0.35 * fade ) );
          vec_circle( ring_x[ i ], ring_y[ i ], ring_radius[ i ] * ( 1.0 - fade * fade ), 14 );
      }
    world_intensity( 1 );

    // particles fade out
    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    for( i = 0; i < MAX_PARTICLES; i++ )
      if( part_life[ i ] > 0 )
      {
          fade = part_life[ i ] / ( part_max_life[ i ] * 1.0 );
          vec_color( vec_scale_alpha( part_color[ i ], fade ), vec_scale_rgb( part_color[ i ], 0.4 * fade ) );
          c = cos( part_angle[ i ] ) * part_length[ i ] * 0.5;
          s = sin( part_angle[ i ] ) * part_length[ i ] * 0.5;
          vec_line( part_x[ i ] - c, part_y[ i ] - s, part_x[ i ] + c, part_y[ i ] + s );
      }
    world_intensity( 1 );

    // floating score popups
    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    for( i = 0; i < MAX_POPUPS; i++ )
      if( popup_life[ i ] > 0 )
      {
          fade = popup_life[ i ] / 50.0;
          vec_color( vec_scale_alpha( 0xFFA0F0FF, fade ), vec_scale_rgb( 0xFF305060, fade ) );
          vec_number( popup_value[ i ], popup_x[ i ] - 8, popup_y[ i ], 4 );
      }
    world_intensity( 1 );
}

void draw_hud()
{
    int i;
    float w;

    vec_thickness( 2 );
    vec_glow_size( 12 );
    vec_color( TEXT_CORE, TEXT_GLOW );
    vec_number( score, 16, 12, 9 );

    // lives as small ships (capped so they never cover the screen)
    vec_color( SHIP_CORE, SHIP_GLOW );
    for( i = 0; i < lives && i < 8; i++ )
      vec_shape( ship_model_x, ship_model_y, 4, 24 + i * 22, 52, -1.5707963, 0.8 );

    // shield energy meter under the lives
    vec_thickness( 1 );
    vec_color( 0xFF806040, 0 );
    vec_line( 16, 68, 96, 68 );
    vec_line( 16, 74, 96, 74 );
    vec_thickness( 4 );
    vec_glow_size( 10 );
    if( shield_energy < SHIELD_MAX * 0.25 ) vec_color( 0xFF6060FF, 0xFF202080 );
    else vec_color( 0xFFFFE060, 0xFF806020 );
    if( shield_energy > 1 ) vec_line( 16, 71, 16 + 80 * shield_energy / SHIELD_MAX, 71 );

    // active power-ups: a letter and a shrinking bar each
    vec_thickness( 1.5 );
    w = 84;
    if( spread_timer > 0 )
    {
        vec_color( pickup_color( PU_SPREAD ), vec_scale_rgb( pickup_color( PU_SPREAD ), 0.4 ) );
        vec_char( '3', 16, w, 4 );
        vec_line( 26, w + 4, 26 + 50 * spread_timer / POWER_TIME, w + 4 );
        w += 14;
    }
    if( rapid_timer > 0 )
    {
        vec_color( pickup_color( PU_RAPID ), vec_scale_rgb( pickup_color( PU_RAPID ), 0.4 ) );
        vec_char( 'R', 16, w, 4 );
        vec_line( 26, w + 4, 26 + 50 * rapid_timer / POWER_TIME, w + 4 );
    }

    vec_thickness( 1.5 );
    vec_color( DIM_TEXT, 0 );
    vec_text( "HI", 284, 14, 5 );
    vec_number( high_score, 306, 14, 5 );
    vec_text( "WAVE", 548, 14, 6 );
    vec_number( level, 548 + vec_text_width( "WAVE ", 6 ), 14, 6 );

    // wave announcement fades in and out
    if( wave_banner > 0 && in_game() )
    {
        vec_intensity( fmin( 1.0, fmin( wave_banner / 30.0, ( 150 - wave_banner ) / 20.0 ) ) );
        vec_thickness( 2.5 );
        vec_glow_size( 18 );
        vec_color( 0xFFFFE0A0, 0xFF805020 );
        if( level < 10 ) w = 320 - vec_text_width( "WAVE 0", 12 ) / 2;
        else w = 320 - vec_text_width( "WAVE 00", 12 ) / 2;
        vec_text( "WAVE", w, 120, 12 );
        vec_number( level, w + vec_text_width( "WAVE ", 12 ), 120, 12 );

        if( wave_rule_text[ 0 ] )
        {
            vec_thickness( 1.5 );
            vec_glow_size( 12 );
            vec_color( 0xFFFFB090, 0xFF603020 );
            vec_text_centered( wave_rule_text, 320, 158, 5 );
        }
        vec_intensity( 1 );
    }

    if( paused )
    {
        vec_thickness( 3 );
        vec_glow_size( 20 );
        vec_color( TEXT_CORE, TEXT_GLOW );
        vec_text_centered( "PAUSED", 320, 150, 16 );
    }
}

void draw_title()
{
    int i, letter;
    float x, y, b, w;
    int[ 2 ] one_char;
    int* title = "ROCKFIELD";

    vec_intensity( fmin( 1.0, state_timer / 40.0 ) );

    // logo: letters bob in a wave, colors cycle along it
    vec_thickness( 3.5 );
    vec_glow_size( 26 );
    w = vec_text_width( title, 26 );
    x = 320 - w / 2;
    one_char[ 1 ] = 0;

    for( letter = 0; title[ letter ]; letter++ )
    {
        y = 70 + 6 * sin( frame * 0.05 + letter * 0.6 );
        b = 0.5 + 0.5 * sin( frame * 0.04 + letter * 0.7 );
        vec_color( make_color_rgb( 160 + 95 * b, 200 + 55 * ( 1 - b ), 255 ),
                   make_color_rgb( 60 + 50 * b, 50, 110 ) );
        one_char[ 0 ] = title[ letter ];
        vec_text( one_char, x + letter * 1.6 * 26, y, 26 );
    }

    // dashed underline and a spinning orbit, just to show the library off
    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    vec_color( 0xFFFFB090, 0xFF603020 );
    vec_dashed_line( 320 - w / 2, 140, 320 + w / 2, 140, 10, 6 );
    vec_arc( 320, 140, 9, frame * 0.08, frame * 0.08 + 4.0, 10 );

    // blinking prompt
    if( ( frame / 30 ) % 2 == 0 )
    {
        vec_thickness( 2 );
        vec_glow_size( 14 );
        vec_color( TEXT_CORE, TEXT_GLOW );
        vec_text_centered( "PRESS START", 320, 190, 10 );
    }

    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    vec_color( DIM_TEXT, 0 );
    vec_text_centered( "B FIRE   A THRUST   X HYPERSPACE   Y SHIELD", 320, 262, 5 );
    vec_text_centered( "START PAUSE", 320, 284, 5 );

    vec_color( 0xFFFFE0A0, 0xFF403010 );
    vec_text( "HIGH SCORE", 230, 320, 6 );
    vec_number( high_score, 230 + vec_text_width( "HIGH SCORE ", 6 ), 320, 6 );

    draw_card_status();

    vec_intensity( 1 );
}

void draw_game_over()
{
    float t = fmin( 1.0, state_timer / 30.0 );

    vec_intensity( t );
    vec_thickness( 3 );
    vec_glow_size( 22 );
    vec_color( 0xFF8080FF, 0xFF202070 );
    vec_text_centered( "GAME OVER", 320, 120 - 20 * ( 1 - t ), 18 );

    vec_thickness( 2 );
    vec_glow_size( 14 );
    if( new_high_score && ( frame / 12 ) % 2 == 0 )
    {
        vec_color( 0xFF60FFFF, 0xFF205050 );
        vec_text_centered( "NEW HIGH SCORE", 320, 185, 8 );
    }

    if( save_message != SAVE_MSG_NONE )
    {
        vec_thickness( 1.5 );
        vec_color( 0xFFC0A0A0, 0 );
        if( save_message == SAVE_MSG_SAVED )        vec_text_centered( "SAVED TO MEMORY CARD", 320, 262, 5 );
        else if( save_message == SAVE_MSG_NO_CARD ) vec_text_centered( "NO MEMORY CARD - NOT SAVED", 320, 262, 5 );
        else                                        vec_text_centered( "CARD HAS ANOTHER GAME - NOT SAVED", 320, 262, 5 );
    }

    if( state_timer > 90 && ( frame / 30 ) % 2 == 0 )
    {
        vec_color( TEXT_CORE, TEXT_GLOW );
        vec_text_centered( "PRESS START", 320, 225, 8 );
    }

    vec_intensity( 1 );
}

// width of a number drawn with vec_number, for right-aligning
float number_width( int value, float size )
{
    int digits = 1;
    while( value >= 10 )
    {
        value /= 10;
        digits++;
    }
    return ( digits * 1.6 - 0.6 ) * size;
}

void draw_card_status()
{
    vec_thickness( 1.2 );
    vec_glow_size( 8 );
    vec_color( 0xFF907070, 0 );
    if( card_state == CARD_OURS )       vec_text_centered( "HIGH SCORES SAVED ON MEMORY CARD", 320, 342, 4.2 );
    else if( card_state == CARD_EMPTY ) vec_text_centered( "MEMORY CARD READY", 320, 342, 4.2 );
    else if( card_state == CARD_OTHER ) vec_text_centered( "MEMORY CARD HAS ANOTHER GAME - NOT SAVING", 320, 342, 4.2 );
    else                                vec_text_centered( "NO MEMORY CARD - HIGH SCORES WILL NOT BE SAVED", 320, 342, 4.2 );
}

void draw_scores_page()
{
    int i;
    float y, t;
    int[ 4 ] name;
    int[ 2 ] rank_text;

    t = fmin( 1.0, state_timer / 30.0 );
    vec_intensity( t );

    vec_thickness( 3 );
    vec_glow_size( 22 );
    vec_color( 0xFFFFE0A0, 0xFF805020 );
    vec_text_centered( "HIGH SCORES", 320, 34, 15 );

    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    vec_color( 0xFFFFB090, 0xFF603020 );
    vec_dashed_line( 170, 82, 470, 82, 8, 6 );

    rank_text[ 1 ] = 0;

    for( i = 0; i < HS_COUNT; i++ )
    {
        y = 104 + i * 40;

        // rows slide in one after another
        vec_intensity( t * fmin( 1.0, fmax( 0.0, ( state_timer - i * 8 ) / 20.0 ) ) );

        if( i == highlight_rank )
        {
            vec_thickness( 2.5 );
            vec_glow_size( 18 );
            if( ( frame / 10 ) % 2 ) vec_color( 0xFF60FFFF, 0xFF208080 );
            else vec_color( 0xFFFFFFFF, 0xFF406060 );
        }
        else
        {
            vec_thickness( 2 );
            vec_glow_size( 14 );
            if( i == 0 ) vec_color( 0xFFFFF0B0, 0xFF805020 );
            else vec_color( 0xFFF0D8E8, 0xFF603848 );
        }

        rank_text[ 0 ] = '1' + i;
        vec_text( rank_text, 180, y, 9 );
        unpack_name( hs_name[ i ], name );
        vec_text( name, 230, y, 9 );
        vec_number( hs_score[ i ], 460 - number_width( hs_score[ i ], 9 ), y, 9 );
    }

    vec_intensity( t );

    if( save_message != SAVE_MSG_NONE && highlight_rank >= 0 )
    {
        vec_thickness( 1.5 );
        vec_glow_size( 10 );
        vec_color( 0xFFC0A0A0, 0 );
        if( save_message == SAVE_MSG_SAVED )        vec_text_centered( "SAVED TO MEMORY CARD", 320, 312, 5 );
        else if( save_message == SAVE_MSG_NO_CARD ) vec_text_centered( "NO MEMORY CARD - NOT SAVED", 320, 312, 5 );
        else                                        vec_text_centered( "CARD HAS ANOTHER GAME - NOT SAVED", 320, 312, 5 );
    }
    else if( ( frame / 30 ) % 2 == 0 )
    {
        vec_thickness( 2 );
        vec_glow_size( 14 );
        vec_color( TEXT_CORE, TEXT_GLOW );
        vec_text_centered( "PRESS START", 320, 308, 8 );
    }

    draw_card_status();
    vec_intensity( 1 );
}

void draw_entry()
{
    int i;
    float x, w, t;
    int[ 2 ] letter;

    t = fmin( 1.0, state_timer / 25.0 );
    vec_intensity( t );

    vec_thickness( 3 );
    vec_glow_size( 22 );
    if( entry_rank == 0 )
    {
        vec_color( 0xFF60FFFF, 0xFF205050 );
        vec_text_centered( "NEW HIGH SCORE", 320, 30, 14 );
    }
    else
    {
        vec_color( 0xFFFFE0A0, 0xFF805020 );
        vec_text_centered( "TOP 5 SCORE", 320, 30, 14 );
    }

    vec_thickness( 2.5 );
    vec_glow_size( 16 );
    vec_color( TEXT_CORE, TEXT_GLOW );
    w = number_width( score, 12 );
    vec_number( score, 320 - w / 2, 84, 12 );

    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    vec_color( DIM_TEXT, 0 );
    vec_text_centered( "ENTER YOUR INITIALS", 320, 136, 6 );

    // three big letters, the one being edited glows and bobs
    letter[ 1 ] = 0;
    for( i = 0; i < 3; i++ )
    {
        x = 320 - 90 + i * 70;
        letter[ 0 ] = entry_charset[ entry_index[ i ] ];

        if( i == entry_cursor )
        {
            vec_thickness( 3.5 );
            vec_glow_size( 26 );
            vec_color( 0xFF60FFFF, 0xFF208080 );
            vec_text( letter, x, 170 + 3 * sin( frame * 0.2 ), 20 );
            if( ( frame / 15 ) % 2 == 0 ) vec_line( x - 4, 226, x + 24, 226 );
        }
        else
        {
            vec_thickness( 3 );
            vec_glow_size( 18 );
            vec_color( 0xFFF0D8E8, 0xFF603848 );
            vec_text( letter, x, 170, 20 );
            vec_line( x - 4, 226, x + 24, 226 );
        }

        // a space would be invisible; show where it is
        if( letter[ 0 ] == ' ' )
        {
            vec_thickness( 1.5 );
            vec_dashed_line( x, 210, x + 20, 210, 3, 3 );
        }
    }

    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    vec_color( DIM_TEXT, 0 );
    vec_text_centered( "UP DOWN  CHANGE     LEFT RIGHT  MOVE", 320, 268, 5 );
    vec_text_centered( "B  NEXT     START  DONE", 320, 290, 5 );

    vec_intensity( 1 );
}

void draw_demo_overlay()
{
    vec_thickness( 2.5 );
    vec_glow_size( 16 );
    if( ( frame / 40 ) % 2 == 0 )
    {
        vec_color( 0xFFFFE0A0, 0xFF805020 );
        vec_text_centered( "DEMO", 320, 80, 10 );
    }

    vec_thickness( 2 );
    vec_glow_size( 14 );
    vec_color( TEXT_CORE, TEXT_GLOW );
    vec_text_centered( "PRESS START", 320, 318, 8 );
}

void draw_flash()
{
    if( flash > 0.01 )
      vec_fill_rect( 0, 0, screen_width, screen_height,
                     vec_scale_alpha( flash_color, flash * 0.6 ), blending_add );
}

// -----------------------------------------------------------------------------
//   MAIN
// -----------------------------------------------------------------------------

void main()
{
    int i;

    srand( get_time() + 1 );
    vec_init( 0, 0 );
    init_sounds();
    set_global_volume( 0.85 );
    select_gamepad( 0 );
    init_table();
    load_builtin_demo();

    // fixed seed: rock outlines and stars look the same on every boot,
    // so demos saved on a card replay with identical shapes
    gseed( 20261031 );
    build_rock_shapes();
    for( i = 0; i < STARS; i++ )
    {
        star_x[ i ] = frand() * screen_width;
        star_y[ i ] = frand() * screen_height;
        star_phase[ i ] = frand() * 6.2831853;
    }
    gseed( rand() + 1 );

    refresh_card();
    high_score = hs_score[ 0 ];
    enter_title();

    while( true )
    {
        // --- update ---
        if( state == STATE_TITLE )
        {
            // notices cards being inserted or swapped on the title screen
            refresh_card();
            update_world();

            if( gamepad_button_start() == 1 && state_timer > 20 ) new_game();

            // logo -> high scores -> demo -> logo ...
            else if( title_page == PAGE_LOGO && state_timer > 480 )
              enter_title_page( PAGE_SCORES );
            else if( title_page == PAGE_SCORES && state_timer > 360 )
            {
                highlight_rank = -1;
                if( demo_frames > 0 ) begin_game( demo_seed, true );
                else enter_title_page( PAGE_LOGO );
            }
        }
        else if( state == STATE_PLAYING )
        {
            update_playing();
        }
        else if( state == STATE_DEMO )
        {
            if( gamepad_button_start() == 1 ) new_game();
            else
            {
                update_playing();
                if( state == STATE_DEMO && ( demo_pos >= demo_frames || demo_pos >= DEMO_MAX_PLAY ) )
                  enter_title();
            }
        }
        else if( state == STATE_ENTRY )
        {
            update_world();
            update_entry();
        }
        else
        {
            update_world();
            if( state_timer > 90 && gamepad_button_start() == 1 ) new_game();
            else if( state_timer > 900 ) enter_title();
        }

        state_timer++;
        if( !paused )
        {
            shake *= 0.86;
            flash *= 0.85;
        }

        // --- draw ---
        anim = frame;
        if( in_game() ) anim = game_frame;

        clear_screen( make_color_rgb( 4, 3, 10 ) );

        vec_offset( fx_range( -shake, shake ), fx_range( -shake, shake ) );
        world_dim = 1.0;
        if( paused ) world_dim = 0.35;
        if( state == STATE_TITLE || state == STATE_ENTRY ) world_dim = 0.4;
        vec_intensity( world_dim );
        draw_stars();
        draw_world();
        vec_intensity( 1 );
        vec_offset( 0, 0 );

        if( state == STATE_TITLE )
        {
            if( title_page == PAGE_LOGO ) draw_title();
            else draw_scores_page();
        }
        else if( state == STATE_ENTRY ) draw_entry();
        else
        {
            draw_hud();
            if( state == STATE_GAMEOVER ) draw_game_over();
            if( state == STATE_DEMO ) draw_demo_overlay();
        }

        // lines first, then the additive flash over everything
        vec_flush();
        draw_flash();

        frame++;
        end_frame();
    }
}
