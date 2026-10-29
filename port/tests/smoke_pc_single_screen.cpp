// ROM-free checks of the actual map action and button compositor. MIT.
#include "hal/menu_bigmap.h"
#include "hal/pc_map_ui.h"
#include <cstdio>
#include <vector>
#include <cstring>

static int scene=3,level=6,can_pause=1,stacked=0,checks,failed;
extern "C" {
unsigned char data_020a0e40=0,data_0209f2c4=0,data_0209f20c=0,data_0209f294=0,data_0209d660=0,data_0209f2d8=0;
int data_020a0e58[16]={},data_020a0e5a[16]={},data_0209d4b0=0;
signed char data_02092110=-1;
char data_0209f4a0[0x18*16]={};
int host_setting_one_screen_menus(){return 1;}
int port_scene_live_id(){return scene;}
int port_level_id(){return level;}
int hal_sub_screen_stacked(){return stacked;}
}
class dScStage_c {public:static int IsPauseDisabled();static int CanPause();};
int dScStage_c::IsPauseDisabled(){return 0;}
int dScStage_c::CanPause(){return can_pause;}
static void check(const char *name,int ok){++checks;if(!ok){++failed;std::printf("FAIL %s\n",name);}}
int main()
{
    menu_bigmap_pad_seam();check("map starts closed and available",!menu_bigmap_up()&&menu_bigmap_available());
    menu_bigmap_request_toggle();menu_bigmap_pad_seam();check("native M intent opens map",menu_bigmap_up());
    check("host action does not send DS Select",data_020a0e58[0]==0&&data_020a0e5a[0]==0);
    for(int i=0;i<20;++i)menu_bigmap_pad_seam();check("no auto-repeat while held",menu_bigmap_up());
    menu_bigmap_request_toggle();menu_bigmap_pad_seam();check("second intent closes",!menu_bigmap_up());
    for(auto wh: {PcMapRect{0,0,512,384},PcMapRect{0,0,1280,720},PcMapRect{0,0,1024,768},PcMapRect{0,0,1920,800},PcMapRect{0,0,256,192}}){
        const int w=wh.w,h=wh.h;auto r=pc_map_button_rect(w,h);
        check("button stays on one canvas",r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=h);
        check("button center hit",pc_map_button_hit(r.x+r.w/2,r.y+r.h/2,w,h));
        check("exclusive right edge",!pc_map_button_hit(r.x+r.w,r.y,w,h));
        check("off-canvas click rejected",!pc_map_button_hit(-1,-1,w,h));
        std::vector<uint32_t> dst((w+9)*h+16,0x12345678u);
        pc_map_button_draw(dst.data(),w,h,w+9,0);
        check("button renders its pixels",dst[r.y*(w+9)+r.x]==0xffffd166u);
        int padding=1;for(int y=0;y<h;++y)for(int x=w;x<w+9;++x)padding&=dst[y*(w+9)+x]==0x12345678u;
        check("compositor respects stride",padding&&dst.back()==0x12345678u);
    }
    auto r=pc_map_button_rect(512,384);
    check("mouse button routes native action",menu_bigmap_click(r.x+2,r.y+2,512,384));menu_bigmap_pad_seam();check("click opens",menu_bigmap_up());
    ++level;menu_bigmap_pad_seam();check("level change closes old map",!menu_bigmap_up());
    can_pause=0;menu_bigmap_request_toggle();menu_bigmap_pad_seam();check("cutscene refuses map",!menu_bigmap_up()&&!menu_bigmap_available());
    can_pause=1;scene=1;menu_bigmap_request_toggle();menu_bigmap_pad_seam();check("title refuses map",!menu_bigmap_up()&&!menu_bigmap_available());
    scene=3;menu_bigmap_pad_seam();check("title request does not leak to level",!menu_bigmap_up());
    stacked=1;menu_bigmap_request_toggle();menu_bigmap_pad_seam();check("minigame does not lose its input surface",!menu_bigmap_up());stacked=0;
    data_020a0e58[0]=0x00040004;data_020a0e5a[0]=4;menu_bigmap_pad_seam();
    check("Select fallback opens map",menu_bigmap_up());
    check("Select not passed to controller mode menu",!(data_020a0e58[0]&0x00040004)&&!(data_020a0e5a[0]&4));
    data_020a0e58[0]=0x10;menu_bigmap_pad_seam();check("walking dismisses map",!menu_bigmap_up());
    std::printf("PC single-screen: %d checks, %d failures\n",checks,failed);
    return failed?1:0;
}
