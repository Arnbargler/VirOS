/* VirOS CHESS Unicode console compatibility test. GNU-EFI x86_64. */
#include <efi.h>
#include <efilib.h>
static EFI_SYSTEM_TABLE *sys;
static SIMPLE_TEXT_OUTPUT_INTERFACE *out;
static const CHAR16 *white[6]={L"\u2654",L"\u2655",L"\u2656",L"\u2657",L"\u2658",L"\u2659"};
static const CHAR16 *black[6]={L"\u265A",L"\u265B",L"\u265C",L"\u265D",L"\u265E",L"\u265F"};
static const CHAR16 *names[6]={L"King",L"Queen",L"Rook",L"Bishop",L"Knight",L"Pawn"};
static const CHAR16 *ascii[6]={L"K",L"Q",L"R",L"B",L"N",L"P"};
static VOID put(UINTN x,UINTN y,UINTN attr,const CHAR16 *s) {
 uefi_call_wrapper(out->SetAttribute,2,out,attr);
 uefi_call_wrapper(out->SetCursorPosition,3,out,x,y);
 uefi_call_wrapper(out->OutputString,2,out,(CHAR16*)s);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system) {
 UINTN i,cols=0,rows=0,mode=0,index;
 EFI_STATUS rc;
 EFI_INPUT_KEY key;
 CHAR16 buf[80];
 InitializeLib(image,system);
 sys=system;out=sys->ConOut;
 uefi_call_wrapper(sys->BootServices->SetWatchdogTimer,4,0,0,0,NULL);
 for(i=0;i<(UINTN)out->Mode->MaxMode;i++){
  rc=uefi_call_wrapper(out->QueryMode,4,out,i,&cols,&rows);
  if(!EFI_ERROR(rc)&&cols==80&&rows==25){mode=i;break;}
 }
 if(i==(UINTN)out->Mode->MaxMode)return EFI_UNSUPPORTED;
 rc=uefi_call_wrapper(out->SetMode,2,out,mode);
 if(EFI_ERROR(rc))return rc;
 uefi_call_wrapper(out->EnableCursor,2,out,FALSE);
 uefi_call_wrapper(out->SetAttribute,2,out,0x1F);
 uefi_call_wrapper(out->ClearScreen,1,out);
 put(2,1,0x1E,L"VirOS CHESS - UNICODE DISPLAY TEST");
 put(2,3,0x1B,L"Firmware glyph support: Unicode vs ASCII");
 put(2,5,0x1F,L"Piece     White     Black     ASCII     TestString status");
 for(i=0;i<6;i++){
  EFI_STATUS w,b;
  UINTN y=7+i*2;
  put(2,y,0x1F,names[i]);
  /* TestString reports whether the firmware console claims it can print
     the character. Some firmware still displays a replacement glyph. */
  w=uefi_call_wrapper(out->TestString,2,out,(CHAR16*)white[i]);
  b=uefi_call_wrapper(out->TestString,2,out,(CHAR16*)black[i]);
  put(13,y,0x1E,white[i]);
  put(23,y,0x1A,black[i]);
  put(34,y,0x1F,ascii[i]);
  SPrint(buf,sizeof(buf),L"W:%s B:%s",
    EFI_ERROR(w)?L"NO ":L"YES",EFI_ERROR(b)?L"NO ":L"YES");
  put(44,y,EFI_ERROR(w)||EFI_ERROR(b)?0x1C:0x1A,buf);
 }
 put(2,20,0x1F,L"Does each symbol look like a real chess piece?");
 put(2,21,0x1F,L"TestString YES is not proof of a correct glyph.");
 put(2,23,0x1E,L"Press any key to return to the VirOS launcher.");
 index=0;
 uefi_call_wrapper(sys->BootServices->WaitForEvent,3,1,&sys->ConIn->WaitForKey,&index);
 uefi_call_wrapper(sys->ConIn->ReadKeyStroke,2,sys->ConIn,&key);
 uefi_call_wrapper(out->SetAttribute,2,out,0x07);
 uefi_call_wrapper(out->EnableCursor,2,out,TRUE);
 return EFI_SUCCESS;
}
