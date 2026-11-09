// ============================================
// Army Men RTS - NEW GAME from scratch
// PS2SDK + gsKit - for AetherSX2
// 10 Units/Buildings - Final List
// Map 5000x3000, WayPoints 10 (2 paths)
// Controls: Left Analog = Camera, X = Select/Confirm,
// Circle/Square = Back/Cancel, R1+Analog = Change Unit, Start = Pause
// Colors: Army = GREEN, Ants = RED, Cockroach = BROWN, Nursery = GREY-WHITE
// ============================================
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <pad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---------- Colors (GSKit RGBA) ----------
#define C_GREEN      GS_SETREG_RGBA(0x2E,0xA0,0x2E,0x80) // Army Men Plastic Green
#define C_GREEN_DARK GS_SETREG_RGBA(0x1E,0x70,0x1E,0x80)
#define C_RED        GS_SETREG_RGBA(0xC0,0x10,0x10,0x80) // Ants Red
#define C_RED_DARK   GS_SETREG_RGBA(0x80,0x00,0x00,0x80)
#define C_BROWN      GS_SETREG_RGBA(0x6B,0x42,0x1E,0x80) // Cockroach Brown
#define C_BROWN_LIGHT GS_SETREG_RGBA(0xA0,0x70,0x40,0x80)
#define C_GREY_WHITE GS_SETREG_RGBA(0xC8,0xC0,0xB0,0x80) // Nursery Mound
#define C_WHITE_SPOT GS_SETREG_RGBA(0xE8,0xE8,0xE0,0x80)
#define C_YELLOW     GS_SETREG_RGBA(0xFF,0xD0,0x00,0x80)
#define C_GROUND     GS_SETREG_RGBA(0x8A,0x7A,0x50,0x80) // Dirt ground
#define C_BLACK      GS_SETREG_RGBA(0x00,0x00,0x00,0x80)
#define C_WHITE      GS_SETREG_RGBA(0xFF,0xFF,0xFF,0x80)

// ---------- Game Constants ----------
#define MAP_W 5000
#define MAP_H 3000
#define MAX_UNITS 600
#define SPAWN_INTERVAL 20.0f // seconds
#define UNIT_SIZE 1.1f

// ---------- Resources (Start = 999,999,999) ----------
int plastic = 999999999;
int electricity = 999999999;

// ---------- Types ----------
typedef enum { T_WORKER, T_SOLDIER, T_COCKROACH, T_NURSERY, T_GTOWER, T_PILLBOX, T_BULLDOZER, T_BWPOST, T_BARRACKS, T_SNIPER, T_COUNT } UnitType;

typedef struct {
    char name[32];
    int hp, maxHp;
    float speed, seeingRange;
    int damage;
    float maxRange, minRange;
    float fireInterval, aimDelay, turnRate;
    int costPlastic, costElectric;
    float buildTime;
    u64 color;
    int isBuilding;
    int isEnemy; // 1 = Ant/Cockroach side, 0 = Army side
} UnitDef;

