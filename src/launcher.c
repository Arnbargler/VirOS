/*
 * VirOS native UEFI text launcher (GNU-EFI, x86_64).
 * No GOP access, no framebuffer code, no ExitBootServices.
 */
#include <efi.h>
#include <efilib.h>

#define WIDTH 80
#define HEIGHT 25
#define MAX_APPS 64
#define VISIBLE 12

typedef struct {
    CHAR16 name[48];
    CHAR16 path[120];
    BOOLEAN installed;
} APP;

static APP apps[MAX_APPS];
static UINTN count = 0, selected = 0, top = 0, palette = 0;
static EFI_HANDLE boot_device;
static EFI_HANDLE launcher_image;
static EFI_SYSTEM_TABLE *st;
static SIMPLE_TEXT_OUTPUT_INTERFACE *out;
static CHAR16 status[80] = L"";
static const CHAR16 *placeholder[] = {
    L"Universal Reader", L"Universal Editor", L"File Manager",
    L"Calculator", L"Calendar", L"Clock & Timer",
    L"System Information", L"Terminal", L"Hex Viewer",
    L"Chess", L"Music Player", L"App 12",
    L"App 13", L"App 14", L"App 15", L"App 16"
};
static const UINTN placeholder_count = sizeof(placeholder)/sizeof(placeholder[0]);

/* UEFI colour values: low nibble foreground, high nibble background. */
typedef struct { UINTN base, header, border, highlight, dim, accent, footer; } THEME;
static const THEME themes[] = {
    {0x1F, 0x1E, 0x19, 0x70, 0x17, 0x1B, 0x1F}, /* Midnight */
    {0x0F, 0x0E, 0x08, 0x70, 0x07, 0x0B, 0x0F}, /* Classic */
    {0x2F, 0x2E, 0x28, 0x70, 0x27, 0x2E, 0x2F}, /* Forest */
    {0x4F, 0x4E, 0x48, 0x70, 0x47, 0x4D, 0x4F}  /* Burgundy */
};
static const CHAR16 *theme_names[] = {L"MIDNIGHT",L"CLASSIC",L"FOREST",L"BURGUNDY"};

