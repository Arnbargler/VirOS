/* VirOS BLOCKS - native UEFI 80x25 falling-block game. */
#include <efi.h>
#include <efilib.h>
#define BW 10
#define BH 20
#define SX 24
#define SY 2
#define LEFT_WALL (SX-1)
#define RIGHT_WALL (SX+BW*2)
static EFI_SYSTEM_TABLE *sys;
static SIMPLE_TEXT_OUTPUT_INTERFACE *con;
static UINT8 board[BH][BW];
static UINT8 frame[BH][BW];
static UINT8 previous[BH][BW];
static BOOLEAN first_frame=TRUE;
static UINTN previous_score=(UINTN)-1,previous_lines=(UINTN)-1,previous_level=(UINTN)-1;
static INTN previous_state=-1;
static UINTN score, lines, level, seed=0x12345;
static INTN px,py,piece,rotation;
static BOOLEAN over=FALSE,paused=FALSE;
static const UINT16 shapes[7][4]={
 {0x0F00,0x2222,0x00F0,0x4444}, /* I */
 {0x0660,0x0660,0x0660,0x0660}, /* O */
 {0x0E40,0x4C40,0x4E00,0x4640}, /* T */
 {0x06C0,0x8C40,0x06C0,0x8C40}, /* S */
 {0x0C60,0x4C80,0x0C60,0x4C80}, /* Z */
 {0x08E0,0x6440,0x0E20,0x44C0}, /* J */
 {0x02E0,0x4460,0x0E80,0xC440}  /* L */
};
/* Coloured solid glyphs over a consistent dark-blue playfield. */
static const UINTN colors[8]={0x10,0x1B,0x1E,0x1D,0x1C,0x1A,0x1F,0x19};
static VOID at(UINTN x,UINTN y,UINTN c,CHAR16 *t) {
 uefi_call_wrapper(con->SetAttribute,2,con,c);
 uefi_call_wrapper(con->SetCursorPosition,3,con,x,y);
 uefi_call_wrapper(con->OutputString,2,con,t);
}
static BOOLEAN block(INTN t,INTN r,INTN x,INTN y) {
 return (shapes[t][r] & ((UINT16)0x8000 >> (y*4+x)))!=0;
}
static BOOLEAN fits(INTN x,INTN y,INTN r) {
 INTN a,b,xx,yy;
 for(b=0;b<4;b++)for(a=0;a<4;a++)if(block(piece,r,a,b)){
  xx=x+a;yy=y+b;
  if(xx<0||xx>=BW||yy>=BH)return FALSE;
  if(yy>=0&&board[yy][xx])return FALSE;
 }
 return TRUE;
}
static UINTN rand7(VOID) {
 seed=seed*1664525U+1013904223U;
 return (seed>>8)%7;
}
static VOID spawn(VOID) {
 piece=(INTN)rand7(); rotation=0; px=3;py=-1;
 if(!fits(px,py,rotation))over=TRUE;
}
static VOID reset(VOID) {
 UINTN x,y;
 for(y=0;y<BH;y++)for(x=0;x<BW;x++)board[y][x]=0;
 score=0;lines=0;level=1;over=FALSE;paused=FALSE;first_frame=TRUE;spawn();
}
static VOID lock_piece(VOID) {
 INTN a,b,xx,yy;INTN y,x,k;
 UINTN cleared=0;
 for(b=0;b<4;b++)for(a=0;a<4;a++)if(block(piece,rotation,a,b)){
  xx=px+a;yy=py+b;
  if(yy<0){over=TRUE;continue;}
  if(xx>=0&&xx<BW&&yy<BH)board[yy][xx]=(UINT8)(piece+1);
 }
 if(over)return;
 for(y=BH-1;y>=0;y--){
  BOOLEAN full=TRUE;
  for(x=0;x<BW;x++)if(!board[y][x])full=FALSE;
  if(!full)continue;
  cleared++;
  for(k=y;k>0;k--)for(x=0;x<BW;x++)board[k][x]=board[k-1][x];
  for(x=0;x<BW;x++)board[0][x]=0;
  y++;
 }
 if(cleared){
  static const UINTN points[5]={0,100,300,500,800};
  score+=points[cleared]*level;
  lines+=cleared; level=1+lines/10;
 }
 spawn();
}
static VOID down(VOID){
 if(fits(px,py+1,rotation))py++;
 else lock_piece();
}
static VOID render(VOID) {
 UINTN x,y,c;INTN a,b,xx,yy;
 CHAR16 number[48];
 for(y=0;y<BH;y++)for(x=0;x<BW;x++)frame[y][x]=board[y][x];
 if(!over)for(b=0;b<4;b++)for(a=0;a<4;a++)if(block(piece,rotation,a,b)){
  xx=px+a;yy=py+b;
  if(xx>=0&&xx<BW&&yy>=0&&yy<BH)frame[yy][xx]=(UINT8)(piece+1);
 }
 for(y=0;y<BH;y++)for(x=0;x<BW;x++) {
  c=frame[y][x];
  if(first_frame || c!=previous[y][x]) {
   at(SX+x*2,SY+y,colors[c],c?L"\u2588\u2588":L"  ");
   previous[y][x]=(UINT8)c;
  }
 }
 if(first_frame||score!=previous_score){
  SPrint(number,sizeof(number),L"Score  %-10d",score);at(52,5,0x0F,number);
  previous_score=score;
 }
 if(first_frame||lines!=previous_lines){
  SPrint(number,sizeof(number),L"Lines  %-10d",lines);at(52,7,0x0F,number);
  previous_lines=lines;
 }
 if(first_frame||level!=previous_level){
  SPrint(number,sizeof(number),L"Level  %-10d",level);at(52,9,0x0F,number);
  previous_level=level;
 }
 if(first_frame){
  at(52,12,0x0B,L"Left/Right Move");
  at(52,13,0x0B,L"Up         Rotate");
  at(52,14,0x0B,L"Down       Soft drop");
  at(52,15,0x0B,L"Space      Hard drop");
  at(52,16,0x0B,L"P          Pause");
  at(52,17,0x0B,L"R          Restart");
  at(52,18,0x0B,L"ESC        Launcher");
 }
 {
  INTN state=over?2:(paused?1:0);
  if(first_frame||state!=previous_state){
   at(52,20,over?0x0C:0x0E,over?L"GAME OVER       ":
     (paused?L"PAUSED          ":L"                "));
   previous_state=state;
  }
 }
 first_frame=FALSE;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *table) {
 EFI_STATUS result;EFI_EVENT timer=NULL,ev[2];EFI_INPUT_KEY key;
 EFI_TIME now;UINTN idx,i,cols,rows,mode=0,elapsed=0;
 InitializeLib(image,table);sys=table;con=sys->ConOut;
 uefi_call_wrapper(sys->BootServices->SetWatchdogTimer,4,0,0,0,NULL);
 for(i=0;i<(UINTN)con->Mode->MaxMode;i++){
  result=uefi_call_wrapper(con->QueryMode,4,con,i,&cols,&rows);
  if(!EFI_ERROR(result)&&cols==80&&rows==25){mode=i;break;}
 }
 if(i==(UINTN)con->Mode->MaxMode)return EFI_UNSUPPORTED;
 result=uefi_call_wrapper(con->SetMode,2,con,mode);
 if(EFI_ERROR(result))return result;
 uefi_call_wrapper(con->EnableCursor,2,con,FALSE);
 uefi_call_wrapper(con->SetAttribute,2,con,0x07);
 uefi_call_wrapper(con->ClearScreen,1,con);
 at(24,0,0x0E,L"VirOS BLOCKS");
 at(LEFT_WALL,1,0x0F,L"\u2554\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2557");
 at(LEFT_WALL,22,0x0F,L"\u255A\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u2550\u255D");
 for(i=2;i<22;i++){at(LEFT_WALL,i,0x0F,L"\u2551");at(RIGHT_WALL,i,0x0F,L"\u2551");}
 if(!EFI_ERROR(uefi_call_wrapper(sys->RuntimeServices->GetTime,2,&now,NULL)))
  seed=((UINTN)now.Second<<24)^((UINTN)now.Nanosecond)^((UINTN)now.Minute<<8)^now.Day;
 reset();
 result=uefi_call_wrapper(sys->BootServices->CreateEvent,5,EVT_TIMER,0,NULL,NULL,&timer);
 if(EFI_ERROR(result))return result;
 result=uefi_call_wrapper(sys->BootServices->SetTimer,3,timer,TimerPeriodic,1000000); /* 100ms */
 if(EFI_ERROR(result)){uefi_call_wrapper(sys->BootServices->CloseEvent,2,timer);return result;}
 ev[0]=sys->ConIn->WaitForKey;ev[1]=timer;
 render();
 for(;;){
  idx=0;
  result=uefi_call_wrapper(sys->BootServices->WaitForEvent,3,2,ev,&idx);
  if(EFI_ERROR(result))break;
  if(idx==1) {
   if(!paused&&!over){elapsed++;if(elapsed >= (level<10? 8-(level-1)/2:2)){elapsed=0;down();render();}}
   continue;
  }
  result=uefi_call_wrapper(sys->ConIn->ReadKeyStroke,2,sys->ConIn,&key);
  if(EFI_ERROR(result))continue;
  if(key.ScanCode==SCAN_ESC)break;
  if(key.UnicodeChar==L'r'||key.UnicodeChar==L'R'){reset();elapsed=0;render();continue;}
  if(key.UnicodeChar==L'p'||key.UnicodeChar==L'P'){if(!over)paused=!paused;render();continue;}
  if(over||paused)continue;
  if(key.ScanCode==SCAN_LEFT&&fits(px-1,py,rotation))px--;
  else if(key.ScanCode==SCAN_RIGHT&&fits(px+1,py,rotation))px++;
  else if(key.ScanCode==SCAN_UP){
   INTN r=(rotation+1)%4;
   if(fits(px,py,r))rotation=r;
   else if(fits(px-1,py,r)){px--;rotation=r;}
   else if(fits(px+1,py,r)){px++;rotation=r;}
  }
  else if(key.ScanCode==SCAN_DOWN){down();score++;}
  else if(key.UnicodeChar==L' '){
   UINTN distance=0;
   while(fits(px,py+1,rotation)){py++;distance++;}
   score+=2*distance;lock_piece();elapsed=0;
  }
  render();
 }
 uefi_call_wrapper(sys->BootServices->CloseEvent,2,timer);
 uefi_call_wrapper(con->SetAttribute,2,con,0x07);
 uefi_call_wrapper(con->EnableCursor,2,con,TRUE);
 return EFI_SUCCESS;
}