// ---------- FINAL DATA - 10 Units (from user .cfg + corrections) ----------
UnitDef defs[T_COUNT] = {
    // Worker (Ant)
    {"Worker",    80, 80,  50, 81, 30, 16, 0, 0.5f, 0, 120, 0, 0, 10, C_RED, 0, 1},
    // Soldier (Ant) - Version A: Damage 60 Range 16 Projectile
    {"Soldier",   160,160, 50, 81, 60, 16, 0, 0.5f, 0, 120, 0, 0, 15, C_RED, 0, 1},
    // Cockroach (Enemy)
    {"Cockroach", 300,300, 50, 81, 50, 30, 0, 0.5f, 0, 120, 0, 0, 0,  C_BROWN, 0, 1},
    // Nursery / Ant Hill - Spawner
    {"Nursery",   3000,3000, 0, 73, 0, 0, 0, 0, 0, 0, 0, 0, 0, C_GREY_WHITE, 1, 1},
    // G-Tower - Blind spot MinRange 8
    {"G-Tower",   800,800,  0, 72, 25, 72, 8, 0.8f, 0.3f, 180, 200, 0, 15, C_GREEN, 1, 0},
    // Pillbox - MinRange 0, Fire 0.1s very fast, Plastic 300 + Electric 200
    {"Pillbox",   800,800,  0, 60, 30, 60, 0, 0.1f, 0.0f, 360, 300, 200, 15, C_GREEN, 1, 0},
    // Bulldozer - HP 400 Speed 25 Sight 40, Cost 250+50, BuildTime 20, No Attack
    {"Bulldozer", 400,400, 25, 40, 0, 0, 0, 0, 0, 150, 250, 50, 20, C_GREEN, 0, 0},
    // BWPost - HP 150 Cost 25, BuildTime 3, Sight 20, Passive
    {"BWPost",    150,150,  0, 20, 0, 0, 0, 0, 0, 0, 25, 0, 3, C_GREEN, 1, 0},
    // Barracks - Replaces HQ, HP 1200 Cost 400, BuildTime 30, Sight 50
    {"Barracks",  1200,1200, 0, 50, 0, 0, 0, 0, 0, 0, 400, 0, 30, C_GREEN, 1, 0},
    // Sniper - HP 80 Speed 20 Sight 90, Cost 250+100, Damage 100, Range 10-80, Fire 2.5s, Aim 0.5s
    {"Sniper",    80, 80,  20, 90, 100, 80, 10, 2.5f, 0.5f, 360, 250, 100, 10, C_GREEN, 0, 0},
};

typedef struct {
    int active;
    UnitType type;
    float x, y; // map pos
    int hp;
    int wpIndex; // current waypoint
    int pathId; // 0 or 1
    float fireTimer;
    float buildTimer;
    int target; // index of target unit, -1 none
} Unit;

Unit units[MAX_UNITS];
int unitCount = 0;
int selected = -1;
int buildMenu = 0; // 0=none, 1=build menu open
int buildChoice = 0;
int paused = 0;
float camX = 2500, camY = 1500;
float spawnTimer = 0;

// ---------- WayPoints - 10 total, 2 paths ----------
// Path 0: WP0 -> WP1 -> WP2 -> WP3 -> WP4 (to Army base)
// Path 1: WP5 -> WP6 -> WP7 -> WP8 -> WP9 (to Army base)
float waypoints[10][2] = {
    {200, 500}, {1000, 500}, {1800, 800}, {2500, 1200}, {3200, 1500}, // Path 0
    {200, 2500}, {1000, 2500}, {1800, 2200}, {2500, 1800}, {3200, 1500} // Path 1
};

// ---------- Helpers ----------
int spawnUnit(UnitType t, float x, float y, int pathId) {
    if (unitCount >= MAX_UNITS) return -1;
    for (int i=0;i<MAX_UNITS;i++) if (!units[i].active) {
        units[i].active = 1; units[i].type = t;
        units[i].x = x; units[i].y = y;
        units[i].hp = defs[t].hp;
        units[i].wpIndex = (pathId==0?0:5);
        units[i].pathId = pathId;
        units[i].fireTimer = 0; units[i].buildTimer = 0; units[i].target = -1;
        unitCount++; return i;
    }
    return -1;
}
float dist(float x1,float y1,float x2,float y2){ float dx=x2-x1, dy=y2-y1; return sqrtf(dx*dx+dy*dy); }

// Find nearest enemy in range, respecting MinRange blind spot
int findTarget(int idx) {
    Unit *u = &units[idx];
    UnitDef *d = &defs[u->type];
    if (d->damage==0) return -1;
    int best=-1; float bestD=1e9;
    for (int i=0;i<MAX_UNITS;i++) if (units[i].active && i!=idx) {
        if (defs[units[i].type].isEnemy == d->isEnemy) continue; // same side
        float dd = dist(u->x,u->y,units[i].x,units[i].y);
        if (dd >= d->minRange && dd <= d->maxRange && dd < bestD) { bestD=dd; best=i; }
    }
    return best;
}