static VOID color(UINTN attr) { uefi_call_wrapper(out->SetAttribute, 2, out, attr); }
static VOID pos(UINTN x, UINTN y) { uefi_call_wrapper(out->SetCursorPosition, 3, out, x, y); }
static VOID write_at(UINTN x, UINTN y, UINTN attr, const CHAR16 *value) {
    color(attr); pos(x,y); uefi_call_wrapper(out->OutputString, 2, out, (CHAR16 *)value);
}
static VOID fill_line(UINTN y, UINTN attr) {
    CHAR16 line[WIDTH+1]; UINTN i;
    for (i=0;i<WIDTH;i++) line[i]=L' ';
    line[WIDTH]=0;
    write_at(0,y,attr,line);
}
static VOID label_at(UINTN x, UINTN y, UINTN attr, const CHAR16 *value, UINTN width) {
    CHAR16 buf[WIDTH+1]; UINTN i=0;
    if (width > WIDTH-x) width=WIDTH-x;
    while (i<width && value[i]) { buf[i]=value[i]; i++; }
    while (i<width) buf[i++]=L' ';
    buf[i]=0;
    write_at(x,y,attr,buf);
}
static VOID set_status(const CHAR16 *message) {
    StrnCpy(status,(CHAR16*)message,79);
    status[79]=0;
}
static VOID add_app(const CHAR16 *name, const CHAR16 *path, BOOLEAN installed) {
    if (count>=MAX_APPS) return;
    StrnCpy(apps[count].name,(CHAR16*)name,47);
    apps[count].name[47]=0;
    StrnCpy(apps[count].path,(CHAR16*)path,119);
    apps[count].path[119]=0;
    apps[count].installed=installed;
    count++;
}
static BOOLEAN is_efi(const CHAR16 *s) {
    UINTN len=StrLen((CHAR16*)s);
    return len>=4 && (s[len-4]==L'.') &&
        (s[len-3]==L'e'||s[len-3]==L'E') &&
        (s[len-2]==L'f'||s[len-2]==L'F') &&
        (s[len-1]==L'i'||s[len-1]==L'I');
}
static VOID discover(VOID) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;
    EFI_FILE_HANDLE root=NULL, dir=NULL;
    EFI_STATUS result;
    UINTN i;
    count=0; selected=0; top=0;
    for(i=0;i<placeholder_count;i++) add_app(placeholder[i],L"",FALSE);
    if (!boot_device) { set_status(L"Boot device unavailable."); return; }
    result=uefi_call_wrapper(st->BootServices->HandleProtocol,3,
        boot_device,&FileSystemProtocol,(VOID**)&fs);
    if(EFI_ERROR(result)) { set_status(L"No FAT filesystem found on boot device."); return; }
    result=uefi_call_wrapper(fs->OpenVolume,2,fs,&root);
    if(EFI_ERROR(result)) { set_status(L"Cannot open boot volume."); return; }
    result=uefi_call_wrapper(root->Open,5,root,&dir,L"\\APPS",EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(result)) {
        set_status(L"Create \\APPS on USB to add executable applications.");
        uefi_call_wrapper(root->Close,1,root);
        return;
    }
    for(;;) {
        UINT8 data[2048];
        UINTN size=sizeof(data);
        EFI_FILE_INFO *info=(EFI_FILE_INFO*)data;
        CHAR16 path[120];
        result=uefi_call_wrapper(dir->Read,3,dir,&size,data);
        if(EFI_ERROR(result)||size==0||count>=MAX_APPS) break;
        if((info->Attribute & EFI_FILE_DIRECTORY) || !is_efi(info->FileName)) continue;
        if(StrLen(info->FileName)>105) continue;
        StrCpy(path,L"\\APPS\\");
        StrCat(path,info->FileName);
        add_app(info->FileName,path,TRUE);
    }
    uefi_call_wrapper(dir->Close,1,dir);
    uefi_call_wrapper(root->Close,1,root);
    set_status(L"");
}
/* Repaint one menu row, never the whole screen for cursor movement. */
static VOID draw_app_row(UINTN idx) {
    const THEME *t=&themes[palette];
    UINTN y;
    CHAR16 tmp[80];
    if(idx<top || idx>=top+VISIBLE) return;
    y=6+idx-top;
    if(idx>=count) { label_at(2,y,t->base,L"",47); return; }
    SPrint(tmp,sizeof(tmp),L" %c %02d  %s",
        idx==selected?L'>':L' ',idx+1,apps[idx].name);
    label_at(2,y,idx==selected?t->highlight:
        (apps[idx].installed?t->accent:t->base),tmp,47);
    write_at(43,y,idx==selected?t->highlight:t->dim,
        apps[idx].installed?L" EFI ":L" --- ");
}

