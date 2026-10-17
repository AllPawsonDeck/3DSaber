#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "vshader_shbin.h"

#define TOP_CLEAR 0x050712FF
#define BOTTOM_CLEAR C2D_Color32(5,7,18,255)
#define MAX_NOTES 700
#define SR 22050
#define MUSIC_CH 8
#define UI_CH 9
#define DISPLAY_TRANSFER_FLAGS (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

typedef struct { float x,y,z; } Vertex;
typedef struct { const char* name; const char* genre; int bpm, root, seed, style, beats; } Track;
typedef struct { float t; u8 col,row,dir; bool done; } Note;

enum { ST_MENU, ST_GAME, ST_PAUSE, ST_RESULTS };

static const Track tracks[] = {
 {"Neon Skyline","RETROWAVE",110,45,481,0,136},
 {"Chrome Rush","ELECTRO HOUSE",128,42,889,1,136},
 {"Afterglow","MELODIC DANCE",144,48,129,2,136},
 {"Midnight Run","DRUM & BASS",164,38,631,3,136},
 {"Prism Pulse","FUTURE BASS",150,46,902,4,136},
 {"Overdrive","HARD DANCE",180,40,1771,5,136}
};
#define TRACK_COUNT ((int)(sizeof(tracks)/sizeof(tracks[0])))
static const char* diffNames[] = {"EASY","NORMAL","EXPERT"};
static const float dirVec[8][2] = {{0,1},{0,-1},{1,0},{-1,0},{.707f,.707f},{-.707f,.707f},{.707f,-.707f},{-.707f,-.707f}};

static const Vertex cubeVerts[] = {
 {-0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f,-0.5f, 0.5f},
 {-0.5f,-0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},
 { 0.5f,-0.5f,-0.5f},{ 0.5f, 0.5f,-0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f,-0.5f},
 {-0.5f,-0.5f,-0.5f},{-0.5f,-0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f, 0.5f, 0.5f},{-0.5f, 0.5f,-0.5f},{-0.5f,-0.5f,-0.5f},
 {-0.5f, 0.5f,-0.5f},{-0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f, 0.5f},{ 0.5f, 0.5f,-0.5f},{-0.5f, 0.5f,-0.5f},
 {-0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f,-0.5f},{ 0.5f,-0.5f, 0.5f},{ 0.5f,-0.5f, 0.5f},{-0.5f,-0.5f, 0.5f},{-0.5f,-0.5f,-0.5f}
};

static C3D_RenderTarget *topTarget,*bottomTarget;
static DVLB_s* dvlb;
static shaderProgram_s program;
static int uProjection,uModel,uTint;
static C3D_Mtx projection;
static C3D_AttrInfo attrInfo;
static C3D_BufInfo bufInfo;
static void* vbo;
static C2D_TextBuf textBuf;

static int state=ST_MENU, selected=0, difficulty=0;
static Note notes[MAX_NOTES]; static int noteCount=0;
static int score=0, combo=0, bestCombo=0, hits=0, misses=0; static float health=100.0f;
static u64 gameStart=0, pausedAt=0; static float elapsed=0, songDuration=0;
static float saberX=0,saberY=-0.85f,saberAngle=0; static touchPosition lastTouch={160,180}; static bool hadTouch=false;
static bool audioOK=false; static s16 *musicBuf=NULL,*uiBuf=NULL; static u32 musicBytes=0,uiBytes=0; static u64 previewUntil=0;
static float logoSpin=0;
static u32 rngState=1;

static float clampf(float v,float a,float b){return v<a?a:v>b?b:v;}
static u32 rng(void){rngState^=rngState<<13; rngState^=rngState>>17; rngState^=rngState<<5; return rngState;}
static float midiHz(float n){return 440.0f*powf(2.0f,(n-69.0f)/12.0f);}

