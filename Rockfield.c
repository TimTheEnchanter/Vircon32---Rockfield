// =============================================================================
//   ROCKFIELD - an Asteroids-style game built with vector.h
// =============================================================================
//   Controls:  Left / Right  rotate          Up or A   thrust
//              B             fire            X         hyperspace
//              Start         start / pause
// =============================================================================

#define VEC_MAX_LINES 900

#include "video.h"
#include "math.h"
#include "time.h"
#include "input.h"
#include "audio.h"
#include "misc.h"
#include "vector.h"

// -----------------------------------------------------------------------------
//   COLORS  (Vircon32 colors are 0xAABBGGRR)
// -----------------------------------------------------------------------------

#define SHIP_CORE      0xFFFFF0B0
#define SHIP_GLOW      0xFFB07020
#define ROCK_CORE      0xFFF0D8E8
#define ROCK_GLOW      0xFF904870
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

#define CH_THRUST     0
#define CH_SAUCER     1
#define CH_FIRST_SFX  2

int next_sfx_channel = CH_FIRST_SFX;
bool thrust_sound_on = false;
bool saucer_sound_on = false;

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

void init_sounds()
{
    select_sound( SND_THRUST );       set_sound_loop( true );
    select_sound( SND_SAUCER_BIG );   set_sound_loop( true );
    select_sound( SND_SAUCER_SMALL ); set_sound_loop( true );
}

// -----------------------------------------------------------------------------
//   HELPERS
// -----------------------------------------------------------------------------

