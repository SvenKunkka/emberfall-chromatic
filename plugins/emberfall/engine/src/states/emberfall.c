#pragma bank 255

#include <gbdk/platform.h>
#include <gb/cgb.h>
#include <string.h>
#include "states/emberfall.h"
#include "states/ef_art.h"
#include "actor.h"
#include "camera.h"
#include "scroll.h"
#include "input.h"
#include "game_time.h"
#include "math.h"

/* This project-local scene uses native GB Studio actors and renderer.
   Actor order is part of the authored scene contract: player, 6 foes,
   4 friendly bolts, 1 hostile bolt. All combat values are original. */
#define EF_FOES 6
#define EF_BOLTS 4
#define EF_TITLE 0
#define EF_PLAY 1
#define EF_UPGRADE 2
#define EF_DEAD 3
#define EF_WIN 4
#define EF_PAUSE 5
#define EF_GUIDE 6

typedef struct ef_foe_t { UBYTE x,y,kind,live,clock,flash; UWORD hp; } ef_foe_t;
typedef struct ef_bolt_t { INT16 x,y; BYTE dx,dy; UBYTE live,ttl; } ef_bolt_t;
ef_foe_t ef_foes[EF_FOES];
ef_bolt_t ef_bolts[EF_BOLTS],ef_acid;
UBYTE ef_mode,ef_stage,ef_x,ef_y,ef_level,ef_xp,ef_need;
UBYTE ef_atk,ef_rate,ef_armor,ef_dash_cd,ef_dash,ef_pulse_cd,ef_pulse_max;
UBYTE ef_iframe,ef_shot,ef_clock,ef_kills,ef_spawned,ef_quota,ef_spawn_clock;
UBYTE ef_choice,ef_boss_spawned,ef_boss_dead,ef_msg_clock,ef_msg,ef_lastjoy,ef_pulse_flash;
BYTE ef_dx,ef_dy;
UWORD ef_hp,ef_maxhp,ef_total,ef_seed;
UBYTE ef_row[20];
UBYTE ef_oam_rows[144];

const char * const ef_names[]={"ASH OUTSKIRTS","RUSTED ALLEY","BROKEN TRANSIT","ACID RESERVOIR","WARDEN OF RUST","REACTOR GARDEN","RED QUARANTINE","BLACK FOUNDRY","SIGNAL APPROACH","THE LAST WARDEN"};
const UBYTE ef_kind[6]={0,0,1,2,3,4};
const UWORD ef_themes[5][4]={
    {RGB(26,28,26),RGB(13,15,15),RGB(5,7,8),RGB(2,3,4)},
    {RGB(29,25,20),RGB(17,13,11),RGB(8,6,7),RGB(3,3,5)},
    {RGB(26,29,22),RGB(13,17,12),RGB(5,9,7),RGB(2,4,4)},
    {RGB(28,23,24),RGB(16,11,14),RGB(8,5,9),RGB(3,2,5)},
    {RGB(23,28,30),RGB(11,15,20),RGB(5,7,12),RGB(2,3,6)}
};
const UWORD ef_pulse_palette[]={RGB(30,31,28),RGB(20,26,22),RGB(9,17,18),RGB(4,10,11)};
void ef_theme(void){set_bkg_palette(0,1,ef_themes[(ef_stage-1)/2]);}