static void bind3D(void){
 C3D_BindProgram(&program); C3D_SetAttrInfo(&attrInfo); C3D_SetBufInfo(&bufInfo);
 C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_ALL); C3D_CullFace(GPU_CULL_NONE);
 C3D_TexEnv* env=C3D_GetTexEnv(0); C3D_TexEnvInit(env); C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0); C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uProjection,&projection);
}
static void drawCubeM(const C3D_Mtx* m,float r,float g,float b,float a){
 C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uModel,m); C3D_FVUnifSet(GPU_VERTEX_SHADER,uTint,r,g,b,a); C3D_DrawArrays(GPU_TRIANGLES,0,36);
}
static void part(const C3D_Mtx* root,float x,float y,float z,float sx,float sy,float sz,float rz,float r,float g,float b){
 C3D_Mtx m=*root; Mtx_Translate(&m,x,y,z,true); if(rz!=0)Mtx_RotateZ(&m,rz,true); Mtx_Scale(&m,sx,sy,sz); drawCubeM(&m,r,g,b,1);
}
static void textAt(float x,float y,float scale,u32 color,const char* fmt,...){
 char s[180]; va_list ap; va_start(ap,fmt); vsnprintf(s,sizeof(s),fmt,ap); va_end(ap); C2D_Text t; C2D_TextParse(&t,textBuf,s); C2D_TextOptimize(&t); C2D_DrawText(&t,C2D_WithColor,x,y,0.5f,scale,scale,color);
}
static void textCenter(float y,float scale,u32 color,const char* fmt,...){
 char s[180]; va_list ap; va_start(ap,fmt); vsnprintf(s,sizeof(s),fmt,ap); va_end(ap); C2D_Text t; C2D_TextParse(&t,textBuf,s); C2D_TextOptimize(&t); C2D_DrawText(&t,C2D_WithColor|C2D_AlignCenter,160,y,0.5f,scale,scale,color);
}

static void stopChannel(int ch){ if(audioOK){CSND_SetPlayState(ch,0); csndExecCmds(false);} }
static void buildUiSound(void){
 int n=(int)(SR*.13f); uiBytes=n*sizeof(s16); uiBuf=(s16*)linearAlloc(uiBytes); if(!uiBuf)return;
 for(int i=0;i<n;i++){float t=(float)i/SR, p=(float)i/n, f=520+560*p; float e=(1-p)*(1-p); float v=sinf(6.2831853f*f*t)+.3f*sinf(6.2831853f*(f*1.5f)*t); uiBuf[i]=(s16)(v*e*9000);}
 GSPGPU_FlushDataCache(uiBuf,uiBytes);
}
static void playUiSound(void){ if(audioOK&&uiBuf)csndPlaySound(UI_CH,SOUND_ONE_SHOT|SOUND_FORMAT_16BIT|SOUND_LINEAR_INTERP,SR,.75f,0,uiBuf,NULL,uiBytes); }
static void buildMusic(const Track* tr){
 stopChannel(MUSIC_CH); if(musicBuf){linearFree(musicBuf);musicBuf=NULL;}
 float beat=60.0f/tr->bpm; float len=beat*16.0f; int n=(int)(SR*len); musicBytes=n*sizeof(s16); musicBuf=(s16*)linearAlloc(musicBytes); if(!musicBuf)return;
 static const int leadPat[16]={0,7,12,7,3,10,12,15,0,7,12,14,7,3,10,7};
 for(int i=0;i<n;i++){
  float t=(float)i/SR; float bp=t/beat; int bi=(int)bp; float ph=bp-bi; float out=0;
  float kickEnv=expf(-ph*9.0f); out += sinf(6.2831853f*(58+70*(1-ph))*t)*kickEnv*.55f;
  float bass=midiHz((float)tr->root + ((bi/4)%4==1?-4:(bi/4)%4==2?3:0)); out += sinf(6.2831853f*bass*t)*.18f;
  float lp=ph<.55f?(1.0f-ph/.55f):0; float lead=midiHz((float)tr->root+24+leadPat[bi&15]); out += sinf(6.2831853f*lead*t)*lp*.14f;
  if(tr->style==3||tr->style==5){float h=fmodf(bp*2.0f,1.0f); if(h<.16f)out += ((i*1103515245u+12345u)&1?1:-1)*(1-h/.16f)*.08f;}
  if(tr->style==4) out += sinf(6.2831853f*(lead*.5f)*t)*lp*.08f;
  out=tanhf(out*.95f); musicBuf[i]=(s16)(out*15000);
 }
 GSPGPU_FlushDataCache(musicBuf,musicBytes);
}
static void startMusic(bool repeat){ if(audioOK&&musicBuf)csndPlaySound(MUSIC_CH,(repeat?SOUND_REPEAT:SOUND_ONE_SHOT)|SOUND_FORMAT_16BIT|SOUND_LINEAR_INTERP,SR,.72f,0,musicBuf,repeat?musicBuf:NULL,musicBytes); }
static void previewTrack(void){ buildMusic(&tracks[selected]); startMusic(false); previewUntil=osGetTime()+2500; }

