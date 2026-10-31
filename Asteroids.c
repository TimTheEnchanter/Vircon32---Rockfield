// =============================================================================
//   ROCKFIELD - an Asteroids-style example for vector.h
// =============================================================================
//   Controls:  Left / Right  rotate        Up or A   thrust
//              B             fire          Start     restart after game over
// =============================================================================

#include "video.h"
#include "math.h"
#include "time.h"
#include "input.h"
#include "misc.h"
#include "vector.h"

// -----------------------------------------------------------------------------
//   COLORS
// -----------------------------------------------------------------------------

#define SHIP_CORE      0xFFFFF0B0
#define SHIP_GLOW      0xFFB07020
#define ROCK_CORE      0xFFF0D8E8
#define ROCK_GLOW      0xFF904870
#define BULLET_CORE    0xFFA0FFFF
#define BULLET_GLOW    0xFF2090A0
#define SPARK_CORE     0xFF60C0FF
#define SPARK_GLOW     0xFF103050
#define TEXT_CORE      0xFFFFFFFF
#define TEXT_GLOW      0xFF705050

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

float wrap( float v, float limit, float margin )
{
    if( v < -margin ) return v + limit + 2 * margin;
    if( v > limit + margin ) return v - limit - 2 * margin;
    return v;
}

// -----------------------------------------------------------------------------
//   SHIP
// -----------------------------------------------------------------------------

float[ 4 ] ship_model_x = { 13, -9, -5, -9 };
float[ 4 ] ship_model_y = {  0, -8,  0,  8 };

float ship_x, ship_y, ship_vx, ship_vy, ship_angle;
bool ship_alive;
int ship_respawn_timer;
int ship_invulnerable;
int fire_cooldown;
int lives;
int score;
int level;
bool game_over;

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

void spawn_rock( float x, float y, int size )
{
    int i;
    float a, speed;

    for( i = 0; i < MAX_ROCKS; i++ )
      if( !rock_active[ i ] )
      {
          a = frand() * 6.2831853;
          speed = frand_range( 0.4, 1.0 ) * ( 4 - size ) * 0.6;
          rock_active[ i ] = true;
          rock_x[ i ] = x;
          rock_y[ i ] = y;
          rock_vx[ i ] = cos( a ) * speed;
          rock_vy[ i ] = sin( a ) * speed;
          rock_angle[ i ] = frand() * 6.2831853;
          rock_spin[ i ] = frand_range( -0.03, 0.03 );
          rock_size[ i ] = size;
          rock_shape[ i ] = rand() % ROCK_SHAPES;
          return;
      }
}

int count_rocks()
{
    int i, n = 0;
    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] ) n++;
    return n;
}

void start_level()
{
    int i;
    float x, y;

    for( i = 0; i < 3 + level; i++ )
    {
        // spawn along the screen edges, away from the ship
        if( rand() % 2 )
        {
            x = frand() * screen_width;
            y = -30;
            if( rand() % 2 ) y = screen_height + 30;
        }
        else
        {
            x = -30;
            if( rand() % 2 ) x = screen_width + 30;
            y = frand() * screen_height;
        }

        spawn_rock( x, y, 3 );
    }
}

// -----------------------------------------------------------------------------
//   BULLETS
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
          return;
      }
}

// -----------------------------------------------------------------------------
//   PARTICLES (short spinning line fragments)
// -----------------------------------------------------------------------------

#define MAX_PARTICLES 80

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

void spawn_particle( float x, float y, float speed, float length, int life, int color )
{
    int i;
    float a;

    for( i = 0; i < MAX_PARTICLES; i++ )
      if( part_life[ i ] <= 0 )
      {
          a = frand() * 6.2831853;
          speed *= frand_range( 0.3, 1.0 );
          part_life[ i ] = life;
          part_max_life[ i ] = life;
          part_x[ i ] = x;
          part_y[ i ] = y;
          part_vx[ i ] = cos( a ) * speed;
          part_vy[ i ] = sin( a ) * speed;
          part_angle[ i ] = frand() * 6.2831853;
          part_spin[ i ] = frand_range( -0.2, 0.2 );
          part_length[ i ] = length;
          part_color[ i ] = color;
          return;
      }
}

void explode( float x, float y, int amount, float speed, float length, int color )
{
    int i;
    for( i = 0; i < amount; i++ )
      spawn_particle( x, y, speed, length, 30 + rand() % 30, color );
}

// -----------------------------------------------------------------------------
//   GAME FLOW
// -----------------------------------------------------------------------------

void reset_ship()
{
    ship_x = screen_width / 2;
    ship_y = screen_height / 2;
    ship_vx = 0;
    ship_vy = 0;
    ship_angle = -1.5707963;
    ship_alive = true;
    ship_invulnerable = 120;
}