UBYTE ef_rand(void) { ef_seed=ef_seed*109u+89u; return ef_seed>>8; }
UBYTE ef_abs(INT16 v) { return v<0?-v:v; }
BYTE ef_sign(INT16 v) { return v<0?-1:v>0?1:0; }
UBYTE ef_distance(UBYTE ax,UBYTE ay,UBYTE bx,UBYTE by) { return ef_abs((INT16)ax-bx)+ef_abs((INT16)ay-by); }
void ef_attr(UBYTE x,UBYTE y,UBYTE w,UBYTE h,UBYTE p) {
    UBYTE yy; memset(ef_row,p,20);VBK_REG=1;
    for(yy=0;yy<h;yy++) set_bkg_tiles(x,y+yy,w,1,ef_row);
    VBK_REG=0;
}
void ef_text(UBYTE x,UBYTE y,const char *s) {
    UBYTE w=0;while(*s && x+w<20){ef_row[w++]=128+(*s++-32);}
    set_bkg_tiles(x,y,w,1,ef_row);ef_attr(x,y,w,1,7);
}
void ef_num(UBYTE x,UBYTE y,UWORD v,UBYTE digits) {
    UBYTE n=digits;while(n){ef_row[--n]=128+16+(v%10);v/=10;}
    set_bkg_tiles(x,y,digits,1,ef_row);ef_attr(x,y,digits,1,7);
}
void ef_box(void) {
    UBYTE y;memset(ef_row,128,20);
    for(y=4;y<15;y++)set_bkg_tiles(1,y,18,1,ef_row);
    ef_attr(1,4,18,11,7);
}
void ef_hide_all(void) { UBYTE i;for(i=0;i<12;i++)actors[i].flags|=ACTOR_FLAG_HIDDEN; }
void ef_pos(UBYTE i,UBYTE x,UBYTE y,UBYTE visible) {
    UBYTE row,top=y>8?y-8:0,bottom=y<136?y+8:144,objects=i<7?2:1;
    actors[i].pos.x=PX_TO_SUBPX((UWORD)x);actors[i].pos.y=PX_TO_SUBPX((UWORD)y);
    /* Reserve whole metasprites within the real ten-object scanline budget.
       Player/boss draw first; regular enemies rotate priority in dense packs.
       Projectiles use one object each and take only the remaining capacity. */
    if(visible){
        for(row=top;row<bottom;row++)if(ef_oam_rows[row]+objects>10){visible=0;break;}
        if(visible)for(row=top;row<bottom;row++)ef_oam_rows[row]+=objects;
    }
    if(visible)actors[i].flags&=~ACTOR_FLAG_HIDDEN;else actors[i].flags|=ACTOR_FLAG_HIDDEN;
}
void ef_sound(UBYTE type) {
    NR52_REG=0x80;NR50_REG=0x66;NR51_REG=0xFF;
    NR21_REG=0x80;NR22_REG=0x92;
    if(type==0){NR23_REG=0xD0;NR24_REG=0x86;}
    else if(type==1){NR23_REG=0x40;NR24_REG=0x87;}
    else if(type==2){NR23_REG=0x70;NR24_REG=0x83;}
    else {NR23_REG=0xB0;NR24_REG=0x87;}
}
void ef_message(UBYTE m){ef_msg=m;ef_msg_clock=80;}
void ef_hud(void) {
    ef_text(0,0,"HP    /     ZONE   ");ef_num(3,0,ef_hp,3);ef_num(7,0,ef_maxhp,3);ef_num(18,0,ef_stage,2);
    ef_text(0,1,"LV   XP    /   K   ");ef_num(2,1,ef_level,2);ef_num(8,1,ef_xp,2);ef_num(11,1,ef_need,2);ef_num(17,1,ef_kills,2);
    ef_text(0,2,"                    ");
    if(ef_foes[5].live){ef_text(0,2,"WARDEN HP");ef_num(10,2,ef_foes[5].hp,3);ef_text(14,2,"/ BOSS");}
    else {ef_text(1,2,ef_names[ef_stage-1]);}
    ef_text(0,16,"A DASH      B PULSE ");
    if(ef_dash_cd){ef_text(2,16,"WAIT");}else ef_text(2,16,"READY");
    if(ef_pulse_cd){ef_text(14,16,"WAIT ");}else ef_text(14,16,"READY");
    ef_text(0,17,"                    ");
    if(ef_msg_clock){
        if(ef_msg==1)ef_text(2,17,"LEVEL UP! +POWER");
        if(ef_msg==2)ef_text(3,17,"PULSE RELEASED");
        if(ef_msg==3)ef_text(3,17,"HIT! KEEP MOVING");
        if(ef_msg==4)ef_text(3,17,"WARDEN INBOUND!");
    }else{ef_text(1,17,"CLEAR");ef_num(7,17,ef_kills,2);ef_text(10,17,"/");ef_num(12,17,ef_quota,2);ef_text(16,17,"AUTO");}
}
void ef_render(void) {
    UBYTE i,j,flip=(ef_clock>>3)&1;
    memset(ef_oam_rows,0,sizeof(ef_oam_rows));
    ef_pos(0,ef_x,ef_y,!(ef_iframe&&flip));
    ef_pos(6,ef_foes[5].x,ef_foes[5].y,ef_foes[5].live&&!(ef_foes[5].flash&&flip));
    for(i=0;i<5;i++){j=(i+(ef_clock>>1))%5;ef_pos(j+1,ef_foes[j].x,ef_foes[j].y,ef_foes[j].live&&!(ef_foes[j].flash&&flip));}
    for(i=0;i<EF_BOLTS;i++)ef_pos(i+7,ef_bolts[i].x,ef_bolts[i].y,ef_bolts[i].live);
    ef_pos(11,ef_acid.x,ef_acid.y,ef_acid.live);
}
void ef_title(void) {
    ef_hide_all();ef_box();
    ef_text(2,5,"E M B E R F A L L");
    ef_text(3,7,"LONE SURVIVOR");
    ef_text(3,9,"THE LAST SIGNAL");
    ef_text(2,11,"START / A : DEPLOY");
    ef_text(2,13,"SELECT : GUIDE");
    ef_text(0,0,"  EMBERFALL / 01    ");ef_text(0,1," ORIGINAL GBC GAME  ");ef_text(0,2,"                    ");
    ef_text(0,16,"MOVE  AIM  SURVIVE  ");ef_text(0,17," 10 ZONES. ONE LIFE.");
}
void ef_guide(void) {
    ef_hide_all();ef_box();
    ef_text(3,4,"FIELD MANUAL");ef_text(2,6,"DPAD : MOVE / AIM");
    ef_text(2,7,"GUN  : AUTO TARGET");ef_text(2,8,"A    : DODGE DASH");ef_text(2,9,"B    : AREA PULSE");
    ef_text(2,10,"START: PAUSE");ef_text(2,12,"KILLS GIVE XP + HP");ef_text(2,13,"A / START : RETURN");
}
void ef_round(void) {
    UBYTE i;for(i=0;i<EF_FOES;i++){memset(&ef_foes[i],0,sizeof(ef_foe_t));ef_foes[i].kind=ef_kind[i];}
    memset(ef_bolts,0,sizeof(ef_bolts));memset(&ef_acid,0,sizeof(ef_acid));
    ef_kills=ef_spawned=ef_boss_spawned=ef_boss_dead=0;
    ef_quota=8+(ef_stage-1)*2;ef_spawn_clock=25;ef_shot=10;ef_dash_cd=ef_pulse_cd=0;
    ef_x=80;ef_y=88;ef_dx=0;ef_dy=1;ef_iframe=60;ef_clock=0;ef_msg_clock=0;
    ef_pulse_flash=0;ef_hide_all();scroll_repaint();set_bkg_data(128,64,ef_font);ef_theme();ef_hud();ef_mode=EF_PLAY;
}
void ef_newgame(void) {
    ef_seed=DIV_REG+game_time+163;ef_stage=ef_level=1;ef_xp=0;ef_need=5;
    ef_hp=ef_maxhp=60;ef_atk=8;ef_rate=32;ef_armor=0;ef_total=0;ef_pulse_max=240;ef_dash=0;
    ef_round();
}
void ef_reward(UBYTE kind) {
    ef_xp+=kind==4?6:kind>=2?2:1;ef_total++;
    while(ef_xp>=ef_need){
        ef_xp-=ef_need;ef_level++;ef_need=5+ef_level*2;ef_atk+=2;ef_maxhp+=5;
        ef_hp+=10;if(ef_hp>ef_maxhp)ef_hp=ef_maxhp;
        ef_sound(1);ef_message(1);
    }
    /* Small guaranteed sustain, no currency or monetization gates. */
    if((ef_total&3)==0 && ef_hp<ef_maxhp){ef_hp+=2;if(ef_hp>ef_maxhp)ef_hp=ef_maxhp;}
}
void ef_hurt_foe(UBYTE i,UBYTE damage) {
    ef_foe_t *f=&ef_foes[i];if(!f->live)return;
    f->flash=8;
    if(f->hp<=damage){f->live=0;ef_reward(f->kind);ef_sound(0);if(i==5)ef_boss_dead=1;else ef_kills++;}
    else f->hp-=damage;
}
void ef_hurt_player(UBYTE damage) {
    if(ef_iframe)return;
    damage=damage>ef_armor?damage-ef_armor:1;
    ef_sound(2);ef_iframe=55;ef_message(3);
    if(ef_hp>damage)ef_hp-=damage;
    else {
        ef_hp=0;ef_mode=EF_DEAD;ef_hide_all();ef_hud();ef_box();
        ef_text(4,5,"SIGNAL LOST");ef_text(2,7,"THE ASH REMEMBERS.");
        ef_text(2,9,"ZONE");ef_num(8,9,ef_stage,2);ef_text(2,10,"KILLS");ef_num(8,10,ef_total,3);
        ef_text(2,12,"A / START: RETRY");
    }
}
void ef_spawn(UBYTE i) {
    ef_foe_t *f=&ef_foes[i];UBYTE side=ef_rand()&3;
    if(side<2){f->x=side?144:16;f->y=56+(ef_rand()%56);}
    else{f->x=20+ef_rand()%120;f->y=side==2?48:120;}
    if(ef_distance(f->x,f->y,ef_x,ef_y)<42){f->x=ef_x<80?144:16;}
    f->hp=10+4*(ef_stage-1);if(f->kind==1)f->hp=f->hp*3/4;
    if(f->kind==2)f->hp*=2;if(f->kind==3)f->hp=f->hp*3/2;
    if(f->kind==4){f->hp=ef_stage==5?180:420;f->x=80;f->y=48;ef_message(4);}
    f->live=1;f->clock=0;f->flash=0;
}
void ef_fire(ef_bolt_t *b,UBYTE x,UBYTE y,UBYTE tx,UBYTE ty,UBYTE speed) {
    INT16 dx=(INT16)tx-x,dy=(INT16)ty-y;
    b->x=x;b->y=y;b->live=1;b->ttl=70;
    if(ef_abs(dx)>ef_abs(dy)*2){b->dx=ef_sign(dx)*speed;b->dy=0;}
    else if(ef_abs(dy)>ef_abs(dx)*2){b->dx=0;b->dy=ef_sign(dy)*speed;}
    else{b->dx=ef_sign(dx)*speed;b->dy=ef_sign(dy)*speed;}
}
void ef_shoot(void) {
    UBYTE i,target=255,best=255,d;
    for(i=0;i<EF_FOES;i++)if(ef_foes[i].live){d=ef_distance(ef_x,ef_y,ef_foes[i].x,ef_foes[i].y);if(d<best){best=d;target=i;}}
    if(target==255)return;
    for(i=0;i<EF_BOLTS;i++)if(!ef_bolts[i].live){ef_fire(&ef_bolts[i],ef_x,ef_y,ef_foes[target].x,ef_foes[target].y,3);break;}
}
void ef_upgrade_screen(void) {
    ef_hide_all();ef_box();
    ef_text(4,4,"ZONE SECURED");ef_text(3,6,"CHOOSE A SALVAGE");
    ef_text(3,8,"POWER +4 ATK");ef_text(3,10,"RAPID -4 DELAY");ef_text(3,12,"VITAL +20 HP");
    ef_text(1,8," ");ef_text(1,10," ");ef_text(1,12," ");ef_text(1,8+ef_choice*2,">");
    ef_text(1,14,"UP/DOWN + A CONFIRM");ef_text(0,17,"   NEXT ZONE READY  ");
}
void ef_finish(void) {
    ef_mode=EF_WIN;ef_hide_all();ef_hud();ef_box();
    ef_text(3,4,"SIGNAL RESTORED");ef_text(2,6,"10 ZONES RECLAIMED");
    ef_text(2,8,"NEVER ALONE.");ef_text(2,10,"LEVEL");ef_num(10,10,ef_level,2);
    ef_text(2,11,"KILLS");ef_num(10,11,ef_total,3);ef_text(2,13,"A / START: NEW RUN");ef_sound(1);
}