static void generateChart(void){
 noteCount=0; const Track* tr=&tracks[selected]; rngState=(u32)(tr->seed+difficulty*75+1234); float b=60.0f/tr->bpm; float beat=4.0f; int id=0;
 while(beat<tr->beats-2 && noteCount<MAX_NOTES){
  float interval=difficulty==0?1.0f:(difficulty==1?((id&3)>=2?.5f:1.0f):.5f); if(difficulty==2&&((int)beat%16)==15)interval=.25f;
  Note* n=&notes[noteCount++]; n->t=beat*b; n->col=(u8)(rng()%4); n->row=(u8)(rng()%3); n->dir=(u8)(difficulty==0?(rng()%4):(rng()%8)); n->done=false; id++; beat+=interval;
 }
 songDuration=tr->beats*b;
}
static void startGame(void){
 stopChannel(MUSIC_CH); generateChart(); buildMusic(&tracks[selected]); startMusic(true);
 score=combo=bestCombo=hits=misses=0; health=100; elapsed=0; hadTouch=false; saberX=0; saberY=-.85f; saberAngle=0; gameStart=osGetTime(); state=ST_GAME;
}
static void finishGame(void){ stopChannel(MUSIC_CH); state=ST_RESULTS; }
static void hitNote(Note* n,float quality){ n->done=true; combo++; if(combo>bestCombo)bestCombo=combo; hits++; int pts=(int)(100+quality*40+clampf(combo,0,30)*2); score+=pts; health=clampf(health+1.4f,0,100); playUiSound(); }
static void missNote(Note* n){ n->done=true; combo=0; misses++; health-=difficulty==2?13:11; if(health<=0)finishGame(); }

static void updateGame(u32 held){
 elapsed=(osGetTime()-gameStart)/1000.0f;
 if(previewUntil && osGetTime()>previewUntil){stopChannel(MUSIC_CH);previewUntil=0;}
 for(int i=0;i<noteCount;i++) if(!notes[i].done && elapsed>notes[i].t+.34f) missNote(&notes[i]);
 if(state!=ST_GAME)return;
 if(elapsed>songDuration+.4f){finishGame();return;}
 if(held&KEY_TOUCH){
  touchPosition p; hidTouchRead(&p); float x=((float)p.px/320.0f)*3.6f-1.8f; float y=(1.0f-(float)p.py/120.0f)*1.15f;
  if(hadTouch){float dx=(float)p.px-lastTouch.px, dy=-(float)p.py+lastTouch.py; float mag=sqrtf(dx*dx+dy*dy); if(mag>2.2f){
    float sx=dx/mag, sy=dy/mag; saberAngle=atan2f(sy,sx)-1.5707963f;
    for(int i=0;i<noteCount;i++){Note* n=&notes[i]; if(n->done)continue; float dt=fabsf(n->t-elapsed); if(dt>.28f)continue; float nx=(n->col-1.5f)*.9f, ny=(1-(int)n->row)*.75f; float ddx=x-nx,ddy=y-ny; if(ddx*ddx+ddy*ddy>.42f)continue; float dot=sx*dirVec[n->dir][0]+sy*dirVec[n->dir][1]; float need=difficulty==0?.05f:difficulty==1?.20f:.32f; if(dot>=need)hitNote(n,clampf(dot,0,1)); else {n->done=true;combo=0;misses++;health-=6;playUiSound();} break; }
  }}
  saberX=x;saberY=y;lastTouch=p;hadTouch=true;
 } else hadTouch=false;
}