void new_game()
{
    int i;

    for( i = 0; i < MAX_ROCKS; i++ )     rock_active[ i ] = false;
    for( i = 0; i < MAX_BULLETS; i++ )   bullet_life[ i ] = 0;
    for( i = 0; i < MAX_PARTICLES; i++ ) part_life[ i ] = 0;

    score = 0;
    lives = 3;
    level = 1;
    game_over = false;
    fire_cooldown = 0;
    reset_ship();
    start_level();
}

void kill_ship()
{
    ship_alive = false;
    ship_respawn_timer = 100;
    explode( ship_x, ship_y, 14, 2.5, 9, SHIP_CORE );
    explode( ship_x, ship_y, 10, 1.5, 4, SPARK_CORE );
    lives--;
    if( lives <= 0 ) game_over = true;
}

void destroy_rock( int i )
{
    int size = rock_size[ i ];
    rock_active[ i ] = false;

    if( size == 3 ) score += 20;
    else if( size == 2 ) score += 50;
    else score += 100;

    explode( rock_x[ i ], rock_y[ i ], 4 + size * 3, 1.0 + size * 0.5, 3 + size * 2, ROCK_CORE );

    if( size > 1 )
    {
        spawn_rock( rock_x[ i ], rock_y[ i ], size - 1 );
        spawn_rock( rock_x[ i ], rock_y[ i ], size - 1 );
    }
}

// -----------------------------------------------------------------------------
//   UPDATE
// -----------------------------------------------------------------------------