float frand()
{
    return ( rand() % 10000 ) / 10000.0;
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

int state = STATE_TITLE;
int state_timer = 0;
int frame = 0;
bool paused = false;

int score;
int high_score = 5000;
bool new_high_score;
int lives;
int level;
int next_extra_life;
int wave_frames;
int wave_banner;
int next_wave_timer;
int beat_timer;
bool beat_high;

// screen effects
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
          rock_shape[ i ] = rand() % ROCK_SHAPES;
          rock_flash[ i ] = 0;
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
void spawn_wave_rocks( int amount )
{
    int i;
    float x, y;

    for( i = 0; i < amount; i++ )
    {
        if( rand() % 2 )
        {
            x = frand() * screen_width;
            y = frand_range( -40, 40 );
        }
        else
        {
            x = frand_range( -40, 40 );
            y = frand() * screen_height;
        }

        spawn_rock( x, y, 3, ( level - 1 ) * 0.08 );
    }
}

// -----------------------------------------------------------------------------
//   PLAYER BULLETS
// -----------------------------------------------------------------------------

#define MAX_BULLETS 8

int[ MAX_BULLETS ] bullet_life;
float[ MAX_BULLETS ] bullet_x;
float[ MAX_BULLETS ] bullet_y;
float[ MAX_BULLETS ] bullet_vx;
float[ MAX_BULLETS ] bullet_vy;

void fire_bullet()
{
    int i;
    float c = cos( ship_angle ), s = sin( ship_angle );

    for( i = 0; i < MAX_BULLETS; i++ )
      if( bullet_life[ i ] <= 0 )
      {
          bullet_life[ i ] = 48;
          bullet_x[ i ] = ship_x + c * 14;
          bullet_y[ i ] = ship_y + s * 14;
          bullet_vx[ i ] = ship_vx + c * 7.0;
          bullet_vy[ i ] = ship_vy + s * 7.0;
          sfx_pitched( SND_FIRE, 0.5, frand_range( 0.95, 1.05 ) );
          return;
      }
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
    saucer_spawn_timer = 600 + rand() % 600 - level * 30;
    if( saucer_spawn_timer < 360 ) saucer_spawn_timer = 360;
}

void spawn_saucer()
{
    float speed;

    // small saucers get more likely as the game goes on
    saucer_small = ( rand() % 100 ) < ( level * 12 + score / 500 );
    if( score < 3000 ) saucer_small = false;

    speed = 1.5;
    if( saucer_small ) speed = 2.1;

    saucer_active = true;
    saucer_y = frand_range( 50, screen_height - 50 );

    if( rand() % 2 )
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
        spawn_particle_v( x, y, cos( a ) * v, sin( a ) * v, length, 30 + rand() % 30, color );
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
}

// -----------------------------------------------------------------------------
//   SCORING
// -----------------------------------------------------------------------------

void add_score( int points, float x, float y )
{
    score += points;
    spawn_popup( x, y, points );

    if( score > high_score )
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
    int k, child;
    float px = rock_x[ i ], py = rock_y[ i ];
    rock_active[ i ] = false;

    if( by_player )
    {
        if( size == 3 ) add_score( 20, rock_x[ i ], rock_y[ i ] );
        else if( size == 2 ) add_score( 50, rock_x[ i ], rock_y[ i ] );
        else add_score( 100, rock_x[ i ], rock_y[ i ] );
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

    if( size > 1 )
      for( k = 0; k < 2; k++ )
      {
          // the two halves flash briefly as they break off
          child = spawn_rock( px, py, size - 1, ( level - 1 ) * 0.08 );
          if( child >= 0 ) rock_flash[ child ] = 8;
      }
}

void destroy_saucer( bool by_player )
{
    if( by_player )
    {
        if( saucer_small ) add_score( 1000, saucer_x, saucer_y );
        else add_score( 200, saucer_x, saucer_y );
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
    set_thrust_sound( false );

    // debris: the ship's own outline flying apart, plus sparks
    for( i = 0; i < 6; i++ )
    {
        a = frand() * 6.2831853;
        spawn_particle_v( ship_x, ship_y, ship_vx * 0.5 + cos( a ) * 1.2, ship_vy * 0.5 + sin( a ) * 1.2,
                          12, 70 + rand() % 30, SHIP_CORE );
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
}

void start_wave()
{
    spawn_wave_rocks( 3 + level );
    wave_frames = 0;
    wave_banner = 150;
    beat_timer = 60;
}

void new_game()
{
    clear_rocks();
    clear_effects();
    if( saucer_active ) remove_saucer();

    score = 0;
    new_high_score = false;
    lives = 3;
    level = 1;
    next_extra_life = 10000;
    next_wave_timer = 0;
    fire_cooldown = 0;
    paused = false;
    reset_ship();
    reset_saucer_timer();
    start_wave();

    sfx( SND_START, 0.7 );
    enter_state( STATE_PLAYING );
}

// title screen: a few rocks drifting in the background
void enter_title()
{
    int i;

    clear_rocks();
    clear_effects();
    if( saucer_active ) remove_saucer();
    set_thrust_sound( false );
    level = 1;

    for( i = 0; i < 7; i++ )
      spawn_rock( frand() * screen_width, frand() * screen_height, 1 + rand() % 3, 0 );

    enter_state( STATE_TITLE );
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

    if( gamepad_button_x() == 1 )
    {
        ship_hyperspace = 40;
        ship_thrusting = false;
        set_thrust_sound( false );
        spawn_ring( ship_x, ship_y, 30, 14, 0xFFFFC080 );
        explode( ship_x, ship_y, 8, 1.5, 3, 0xFFFFC080 );
        sfx( SND_HYPERSPACE, 0.6 );
        return;
    }

    if( gamepad_left() > 0 )  ship_angle -= 0.075;
    if( gamepad_right() > 0 ) ship_angle += 0.075;

    ship_thrusting = ( gamepad_up() > 0 || gamepad_button_a() > 0 );
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
                          2.5, 12 + rand() % 8, FLAME_CORE );
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

    // fire on press, auto-repeat while held
    if( fire_cooldown > 0 ) fire_cooldown--;
    if( gamepad_button_b() == 1 || ( gamepad_button_b() > 0 && fire_cooldown == 0 ) )
    {
        fire_bullet();
        fire_cooldown = 10;
    }

    if( ship_invulnerable > 0 ) ship_invulnerable--;
}

bool ship_vulnerable()
{
    return ship_alive && ship_invulnerable == 0 && ship_hyperspace == 0;
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
        if( state == STATE_PLAYING && ship_alive && next_wave_timer == 0 )
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
        saucer_turn_timer = 50 + rand() % 70;
        saucer_vy = ( ( rand() % 3 ) - 1 ) * fabs( saucer_vx ) * 0.8;
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
                    destroy_rock( i, true );
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
    if( gamepad_button_start() == 1 )
    {
        paused = !paused;

        // pause everything first, so the chime itself still plays
        if( paused ) pause_all_channels();
        else resume_all_channels();
        sfx( SND_PAUSE, 0.6 );
    }

    if( paused ) return;

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
        if( saucer_active ) remove_saucer();
        enter_state( STATE_GAMEOVER );
    }
}

// -----------------------------------------------------------------------------
//   DRAWING
// -----------------------------------------------------------------------------

#define STARS 60
float[ STARS ] star_x;
float[ STARS ] star_y;
float[ STARS ] star_phase;

// effect fades are relative to the world brightness
void world_intensity( float f )
{
    vec_intensity( f * world_dim );
}

void draw_stars()
{
    int i;
    float b;

    vec_thickness( 1 );
    for( i = 0; i < STARS; i++ )
    {
        b = 0.55 + 0.45 * sin( frame * 0.03 + star_phase[ i ] );
        vec_color( make_color_rgb( 50 + 50 * b, 50 + 50 * b, 80 + 60 * b ), 0 );
        vec_line( star_x[ i ], star_y[ i ], star_x[ i ] + 1, star_y[ i ] );
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
      if( ( frame / 6 + i ) % 3 == 0 )
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
    bool ship_visible;

    // rocks (newly split ones flash white)
    vec_thickness( 2 );
    vec_glow_size( 18 );
    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
      {
          if( rock_flash[ i ] > 0 ) vec_color( 0xFFFFFFFF, 0xFFC0A0B0 );
          else vec_color( ROCK_CORE, ROCK_GLOW );

          r = rock_radius( rock_size[ i ] );
          vec_shape_wrapped( &rock_shape_x[ rock_shape[ i ] * ROCK_POINTS ],
                             &rock_shape_y[ rock_shape[ i ] * ROCK_POINTS ],
                             ROCK_POINTS, rock_x[ i ], rock_y[ i ], rock_angle[ i ], r, r * 1.1 );
      }

    if( saucer_active ) draw_saucer();

    // ship (blinks while invulnerable)
    ship_visible = ship_alive && ship_hyperspace == 0 && state == STATE_PLAYING;
    if( ship_invulnerable > 0 && ( frame / 4 ) % 2 == 0 ) ship_visible = false;

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
            len = 10 + ( rand() % 9 );
            vec_color( FLAME_CORE, FLAME_GLOW );
            vec_line( ship_x - c * 6 - s * 4, ship_y - s * 6 + c * 4, ship_x - c * len, ship_y - s * len );
            vec_line( ship_x - c * 6 + s * 4, ship_y - s * 6 - c * 4, ship_x - c * len, ship_y - s * len );
        }
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
          r = 2.5 + ( frame % 6 ) * 0.3;
          vec_line( eshot_x[ i ] - r, eshot_y[ i ], eshot_x[ i ], eshot_y[ i ] - r );
          vec_line( eshot_x[ i ], eshot_y[ i ] - r, eshot_x[ i ] + r, eshot_y[ i ] );
          vec_line( eshot_x[ i ] + r, eshot_y[ i ], eshot_x[ i ], eshot_y[ i ] + r );
          vec_line( eshot_x[ i ], eshot_y[ i ] + r, eshot_x[ i ] - r, eshot_y[ i ] );
      }

    // shockwave rings: grow and fade
    vec_thickness( 1.5 );
    vec_glow_size( 14 );
    for( i = 0; i < MAX_RINGS; i++ )
      if( ring_life[ i ] > 0 )
      {
          fade = ring_life[ i ] / ( ring_max_life[ i ] * 1.0 );
          world_intensity( fade * 0.8 );
          vec_color( ring_color[ i ], vec_scale_rgb( ring_color[ i ], 0.35 ) );
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
          world_intensity( fade );
          vec_color( part_color[ i ], vec_scale_rgb( part_color[ i ], 0.4 ) );
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
          world_intensity( popup_life[ i ] / 50.0 );
          vec_color( 0xFFA0F0FF, 0xFF305060 );
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

    vec_thickness( 1.5 );
    vec_color( DIM_TEXT, 0 );
    vec_text( "HI", 284, 14, 5 );
    vec_number( high_score, 306, 14, 5 );
    vec_text( "WAVE", 548, 14, 6 );
    vec_number( level, 548 + vec_text_width( "WAVE ", 6 ), 14, 6 );

    // wave announcement fades in and out
    if( wave_banner > 0 && state == STATE_PLAYING )
    {
        vec_intensity( fmin( 1.0, fmin( wave_banner / 30.0, ( 150 - wave_banner ) / 20.0 ) ) );
        vec_thickness( 2.5 );
        vec_glow_size( 18 );
        vec_color( 0xFFFFE0A0, 0xFF805020 );
        if( level < 10 ) w = 320 - vec_text_width( "WAVE 0", 12 ) / 2;
        else w = 320 - vec_text_width( "WAVE 00", 12 ) / 2;
        vec_text( "WAVE", w, 120, 12 );
        vec_number( level, w + vec_text_width( "WAVE ", 12 ), 120, 12 );
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
    vec_text_centered( "B FIRE   A THRUST   X HYPERSPACE", 320, 262, 5 );
    vec_text_centered( "START PAUSE", 320, 284, 5 );

    vec_color( 0xFFFFE0A0, 0xFF403010 );
    vec_text( "HIGH SCORE", 230, 320, 6 );
    vec_number( high_score, 230 + vec_text_width( "HIGH SCORE ", 6 ), 320, 6 );

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

    if( state_timer > 90 && ( frame / 30 ) % 2 == 0 )
    {
        vec_color( TEXT_CORE, TEXT_GLOW );
        vec_text_centered( "PRESS START", 320, 225, 8 );
    }

    vec_intensity( 1 );
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

    build_rock_shapes();
    for( i = 0; i < STARS; i++ )
    {
        star_x[ i ] = frand() * screen_width;
        star_y[ i ] = frand() * screen_height;
        star_phase[ i ] = frand() * 6.2831853;
    }

    enter_title();

    while( true )
    {
        // --- update ---
        if( state == STATE_TITLE )
        {
            update_world();
            if( gamepad_button_start() == 1 && state_timer > 20 ) new_game();
        }
        else if( state == STATE_PLAYING )
        {
            update_playing();
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
        clear_screen( make_color_rgb( 4, 3, 10 ) );

        vec_offset( frand_range( -shake, shake ), frand_range( -shake, shake ) );
        world_dim = 1.0;
        if( paused ) world_dim = 0.35;
        if( state == STATE_TITLE ) world_dim = 0.45;
        vec_intensity( world_dim );
        draw_stars();
        draw_world();
        vec_intensity( 1 );
        vec_offset( 0, 0 );

        if( state == STATE_TITLE ) draw_title();
        else draw_hud();
        if( state == STATE_GAMEOVER ) draw_game_over();

        // lines first, then the additive flash over everything
        vec_flush();
        draw_flash();

        frame++;
        end_frame();
    }
}
