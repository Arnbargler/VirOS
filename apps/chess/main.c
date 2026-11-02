/* VirOS CHESS: UEFI text console glyph compatibility laboratory. */
#include <efi.h>
#include <efilib.h>
static EFI_SYSTEM_TABLE *sys;
static SIMPLE_TEXT_OUTPUT_INTERFACE *out;
static VOID put(UINTN x,UINTN y,UINTN attr,const CHAR16 *s) {
 uefi_call_wrapper(out->SetAttribute,2,out,attr);
 uefi_call_wrapper(out->SetCursorPosition,3,out,x,y);
 uefi_call_wrapper(out->OutputString,2,out,(CHAR16*)s);
}
typedef struct { const CHAR16 *name; const CHAR16 *glyphs; } GROUP;
static const GROUP groups[]={
 {L"ASCII controls",L"ABC 123 # @ + * ?"},
 {L"UEFI box lines",L"\u2500 \u2502 \u250C \u2510 \u2514 \u2518 \u253C"},
 {L"UEFI double",L"\u2550 \u2551 \u2554 \u2557 \u255A \u255D \u256C"},
 {L"Block/shading",L"\u2588 \u2593 \u2592 \u2591 \u2580 \u2584"},
 {L"Arrows",L"\u2190 \u2191 \u2192 \u2193 \u2194"},
 {L"White chess",L"\u2654 \u2655 \u2656 \u2657 \u2658 \u2659"},
 {L"Black chess",L"\u265A \u265B \u265C \u265D \u265E \u265F"}
};
static VOID show_group(UINTN row,const GROUP *group) {
 CHAR16 buf[48];
 EFI_STATUS r=uefi_call_wrapper(out->TestString,2,out,(CHAR16*)group->glyphs);
 put(2,row,0x1F,group->name);
 put(23,row,0x1E,group->glyphs);
 SPrint(buf,sizeof(buf),L"%s",EFI_ERROR(r)?L"NO ":L"YES");
 put(64,row,EFI_ERROR(r)?0x1C:0x1A,buf);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *system) {
 UINTN i,cols=0,rows=0,mode=0,index;
 EFI_STATUS rc;EFI_INPUT_KEY key;
 InitializeLib(image,system);sys=system;out=sys->ConOut;
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
 put(2,1,0x1E,L"VirOS - CONSOLE CHARACTER TEST");
 put(2,3,0x1B,L"Category               Rendered characters                TestString");
 for(i=0;i<sizeof(groups)/sizeof(groups[0]);i++)show_group(5+i*2,&groups[i]);
 put(2,20,0x1F,L"YES means firmware accepted the string, NOT that glyphs appear.");
 put(2,21,0x1F,L"Compare each row against the ASCII control row.");
 put(2,23,0x1E,L"Press any key to return to VirOS.");
 index=0;
 uefi_call_wrapper(sys->BootServices->WaitForEvent,3,1,&sys->ConIn->WaitForKey,&index);
 uefi_call_wrapper(sys->ConIn->ReadKeyStroke,2,sys->ConIn,&key);
 uefi_call_wrapper(out->SetAttribute,2,out,0x07);
 uefi_call_wrapper(out->EnableCursor,2,out,TRUE);
 return EFI_SUCCESS;
}