static const CHAR16 *months[] = {
 L"January",L"February",L"March",L"April",L"May",L"June",
 L"July",L"August",L"September",L"October",L"November",L"December"
};
static BOOLEAN leap(UINTN y) { return y%4==0 && (y%100!=0 || y%400==0); }
static UINTN days_in_month(UINTN y,UINTN m) {
 static const UINTN d[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 return m>=1&&m<=12 ? d[m-1]+(m==2&&leap(y)?1:0) : 0;
}
static UINTN weekday(UINTN y,UINTN m,UINTN d) {
 static const UINTN offsets[12]={0,3,2,5,0,3,5,1,4,6,2,4};
 if(m<3)y--;
 return (y+y/4-y/100+y/400+offsets[m-1]+d)%7;
}
static BOOLEAN rtc(EFI_TIME *time) {
 EFI_STATUS result=uefi_call_wrapper(st->RuntimeServices->GetTime,2,time,NULL);
 return !EFI_ERROR(result)&&time->Year>=1900&&time->Month>=1&&time->Month<=12
  &&time->Day>=1&&time->Day<=days_in_month(time->Year,time->Month)
  &&time->Hour<24&&time->Minute<60&&time->Second<60;
}
static VOID clock_only(VOID) {
 EFI_TIME now; CHAR16 buf[32];
 if(!rtc(&now))return;
 SPrint(buf,sizeof(buf),L"%02d:%02d:%02d",now.Hour,now.Minute,now.Second);
 label_at(55,5,themes[palette].accent,buf,23);
}
static VOID calendar(VOID) {
 const THEME *t=&themes[palette];
 EFI_TIME now;
 CHAR16 buf[64],day_buf[8];
 UINTN y,first,day,p,x;
 for(y=5;y<=18;y++)label_at(53,y,t->base,L"",26);
 if(!rtc(&now)) {write_at(54,7,t->accent,L"RTC unavailable");return;}
 clock_only();
 SPrint(buf,sizeof(buf),L"%s %d",months[now.Month-1],now.Year);
 label_at(53,7,t->header,buf,26);
 write_at(54,9,t->dim,L"Su Mo Tu We Th Fr Sa");
 first=weekday(now.Year,now.Month,1);
 for(day=1;day<=days_in_month(now.Year,now.Month);day++) {
  p=first+day-1; x=54+(p%7)*3; y=10+p/7;
  SPrint(day_buf,sizeof(day_buf),L"%2d",day);
  label_at(x,y,day==now.Day?t->highlight:t->base,day_buf,2);
 }
 write_at(54,17,t->dim,L"UEFI firmware clock");
}
static VOID draw(VOID) {
    const THEME *t=&themes[palette];
    UINTN y,i;
    uefi_call_wrapper(out->EnableCursor,2,out,FALSE);
    for(y=0;y<HEIGHT;y++) fill_line(y,t->base);
    fill_line(0,t->header);
    write_at(2,0,t->header,L"VirOS  /  UNIVERSAL EFI LAUNCHER");
    write_at(63,0,t->header,L"v0.1  80x25");
    fill_line(2,t->border);
    write_at(2,2,t->border,L" APPLICATIONS ");
    write_at(53,2,t->border,L" CLOCK / CALENDAR ");


    for(i=0;i<VISIBLE;i++) draw_app_row(top+i);
    calendar();
    fill_line(20,t->border);
    label_at(2,21,t->base,status,76);
    fill_line(23,t->footer);
    write_at(2,23,t->footer,L"F1 Help   F2 Theme   F3 Refresh   ENTER Open   Up/Down Move");

}
static VOID help(VOID) {
    EFI_INPUT_KEY key;
    UINTN index;
    const THEME *t=&themes[palette];
    fill_line(7,t->header); fill_line(8,t->header); fill_line(9,t->header);
    fill_line(10,t->header);fill_line(11,t->header);fill_line(12,t->header);
    fill_line(13,t->header);fill_line(14,t->header);fill_line(15,t->header);
    write_at(5,7,t->header,L" VIR OS HELP - built-in firmware text features");
    write_at(5,9,t->header,L" SetMode requests 80x25, colours use SetAttribute.");
    write_at(5,10,t->header,L" Cursor movement uses SetCursorPosition.");
    write_at(5,11,t->header,L" EFI apps load from \\APPS on the boot USB key.");
    write_at(5,12,t->header,L" Placeholder apps are intentionally not executable.");
    write_at(5,14,t->header,L" Press any key to close this panel.");
    uefi_call_wrapper(st->BootServices->WaitForEvent,3,1,&st->ConIn->WaitForKey,&index);
    uefi_call_wrapper(st->ConIn->ReadKeyStroke,2,st->ConIn,&key);
}
static EFI_STATUS launch(APP *app) {
    EFI_DEVICE_PATH *dp;
    EFI_HANDLE image=NULL;
    EFI_STATUS result;
    if(!app->installed) {set_status(L"Placeholder: application not installed yet.");return EFI_NOT_FOUND;}
    dp=FileDevicePath(boot_device,app->path);
    if(!dp) {set_status(L"Could not construct app device path.");return EFI_OUT_OF_RESOURCES;}
    result=uefi_call_wrapper(st->BootServices->LoadImage,6,FALSE,
        launcher_image,dp,NULL,0,&image);
    if(!EFI_ERROR(result))
        result=uefi_call_wrapper(st->BootServices->StartImage,3,image,NULL,NULL);
    if(image) uefi_call_wrapper(st->BootServices->UnloadImage,1,image);
    FreePool(dp);
    if(EFI_ERROR(result)) set_status(L"Application failed to load/start. Check architecture and file.");
    else set_status(L"Application returned to VirOS.");
    /* Child apps may have changed video mode and cursor state. */
    return result;
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    EFI_LOADED_IMAGE *loaded=NULL;
    EFI_INPUT_KEY key;
    EFI_STATUS result;
    EFI_EVENT timer=NULL,events[2];
    EFI_TIME last_time;
    BOOLEAN last_valid=FALSE;
    UINTN i,mode=0,cols=0,rows=0,idx;
    InitializeLib(image_handle,system_table);
    st=system_table; out=st->ConOut;
    launcher_image=image_handle;
    /* Explicitly select an available 80x25 mode. Never assume mode 0. */
    for(i=0;i<(UINTN)out->Mode->MaxMode;i++) {
        result=uefi_call_wrapper(out->QueryMode,4,out,i,&cols,&rows);
        if(!EFI_ERROR(result)&&cols==WIDTH&&rows==HEIGHT) {mode=i;break;}
    }
    if(i==(UINTN)out->Mode->MaxMode) {
        Print(L"VirOS requires an 80x25 UEFI console mode.\r\n");
        return EFI_UNSUPPORTED;
    }
    result=uefi_call_wrapper(out->SetMode,2,out,mode);
    if(EFI_ERROR(result)) return result;
    uefi_call_wrapper(st->BootServices->HandleProtocol,3,
        image_handle,&LoadedImageProtocol,(VOID**)&loaded);
    if(loaded) boot_device=loaded->DeviceHandle;
    /* The firmware watchdog can reset long-running UEFI applications. */
    uefi_call_wrapper(st->BootServices->SetWatchdogTimer,4,0,0,0,NULL);
    discover();
    draw();
    events[0]=st->ConIn->WaitForKey;
    result=uefi_call_wrapper(st->BootServices->CreateEvent,5,EVT_TIMER,0,NULL,NULL,&timer);
    if(!EFI_ERROR(result)) {
        uefi_call_wrapper(st->BootServices->SetTimer,3,timer,TimerPeriodic,10000000);
        events[1]=timer;
    } else timer=NULL;
    for(;;) {
        UINTN old_selected=selected, old_top=top;
        BOOLEAN redraw=FALSE, status_changed=FALSE;
        idx=0;
        uefi_call_wrapper(st->BootServices->WaitForEvent,3,timer?2:1,events,&idx);
        if(timer && idx==1) {
            EFI_TIME now;
            if(rtc(&now)) {
                if(!last_valid || now.Year!=last_time.Year ||
                   now.Month!=last_time.Month || now.Day!=last_time.Day)
                    calendar();
                else if(now.Hour!=last_time.Hour || now.Minute!=last_time.Minute ||
                        now.Second!=last_time.Second) clock_only();
                last_time=now; last_valid=TRUE;
            }
            continue;
        }
        result=uefi_call_wrapper(st->ConIn->ReadKeyStroke,2,st->ConIn,&key);
        if(EFI_ERROR(result)) continue;
        if(key.ScanCode==SCAN_UP && selected>0) selected--;
        else if(key.ScanCode==SCAN_DOWN && selected+1<count) selected++;
        else if(key.ScanCode==SCAN_PAGE_UP) selected=selected>VISIBLE?selected-VISIBLE:0;
        else if(key.ScanCode==SCAN_PAGE_DOWN) selected+VISIBLE<count? (selected+=VISIBLE):(selected=count-1);
        else if(key.ScanCode==SCAN_HOME) selected=0;
        else if(key.ScanCode==SCAN_END) selected=count-1;
        else if(key.ScanCode==SCAN_F1) { help(); redraw=TRUE; }
        else if(key.ScanCode==SCAN_F2) {
            palette=(palette+1)%(sizeof(themes)/sizeof(themes[0]));
            redraw=TRUE;
        }
        else if(key.ScanCode==SCAN_F3) { discover(); redraw=TRUE; }
        else if(key.UnicodeChar==CHAR_CARRIAGE_RETURN) {
            if(selected<count) {
                BOOLEAN real_app=apps[selected].installed;
                launch(&apps[selected]);
                if(real_app) {
                    /* A child might change console mode: restore requested geometry. */
                    uefi_call_wrapper(out->SetMode,2,out,mode);
                    redraw=TRUE;
                } else status_changed=TRUE;
            }
        } else if(key.ScanCode==SCAN_ESC) {
            set_status(L"Screen refreshed.");
            redraw=TRUE;
        }
        if(selected<top) top=selected;
        if(selected>=top+VISIBLE) top=selected-VISIBLE+1;
        if(redraw) draw();
        else if(top!=old_top) {
            /* Only redraw the application list on scrolling. */
            for(i=0;i<VISIBLE;i++) draw_app_row(top+i);
            if(status_changed) label_at(2,21,themes[palette].base,status,76);
        } else {
            if(selected!=old_selected) {
                draw_app_row(old_selected);
                draw_app_row(selected);
            }
            if(status_changed) label_at(2,21,themes[palette].base,status,76);
        }
    }
}