// BWPost rule: if enemy within HALF range, enemy destroys BWPost
void bwpostRule() {
    for (int i=0;i<MAX_UNITS;i++) if (units[i].active && units[i].type==T_BWPOST) {
        for (int j=0;j<MAX_UNITS;j++) if (units[j].active && defs[units[j].type].isEnemy) {
            float half = defs[units[j].type].maxRange / 2.0f;
            if (dist(units[i].x,units[i].y,units[j].x,units[j].y) < half) {
                units[i].hp -= 5; // BWPost slowly destroyed when enemy close
                if (units[i].hp<=0) { units[i].active=0; unitCount--; }
                break;
            }
        }
    }
}

// Pillbox / G-Tower aggressive: enemies stop patrol and come to attack them
void aggressiveRule() {
    int hasAggro = 0;
    for (int i=0;i<MAX_UNITS;i++) if (units[i].active && (units[i].type==T_PILLBOX || units[i].type==T_GTOWER)) hasAggro=1;
    if (!hasAggro) return;
    for (int i=0;i<MAX_UNITS;i++) if (units[i].active && defs[units[i].type].isEnemy && units[i].type!=T_NURSERY) {
        // find nearest Pillbox/G-Tower and move toward it
        int best=-1; float bestD=1e9;
        for (int j=0;j<MAX_UNITS;j++) if (units[j].active && (units[j].type==T_PILLBOX || units[j].type==T_GTOWER)) {
            float dd=dist(units[i].x,units[i].y,units[j].x,units[j].y);
            if (dd<bestD){bestD=dd;best=j;}
        }
        if (best!=-1 && bestD>10) {
            float dx=units[best].x-units[i].x, dy=units[best].y-units[i].y;
            float len=sqrtf(dx*dx+dy*dy); if(len>0){ units[i].x+=dx/len*defs[units[i].type].speed*0.016f; units[i].y+=dy/len*defs[units[i].type].speed*0.016f; }
        }
    }
}

void update(float dt) {
    if (paused) return;
    // Spawn every 20s: Worker 3 + Soldier 2 + Cockroach 1 = 6
    spawnTimer += dt;
    if (spawnTimer >= SPAWN_INTERVAL) {
        spawnTimer = 0;
        for (int k=0;k<3;k++) spawnUnit(T_WORKER, 200, 500, 0);
        for (int k=0;k<2;k++) spawnUnit(T_SOLDIER, 200, 2500, 1);
        spawnUnit(T_COCKROACH, 200, 1500, 0);
    }
    // Move ants along waypoints if no aggro target
    for (int i=0;i<MAX_UNITS;i++) if (units[i].active) {
        Unit *u=&units[i]; UnitDef *d=&defs[u->type];
        if (d->isBuilding) continue;
        if (d->isEnemy) {
            // waypoint patrol
            int wp = u->wpIndex;
            if (wp>=0 && wp<10) {
                float tx=waypoints[wp][0], ty=waypoints[wp][1];
                float dd=dist(u->x,u->y,tx,ty);
                if (dd<20) {
                    if (u->pathId==0 && wp<4) u->wpIndex++;
                    else if (u->pathId==1 && wp<9) u->wpIndex++;
                } else {
                    float dx=tx-u->x, dy=ty-u->y; float len=sqrtf(dx*dx+dy*dy);
                    if(len>0){ u->x+=dx/len*d->speed*dt*0.5f; u->y+=dy/len*d->speed*dt*0.5f; }
                }
            }
        }
        // Combat
        if (d->damage>0) {
            u->fireTimer -= dt;
            int tgt = findTarget(i);
            u->target = tgt;
            if (tgt!=-1 && u->fireTimer<=0) {
                // Aim delay simulated by fireTimer reset
                units[tgt].hp -= d->damage;
                u->fireTimer = d->fireInterval + d->aimDelay;
                if (units[tgt].hp<=0){ units[tgt].active=0; unitCount--; }
            }
        }
    }
    bwpostRule();
    aggressiveRule();
}