void emberfall_init(void) BANKED {
    UBYTE i;
    camera_offset_x=camera_offset_y=0;camera_deadzone_x=camera_deadzone_y=0;
    PLAYER.pos.x=PX_TO_SUBPX(80u);PLAYER.pos.y=PX_TO_SUBPX(88u);
    /* Explicitly pin and activate all authored actors, including projectile slots. */
    for(i=0;i<12;i++){
        actors[i].flags|=ACTOR_FLAG_PINNED|ACTOR_FLAG_PERSISTENT;
        actors[i].flags&=~ACTOR_FLAG_COLLISION;
        if(i)activate_actor(&actors[i]);
        actors[i].flags|=ACTOR_FLAG_HIDDEN;
    }
    ef_stage=1;ef_mode=EF_TITLE;ef_clock=0;ef_lastjoy=0;
}

void emberfall_update(void) BANKED {
    UBYTE i,j,press=joy&~ef_lastjoy;UBYTE move=0;
    ef_lastjoy=joy;ef_clock++;
    /* Repaint occurs after state_init, so initialize title on its first update. */
    if(ef_mode==EF_TITLE){
        if(ef_clock==1){set_bkg_data(128,64,ef_font);ef_title();}
        if(press&(J_A|J_START)){ef_newgame();return;}
        if(press&J_SELECT){ef_mode=EF_GUIDE;ef_guide();}return;
    }
    if(ef_mode==EF_GUIDE){if(press&(J_A|J_START|J_B)){ef_mode=EF_TITLE;ef_title();}return;}
    if(ef_mode==EF_DEAD||ef_mode==EF_WIN){if(press&(J_A|J_START))ef_newgame();return;}
    if(ef_mode==EF_PAUSE){
        if(press&J_START){ef_mode=EF_PLAY;scroll_repaint();set_bkg_data(128,64,ef_font);ef_theme();ef_hud();ef_render();}return;
    }
    if(ef_mode==EF_UPGRADE){
        if(press&J_UP){ef_choice=ef_choice?ef_choice-1:2;ef_upgrade_screen();}
        if(press&J_DOWN){ef_choice=(ef_choice+1)%3;ef_upgrade_screen();}
        if(press&J_A){
            if(ef_choice==0)ef_atk+=4;
            if(ef_choice==1){ef_rate=ef_rate>12?ef_rate-4:10;}
            if(ef_choice==2){ef_maxhp+=20;ef_armor++;}
            ef_hp+=20;if(ef_hp>ef_maxhp)ef_hp=ef_maxhp;
            ef_stage++;ef_dash=0;ef_round();
        }return;
    }
    if(press&J_START){ef_mode=EF_PAUSE;ef_hide_all();ef_box();ef_text(6,7,"PAUSED");ef_text(2,10,"START : KEEP GOING");return;}
    if(ef_iframe)ef_iframe--;if(ef_dash_cd)ef_dash_cd--;if(ef_pulse_cd)ef_pulse_cd--;if(ef_msg_clock)ef_msg_clock--;
    if(ef_pulse_flash && !--ef_pulse_flash)ef_theme();
    if(press&J_A && !ef_dash_cd){ef_dash=8;ef_dash_cd=80;ef_iframe=20;ef_sound(3);}
    if(ef_dash){
        ef_dash--;if(ef_dx<0&&ef_x>18)ef_x-=3;if(ef_dx>0&&ef_x<142)ef_x+=3;
        if(ef_dy<0&&ef_y>50)ef_y-=3;if(ef_dy>0&&ef_y<118)ef_y+=3;move=1;
    }else{
        ef_dx=ef_dy=0;
        if(INPUT_LEFT&&ef_x>16){ef_x--;ef_dx=-1;move=1;}
        if(INPUT_RIGHT&&ef_x<144){ef_x++;ef_dx=1;move=1;}
        if(INPUT_UP&&ef_y>48){ef_y--;ef_dy=-1;move=1;}
        if(INPUT_DOWN&&ef_y<120){ef_y++;ef_dy=1;move=1;}
        if(move){actor_set_dir(&PLAYER,ef_dx<0?DIR_LEFT:ef_dx>0?DIR_RIGHT:ef_dy<0?DIR_UP:DIR_DOWN,TRUE);}
        else{ef_dy=1;actor_set_anim_idle(&PLAYER);}
    }
    if(press&J_B && !ef_pulse_cd){
        ef_pulse_cd=ef_pulse_max;ef_message(2);ef_sound(3);ef_pulse_flash=8;set_bkg_palette(0,1,ef_pulse_palette);
        for(i=0;i<EF_FOES;i++)if(ef_foes[i].live&&ef_distance(ef_x,ef_y,ef_foes[i].x,ef_foes[i].y)<70)ef_hurt_foe(i,ef_atk*3);
        ef_acid.live=0;
    }
    if(ef_spawn_clock)ef_spawn_clock--;
    else if(ef_spawned<ef_quota){
        for(i=0;i<5;i++){
            j=(ef_spawned+i)%5;if(ef_foes[j].live)continue;
            if(j==2&&ef_stage<2)continue;if(j==3&&ef_stage<3)continue;if(j==4&&ef_stage<4)continue;
            ef_spawn(j);ef_spawned++;ef_spawn_clock=ef_stage<6?65:45;break;
        }
    }
    if((ef_stage==5||ef_stage==10)&&ef_spawned>=ef_quota/2&&!ef_boss_spawned){ef_spawn(5);ef_boss_spawned=1;}
    if(ef_shot)ef_shot--;else{ef_shoot();ef_shot=ef_rate;}
    for(i=0;i<EF_FOES;i++)if(ef_foes[i].live){
        ef_foe_t *f=&ef_foes[i];UBYTE d=ef_distance(ef_x,ef_y,f->x,f->y);f->clock++;if(f->flash)f->flash--;
        if(f->kind==3&&d<48){
            if((ef_clock&3)==0){if(f->x<ef_x&&f->x>16)f->x--;if(f->x>ef_x&&f->x<144)f->x++;}
        }else if((ef_clock%(f->kind==1?2:f->kind==2?5:3))==0){
            f->x+=ef_sign((INT16)ef_x-f->x);f->y+=ef_sign((INT16)ef_y-f->y);
        }
        /* Spitters shoot aimed acid. Bosses telegraph, then accelerate and shoot. */
        if((f->kind==3||f->kind==4)&&f->clock>=90){
            f->clock=0;if(!ef_acid.live)ef_fire(&ef_acid,f->x,f->y,ef_x,ef_y,2);
        }
        if(f->kind==4&&f->clock>65&&f->clock<80){
            f->flash=2;
            if(f->x<ef_x)f->x++;else if(f->x>ef_x)f->x--;
            if(f->y<ef_y)f->y++;else if(f->y>ef_y)f->y--;
        }
        if(d<12){ef_hurt_player(4+ef_stage+(f->kind>=2?3:0));if(ef_mode!=EF_PLAY)return;}
    }
    for(i=0;i<EF_BOLTS;i++)if(ef_bolts[i].live){
        ef_bolt_t *b=&ef_bolts[i];b->x+=b->dx;b->y+=b->dy;
        if(!(--b->ttl)||b->x<8||b->x>152||b->y<40||b->y>128){b->live=0;continue;}
        for(j=0;j<EF_FOES;j++)if(ef_foes[j].live&&ef_distance(b->x,b->y,ef_foes[j].x,ef_foes[j].y)<13){ef_hurt_foe(j,ef_atk);b->live=0;break;}
    }
    if(ef_acid.live){
        ef_acid.x+=ef_acid.dx;ef_acid.y+=ef_acid.dy;
        if(!(--ef_acid.ttl)||ef_acid.x<8||ef_acid.x>152||ef_acid.y<40||ef_acid.y>128)ef_acid.live=0;
        else if(ef_distance(ef_x,ef_y,ef_acid.x,ef_acid.y)<10){ef_acid.live=0;ef_hurt_player(6+ef_stage);if(ef_mode!=EF_PLAY)return;}
    }
    if(ef_kills>=ef_quota&&((ef_stage!=5&&ef_stage!=10)||ef_boss_dead)){
        if(ef_stage==10){ef_finish();return;}
        ef_mode=EF_UPGRADE;ef_choice=0;ef_hud();ef_upgrade_screen();ef_sound(1);return;
    }
    ef_render();if((ef_clock&7)==0)ef_hud();
}