void update_ship()
{
    float speed;

    if( !ship_alive )
    {
        if( !game_over )
        {
            ship_respawn_timer--;
            if( ship_respawn_timer <= 0 ) reset_ship();
        }
        return;
    }

    if( gamepad_left() > 0 )  ship_angle -= 0.075;
    if( gamepad_right() > 0 ) ship_angle += 0.075;

    if( gamepad_up() > 0 || gamepad_button_a() > 0 )
    {
        ship_vx += cos( ship_angle ) * 0.12;
        ship_vy += sin( ship_angle ) * 0.12;
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

    ship_x = wrap( ship_x + ship_vx, screen_width, 12 );
    ship_y = wrap( ship_y + ship_vy, screen_height, 12 );

    // fire on press, auto-repeat while held
    if( fire_cooldown > 0 ) fire_cooldown--;
    if( gamepad_button_b() == 1 || ( gamepad_button_b() > 0 && fire_cooldown == 0 ) )
    {
        fire_bullet();
        fire_cooldown = 10;
    }

    if( ship_invulnerable > 0 ) ship_invulnerable--;
}

void update_world()
{
    int i, b;
    float dx, dy, r;

    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
      {
          r = rock_radius( rock_size[ i ] );
          rock_x[ i ] = wrap( rock_x[ i ] + rock_vx[ i ], screen_width, r );
          rock_y[ i ] = wrap( rock_y[ i ] + rock_vy[ i ], screen_height, r );
          rock_angle[ i ] += rock_spin[ i ];
      }

    for( b = 0; b < MAX_BULLETS; b++ )
      if( bullet_life[ b ] > 0 )
      {
          bullet_life[ b ]--;
          bullet_x[ b ] = wrap( bullet_x[ b ] + bullet_vx[ b ], screen_width, 2 );
          bullet_y[ b ] = wrap( bullet_y[ b ] + bullet_vy[ b ], screen_height, 2 );

          // bullet vs rocks
          for( i = 0; i < MAX_ROCKS; i++ )
            if( rock_active[ i ] )
            {
                dx = bullet_x[ b ] - rock_x[ i ];
                dy = bullet_y[ b ] - rock_y[ i ];
                r = rock_radius( rock_size[ i ] );
                if( dx * dx + dy * dy < r * r )
                {
                    bullet_life[ b ] = 0;
                    destroy_rock( i );
                    break;
                }
            }
      }

    // ship vs rocks
    if( ship_alive && ship_invulnerable == 0 )
      for( i = 0; i < MAX_ROCKS; i++ )
        if( rock_active[ i ] )
        {
            dx = ship_x - rock_x[ i ];
            dy = ship_y - rock_y[ i ];
            r = rock_radius( rock_size[ i ] ) + 7;
            if( dx * dx + dy * dy < r * r )
            {
                destroy_rock( i );
                kill_ship();
                break;
            }
        }

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

    // next wave
    if( count_rocks() == 0 && !game_over )
    {
        level++;
        start_level();
    }
}

// -----------------------------------------------------------------------------
//   DRAWING
// -----------------------------------------------------------------------------

#define STARS 50
float[ STARS ] star_x;
float[ STARS ] star_y;

void draw_world( int frame )
{
    int i;
    float c, s, f, len, fade;

    // dim stars (no glow, so they cost almost nothing)
    vec_thickness( 1 );
    vec_color( make_color_rgb( 70, 70, 100 ), 0 );
    for( i = 0; i < STARS; i++ )
      vec_line( star_x[ i ], star_y[ i ], star_x[ i ] + 1, star_y[ i ] );

    // rocks
    vec_thickness( 2 );
    vec_glow_size( 18 );
    vec_color( ROCK_CORE, ROCK_GLOW );
    for( i = 0; i < MAX_ROCKS; i++ )
      if( rock_active[ i ] )
        vec_shape( &rock_shape_x[ rock_shape[ i ] * ROCK_POINTS ],
                   &rock_shape_y[ rock_shape[ i ] * ROCK_POINTS ],
                   ROCK_POINTS, rock_x[ i ], rock_y[ i ], rock_angle[ i ],
                   rock_radius( rock_size[ i ] ) );

    // ship (blinks while invulnerable)
    if( ship_alive && ( ship_invulnerable == 0 || ( frame / 4 ) % 2 ) )
    {
        vec_color( SHIP_CORE, SHIP_GLOW );
        vec_shape( ship_model_x, ship_model_y, 4, ship_x, ship_y, ship_angle, 1.0 );

        // flickering thrust flame
        if( gamepad_up() > 0 || gamepad_button_a() > 0 )
        {
            c = cos( ship_angle );
            s = sin( ship_angle );
            len = 9 + ( rand() % 8 );
            vec_color( 0xFF40B0FF, 0xFF103060 );
            vec_line( ship_x - c * 6 - s * 4, ship_y - s * 6 + c * 4, ship_x - c * len, ship_y - s * len );
            vec_line( ship_x - c * 6 + s * 4, ship_y - s * 6 - c * 4, ship_x - c * len, ship_y - s * len );
        }
    }

    // bullets as short tracers
    vec_thickness( 2.5 );
    vec_glow_size( 12 );
    vec_color( BULLET_CORE, BULLET_GLOW );
    for( i = 0; i < MAX_BULLETS; i++ )
      if( bullet_life[ i ] > 0 )
        vec_line( bullet_x[ i ], bullet_y[ i ],
                  bullet_x[ i ] - bullet_vx[ i ] * 0.6, bullet_y[ i ] - bullet_vy[ i ] * 0.6 );

    // particles fade out by dimming their color
    vec_thickness( 1.5 );
    vec_glow_size( 10 );
    for( i = 0; i < MAX_PARTICLES; i++ )
      if( part_life[ i ] > 0 )
      {
          fade = part_life[ i ] / ( part_max_life[ i ] * 1.0 );
          f = 255 * fade;
          vec_color( make_color_rgba( get_color_red( part_color[ i ] ),
                                      get_color_green( part_color[ i ] ),
                                      get_color_blue( part_color[ i ] ), f ),
                     make_color_rgb( 110 * fade, 70 * fade, 40 * fade ) );
          c = cos( part_angle[ i ] ) * part_length[ i ] * 0.5;
          s = sin( part_angle[ i ] ) * part_length[ i ] * 0.5;
          vec_line( part_x[ i ] - c, part_y[ i ] - s, part_x[ i ] + c, part_y[ i ] + s );
      }
}

void draw_hud( int frame )
{
    int i;
    float w;

    vec_thickness( 2 );
    vec_glow_size( 12 );
    vec_color( TEXT_CORE, TEXT_GLOW );
    vec_number( score, 16, 12, 9 );

    // remaining lives as small ships
    vec_color( SHIP_CORE, SHIP_GLOW );
    for( i = 0; i < lives; i++ )
      vec_shape( ship_model_x, ship_model_y, 4, 24 + i * 22, 52, -1.5707963, 0.8 );

    vec_color( make_color_rgb( 150, 150, 190 ), 0 );
    vec_thickness( 1.5 );
    vec_text( "WAVE", 548, 14, 6 );
    vec_number( level, 548 + vec_text_width( "WAVE ", 6 ), 14, 6 );

    if( game_over )
    {
        vec_thickness( 3 );
        vec_glow_size( 20 );
        vec_color( 0xFF8080FF, 0xFF202070 );
        w = vec_text_width( "GAME OVER", 18 );
        vec_text( "GAME OVER", ( screen_width - w ) / 2, 135, 18 );

        if( ( frame / 30 ) % 2 )
        {
            vec_thickness( 2 );
            vec_glow_size( 12 );
            vec_color( TEXT_CORE, TEXT_GLOW );
            w = vec_text_width( "PRESS START", 8 );
            vec_text( "PRESS START", ( screen_width - w ) / 2, 215, 8 );
        }
    }
}

// -----------------------------------------------------------------------------
//   MAIN
// -----------------------------------------------------------------------------

void main()
{
    int frame = 0;
    int i;

    srand( get_time() + 1 );
    vec_init( 0, 0 );
    select_gamepad( 0 );

    build_rock_shapes();
    for( i = 0; i < STARS; i++ )
    {
        star_x[ i ] = frand() * screen_width;
        star_y[ i ] = frand() * screen_height;
    }

    new_game();

    while( true )
    {
        if( game_over && gamepad_button_start() == 1 )
          new_game();

        update_ship();
        update_world();

        clear_screen( make_color_rgb( 4, 3, 10 ) );
        draw_world( frame );
        draw_hud( frame );
        vec_flush();

        frame++;
        end_frame();
    }
}