// ---------- Drawing (simple colored quads - .obj shape replaced by colored box for PS2 test, full .obj loader in next version) ----------
void drawUnit(GSGLOBAL *gs, Unit *u) {
    UnitDef *d = &defs[u->type];
    float sx = u->x - camX + 320;
    float sy = u->y - camY + 240;
    if (sx<-50||sx>700||sy<-50||sy>500) return;
    float s = d->isBuilding ? 18 : 8;
    if (u->type==T_NURSERY) s=28;
    if (u->type==T_BULLDOZER) s=14;
    // Body - color from defs
    gsKit_prim_quad(gs, sx-s, sy-s, sx+s, sy-s, sx-s, sy+s, sx+s, sy+s, 1, d->color);
    // HP bar
    float hpPct = (float)u->hp / (float)d->maxHp; if(hpPct<0) hpPct=0;
    gsKit_prim_quad(gs, sx-s, sy-s-6, sx-s+s*2*hpPct, sy-s-6, sx-s, sy-s-2, sx-s+s*2*hpPct, sy-s-2, 1, C_GREEN_DARK);
    // Selection ring
    if (selected!=-1 && &units[selected]==u) {
        gsKit_prim_quad(gs, sx-s-3, sy-s-3, sx+s+3, sy-s-3, sx-s-3, sy+s+3, sx+s+3, sy+s+3, 0, C_WHITE);
    }
}