static void drawLogo(void){
 bind3D(); C3D_Mtx root; Mtx_Identity(&root); Mtx_Translate(&root,0,0,-5.4f,true); Mtx_RotateX(&root,C3D_AngleFromDegrees(-7),true); Mtx_RotateY(&root,logoSpin,true); logoSpin+=.010f;
 part(&root,-.62f,.5f,0,1.05f,1.05f,.5f,0,.95f,.05f,.08f); part(&root,.62f,.5f,0,1.05f,1.05f,.5f,0,.03f,.35f,1.0f);
 part(&root,-.25f,-.25f,.34f,.18f,1.55f,.18f,C3D_AngleFromDegrees(-31),.48f,.50f,.52f); part(&root,.27f,-.25f,.38f,.18f,1.55f,.18f,C3D_AngleFromDegrees(31),.95f,.96f,.98f);
 part(&root,0,-1.35f,-.15f,2.7f,.03f,.03f,0,.07f,.65f,.85f);
}
static void drawArena(void){
 bind3D(); C3D_Mtx m; Mtx_Identity(&m);
 for(int lane=0;lane<4;lane++){float x=(lane-1.5f)*.9f; part(&m,x,-1.35f,-8.2f,.025f,.025f,11.0f,0,.06f,.30f,.42f);}
 for(int r=0;r<3;r++){float y=(1-r)*.75f; part(&m,0,y,-8.0f,3.5f,.018f,11.0f,0,.20f,.06f,.22f);}
 for(int i=0;i<noteCount;i++){Note* n=&notes[i]; if(n->done)continue; float dt=n->t-elapsed; if(dt>1.8f||dt<-.36f)continue; float z=-2.65f-dt*6.1f; float x=(n->col-1.5f)*.9f, y=(1-(int)n->row)*.75f; C3D_Mtx root; Mtx_Identity(&root); Mtx_Translate(&root,x,y,z,true); float wob=sinf((elapsed+n->t)*4)*.05f; Mtx_RotateY(&root,wob,true); part(&root,0,0,0,.58f,.58f,.58f,0,.03f,.38f,.95f); float ang=atan2f(dirVec[n->dir][1],dirVec[n->dir][0])-1.5707963f; part(&root,0,0,.34f,.08f,.36f,.055f,ang,.96f,.98f,1.0f); part(&root,dirVec[n->dir][0]*.18f,dirVec[n->dir][1]*.18f,.36f,.18f,.18f,.06f,ang,.96f,.98f,1.0f); }
 C3D_Mtx s; Mtx_Identity(&s); Mtx_Translate(&s,saberX,saberY,-2.05f,true); part(&s,0,-.36f,0,.18f,.42f,.16f,saberAngle,.18f,.20f,.23f); part(&s,0,.43f,0,.11f,1.25f,.11f,saberAngle,.12f,.88f,1.0f);
}

