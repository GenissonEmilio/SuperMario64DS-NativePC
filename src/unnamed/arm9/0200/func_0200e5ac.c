extern int func_0200e6d8(int);
extern int func_0200e5fc(int);
struct Player;
/* Player::func_ov002_020bd664(unsigned char *, int, int), the Kuppa-script
   player command dispatcher in ov002's Player TU. */
extern int _ZN6Player19func_ov002_020bd664EPhii(struct Player *player, unsigned char *cmd, int beginFrame, int endFrame);
int func_0200e5ac(void* c,void* r1,void* r2){
  int r=func_0200e6d8(*(unsigned char*)((char*)c+1));
  if(r==0){
    r=func_0200e5fc(*(unsigned char*)((char*)c+1));
    if(r==0) return 0;
  }
  return _ZN6Player19func_ov002_020bd664EPhii((struct Player*)r,(unsigned char*)c,(int)r1,(int)r2);
}