int main(int argc, char *argv[]) {
    SifInitRpc(0);
    GSGLOBAL *gs = gsKit_init_global();
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_ONESHOT);
    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFB_OFF, D_CTRL_STS_UNSPEC, 0, 0, 1);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_FROMSPR);
    dmaKit_chan_init(DMA_CHANNEL_TOSPR);
    padInit(0); padPortOpen(0,0, (u32*)0);

    // Initial state: 1 Bulldozer only + 1 Nursery (enemy base)
    memset(units,0,sizeof(units));
    spawnUnit(T_BULLDOZER, 3200, 1500, 0);
    spawnUnit(T_NURSERY, 200, 1500, 0);

    struct padButtonStatus buttons;
    u32 old_pad=0;
    u64 lastTime = 0;

    while(1) {
        padRead(0,0,&buttons);
        u32 paddata = 0xffff ^ buttons.btns;

        // Left Analog = Camera / Map move
        int lx = buttons.ljoy_h - 128;
        int ly = buttons.ljoy_v - 128;
        if (abs(lx)>20) camX += lx * 0.08f;
        if (abs(ly)>20) camY += ly * 0.08f;
        if (camX<320) camX=320; if (camX>MAP_W-320) camX=MAP_W-320;
        if (camY<240) camY=240; if (camY>MAP_H-240) camY=MAP_H-240;

        // R1 + Left Analog = Change / Select unit (simplified: R1 cycles selection)
        if ((paddata & PAD_R1) && !(old_pad & PAD_R1)) {
            // cycle to next army unit
            for (int i=0;i<MAX_UNITS;i++){ int idx=(selected+1+i)%MAX_UNITS; if(units[idx].active && !defs[units[idx].type].isEnemy){ selected=idx; break; } }
        }
        // X = Select / Confirm build
        if ((paddata & PAD_CROSS) && !(old_pad & PAD_CROSS)) {
            if (buildMenu) {
                // Build: Barracks first rule - if no Barracks, only Barracks can be built
                int hasBarracks=0; for(int i=0;i<MAX_UNITS;i++) if(units[i].active && units[i].type==T_BARRACKS) hasBarracks=1;
                // buildChoice: 0=Barracks(400),1=Pillbox(300+200),2=G-Tower(200),3=BWPost(25),4=Sniper(250+100 via Barracks)
                int costsP[5]={400,300,200,25,250};
                int costsE[5]={0,200,0,0,100};
                UnitType types[5]={T_BARRACKS,T_PILLBOX,T_GTOWER,T_BWPOST,T_SNIPER};
                if (!hasBarracks && buildChoice!=0) {
                    // blocked - must build Barracks first
                } else if (plastic>=costsP[buildChoice] && electricity>=costsE[buildChoice]) {
                    plastic-=costsP[buildChoice]; electricity-=costsE[buildChoice];
                    float bx = camX + 40, by = camY + 40;
                    spawnUnit(types[buildChoice], bx, by, 0);
                }
                buildMenu=0;
            } else {
                // select nearest army unit to camera center
                int best=-1; float bd=1e9;
                for(int i=0;i<MAX_UNITS;i++) if(units[i].active && !defs[units[i].type].isEnemy){
                    float dd=dist(units[i].x,units[i].y,camX,camY); if(dd<bd){bd=dd;best=i;}
                }
                selected=best;
            }
        }
        // Circle / Square = Back / Cancel
        if (((paddata & PAD_CIRCLE) && !(old_pad & PAD_CIRCLE)) || ((paddata & PAD_SQUARE) && !(old_pad & PAD_SQUARE))) {
            if (buildMenu) buildMenu=0; else selected=-1;
        }
        // Triangle = Not used
        // Start = Pause / Replay / Play again
        if ((paddata & PAD_START) && !(old_pad & PAD_START)) {
            if (paused) {
                // Replay: reset
                memset(units,0,sizeof(units)); unitCount=0; selected=-1;
                plastic=999999999; electricity=999999999;
                spawnUnit(T_BULLDOZER, 3200, 1500, 0);
                spawnUnit(T_NURSERY, 200, 1500, 0);
                paused=0;
            } else paused=1;
        }
        // Open build menu with R1+X alternative: if Bulldozer selected and X held? Use R2 to open build menu for simplicity + also Circle long? Here: L1 opens build menu
        if ((paddata & PAD_L1) && !(old_pad & PAD_L1)) { buildMenu = !buildMenu; buildChoice=0; }
        if (buildMenu) {
            if ((paddata & PAD_UP) && !(old_pad & PAD_UP)) { if(buildChoice>0) buildChoice--; }
            if ((paddata & PAD_DOWN) && !(old_pad & PAD_DOWN)) { if(buildChoice<4) buildChoice++; }
        }

        old_pad = paddata;
        update(0.016f);

        // ---------- Render ----------
        gsKit_clear(gs, C_GROUND);
        // Draw ground grid (map feel)
        for (int gx=0; gx<MAP_W; gx+=500) {
            float sx = gx - camX + 320;
            if (sx>=0 && sx<640) gsKit_prim_line(gs, sx, 0, sx, 448, 1, C_BLACK);
        }
        for (int gy=0; gy<MAP_H; gy+=500) {
            float sy = gy - camY + 240;
            if (sy>=0 && sy<448) gsKit_prim_line(gs, 0, sy, 640, sy, 1, C_BLACK);
        }
        // Waypoints (small dots)
        for (int w=0; w<10; w++) {
            float sx = waypoints[w][0]-camX+320, sy=waypoints[w][1]-camY+240;
            if (sx>=0&&sx<640&&sy>=0&&sy<448) gsKit_prim_quad(gs, sx-2,sy-2,sx+2,sy-2,sx-2,sy+2,sx+2,sy+2,1,C_WHITE);
        }
        for (int i=0;i<MAX_UNITS;i++) if (units[i].active) drawUnit(gs, &units[i]);

        // HUD - Text via gsKit font (simplified: use gsKit_printf if available, fallback: no text)
        // Note: Real HUD text needs gsKit font - kept minimal for compile
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
        gsKit_TexManager_nextFrame(gs);
    }
    return 0;
}