static void renderMenu2D(void){
 C2D_TextBufClear(textBuf); C2D_DrawRectSolid(0,0,0,320,240,BOTTOM_CLEAR);
 textCenter(11,.72f,C2D_Color32(245,248,255,255),"GPT SABER 3DS"); textCenter(33,.40f,C2D_Color32(88,220,255,255),"OLD 3DS NATIVE EDITION");
 textAt(14,61,.48f,C2D_Color32(160,170,195,255),"SONG");
 for(int i=0;i<TRACK_COUNT;i++){float y=80+i*20; u32 c=i==selected?C2D_Color32(40,220,255,255):C2D_Color32(215,220,232,255); if(i==selected)C2D_DrawRectSolid(8,y-3,.1f,304,18,C2D_Color32(18,52,74,255)); textAt(17,y,.43f,c,"%s",tracks[i].name); textAt(225,y,.36f,C2D_Color32(115,125,150,255),"%d BPM",tracks[i].bpm);}
 textAt(14,204,.42f,C2D_Color32(160,170,195,255),"L/R DIFFICULTY"); textAt(136,204,.48f,C2D_Color32(255,255,255,255),"%s",diffNames[difficulty]);
 textCenter(224,.36f,C2D_Color32(130,140,165,255),"A PLAY   X PREVIEW   D-PAD SONG   START EXIT");
}
static void renderGame2D(void){
 C2D_TextBufClear(textBuf); C2D_DrawRectSolid(0,0,0,320,240,C2D_Color32(3,8,16,255));
 textAt(9,7,.43f,C2D_Color32(232,237,247,255),"%s",tracks[selected].name); textAt(235,7,.38f,C2D_Color32(72,218,255,255),"%s",diffNames[difficulty]);
 textAt(9,28,.42f,C2D_Color32(170,180,205,255),"SCORE %d",score); textAt(125,28,.42f,C2D_Color32(170,180,205,255),"COMBO %dx",combo); textAt(240,28,.42f,C2D_Color32(170,180,205,255),"%d%%",(int)health);
 C2D_DrawRectSolid(10,47,.1f,300,5,C2D_Color32(30,36,52,255)); C2D_DrawRectSolid(10,47,.2f,300*clampf(health/100,0,1),5,health>35?C2D_Color32(42,220,255,255):C2D_Color32(255,55,88,255));
 float ox=16,oy=70,cw=72,ch=45; for(int r=0;r<3;r++)for(int c=0;c<4;c++){u32 col=((r+c)&1)?C2D_Color32(12,28,42,255):C2D_Color32(10,22,34,255); C2D_DrawRectSolid(ox+c*cw,oy+r*ch,.1f,cw-3,ch-3,col);} 
 float tx=ox+(saberX+1.8f)/3.6f*(cw*4-3), ty=oy+(1.15f-saberY)/2.3f*(ch*3-3); C2D_DrawRectSolid(tx-5,ty-5,.5f,10,10,C2D_Color32(55,230,255,255));
 textCenter(211,.38f,C2D_Color32(200,210,230,255),"SWIPE THE TOUCH SCREEN TO MOVE + CUT"); textCenter(226,.33f,C2D_Color32(100,115,145,255),"START PAUSE   B RETURN TO MENU");
}
static void renderPause2D(void){renderGame2D();C2D_DrawRectSolid(30,75,.8f,260,90,C2D_Color32(4,6,12,235));textCenter(94,.65f,C2D_Color32(255,255,255,255),"PAUSED");textCenter(124,.4f,C2D_Color32(150,215,235,255),"A RESUME   B MENU");}
static void renderResults2D(void){
 C2D_TextBufClear(textBuf);C2D_DrawRectSolid(0,0,0,320,240,BOTTOM_CLEAR);textCenter(22,.65f,C2D_Color32(255,255,255,255),health<=0?"FAILED":"LEVEL COMPLETE");
 int total=hits+misses;float acc=total?100.0f*hits/total:0;textCenter(65,.55f,C2D_Color32(60,220,255,255),"SCORE %d",score);textCenter(94,.43f,C2D_Color32(200,210,230,255),"HITS %d   MISSES %d",hits,misses);textCenter(119,.43f,C2D_Color32(200,210,230,255),"BEST COMBO %dx",bestCombo);textCenter(144,.43f,C2D_Color32(200,210,230,255),"ACCURACY %.1f%%",acc);textCenter(203,.38f,C2D_Color32(130,145,170,255),"A RETRY   B MENU");
}
static void topText(void){
 C2D_SceneTarget(topTarget); C2D_Prepare(); C2D_TextBufClear(textBuf);
 if(state==ST_MENU){C2D_Text t;C2D_TextParse(&t,textBuf,"ONE SABER // TRUE 3D");C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor|C2D_AlignCenter,200,205,.5f,.45f,.45f,C2D_Color32(175,225,245,255));}
 else if(state==ST_GAME||state==ST_PAUSE){C2D_Text t;char b[96];snprintf(b,sizeof(b),"%d   %dx",score,combo);C2D_TextParse(&t,textBuf,b);C2D_TextOptimize(&t);C2D_DrawText(&t,C2D_WithColor,12,10,.5f,.5f,.5f,C2D_Color32(240,245,255,255));}
 C2D_Flush();
}

static void render(void){
 C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
 C3D_RenderTargetClear(topTarget,C3D_CLEAR_ALL,TOP_CLEAR,0); C3D_FrameDrawOn(topTarget); if(state==ST_MENU||state==ST_RESULTS)drawLogo(); else drawArena(); topText();
 C2D_TargetClear(bottomTarget,BOTTOM_CLEAR); C2D_SceneBegin(bottomTarget); if(state==ST_MENU)renderMenu2D(); else if(state==ST_GAME)renderGame2D(); else if(state==ST_PAUSE)renderPause2D(); else renderResults2D();
 C3D_FrameEnd(0);
}

static bool initGraphics(void){
 gfxInitDefault(); if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE))return false; if(!C2D_Init(C2D_DEFAULT_MAX_OBJECTS))return false;
 topTarget=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8); C3D_RenderTargetSetOutput(topTarget,GFX_TOP,GFX_LEFT,DISPLAY_TRANSFER_FLAGS); bottomTarget=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
 dvlb=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size); shaderProgramInit(&program); shaderProgramSetVsh(&program,&dvlb->DVLE[0]); uProjection=shaderInstanceGetUniformLocation(program.vertexShader,"projection"); uModel=shaderInstanceGetUniformLocation(program.vertexShader,"modelView"); uTint=shaderInstanceGetUniformLocation(program.vertexShader,"tint");
 AttrInfo_Init(&attrInfo); AttrInfo_AddLoader(&attrInfo,0,GPU_FLOAT,3); vbo=linearAlloc(sizeof(cubeVerts)); memcpy(vbo,cubeVerts,sizeof(cubeVerts)); BufInfo_Init(&bufInfo); BufInfo_Add(&bufInfo,vbo,sizeof(Vertex),1,0x0); Mtx_PerspTilt(&projection,C3D_AngleFromDegrees(52),C3D_AspectRatioTop,.05f,40.0f,false); textBuf=C2D_TextBufNew(4096); return true;
}
static void shutdown(void){stopChannel(MUSIC_CH);stopChannel(UI_CH);if(audioOK)csndExit();if(musicBuf)linearFree(musicBuf);if(uiBuf)linearFree(uiBuf);C2D_TextBufDelete(textBuf);linearFree(vbo);shaderProgramFree(&program);DVLB_Free(dvlb);C2D_Fini();C3D_Fini();gfxExit();}

int main(void){
 if(!initGraphics())return 1; audioOK=R_SUCCEEDED(csndInit()); if(audioOK)buildUiSound();
 while(aptMainLoop()){
  hidScanInput(); u32 down=hidKeysDown(),held=hidKeysHeld();
  if(state==ST_MENU){
   if(down&KEY_START)break; if(down&KEY_DUP){selected=(selected+TRACK_COUNT-1)%TRACK_COUNT;playUiSound();} if(down&KEY_DDOWN){selected=(selected+1)%TRACK_COUNT;playUiSound();}
   if(down&KEY_L){difficulty=(difficulty+2)%3;playUiSound();} if(down&KEY_R){difficulty=(difficulty+1)%3;playUiSound();} if(down&KEY_X){previewTrack();} if(down&KEY_A){previewUntil=0;startGame();}
   if(previewUntil&&osGetTime()>previewUntil){stopChannel(MUSIC_CH);previewUntil=0;}
  } else if(state==ST_GAME){
   if(down&KEY_START){pausedAt=osGetTime();stopChannel(MUSIC_CH);state=ST_PAUSE;} else if(down&KEY_B){stopChannel(MUSIC_CH);state=ST_MENU;} else updateGame(held);
  } else if(state==ST_PAUSE){
   if(down&KEY_A||down&KEY_START){u64 now=osGetTime();gameStart+=now-pausedAt;startMusic(true);state=ST_GAME;} else if(down&KEY_B){state=ST_MENU;}
  } else if(state==ST_RESULTS){ if(down&KEY_A)startGame(); else if(down&KEY_B)state=ST_MENU; }
  render();
 }
 shutdown();return 0;
}
