#include "connection.hpp"
#include "render.hpp"
#include <shellapi.h>
#include <wtsapi32.h>
#include <thread>
#include <deque>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <memory>

constexpr UINT TrayMessage=WM_APP+1,FetchMessage=WM_APP+2,RevealMessage=WM_APP+3;
constexpr int LogicalW=WidgetLayout::LogicalWidth,LogicalH=WidgetLayout::LogicalHeight;
enum {ShowPet=100,Refresh,Summary,Quiet,Topmost,ExitApp,ExpressionSettings};
HWND windowHandle=nullptr;NOTIFYICONDATAW tray{};UINT taskbarCreated=0;
std::atomic_bool stopping=false,busy=false;std::thread worker;
Snapshot snapshot;AlertState alerts;bool stale=true,quiet=false,topmost=true,locked=false,demo=false,prototype=false;
bool savedVisible=true;int savedX=INT_MIN,savedY=INT_MIN,dpi=96;unsigned long long frame=0;std::wstring status=L"Connecting to Codex...";
ExpressionThresholds expressionThresholds;std::unique_ptr<Renderer> renderer;ULONGLONG resetUntil=0;bool exportMode=false;
fs::path dataDir;std::deque<Notice> notices;ULONGLONG nextNotice=0;SIZE_T measuredPeak=0;std::string lastError;
long long lastFetchAttempt=0;HICON nibbleIcon=nullptr;

void logEvent(const std::string& text){try{auto path=dataDir/L"events.log";if(fs::exists(path)&&fs::file_size(path)>65536){std::ofstream clear(path,std::ios::trunc);}std::ofstream f(path,std::ios::app);f<<utcNow()<<" "<<text<<"\n";}catch(...) {}}
void saveState(){if(demo||prototype)return;try{
    RECT r{};GetWindowRect(windowHandle,&r);
    Json j={{"version",1},{"x",r.left},{"y",r.top},{"quiet",quiet},{"topmost",topmost},{"visible",IsWindowVisible(windowHandle)!=FALSE},{"snapshot",encode(snapshot)},{"alerts",alerts.json()},{"expressionThresholds",expressionThresholds.json()}};
    auto temp=dataDir/L"state.tmp",dest=dataDir/L"state.json";{std::ofstream f(temp);if(!f)throw std::runtime_error("save");f<<j.dump(2);if(!f)throw std::runtime_error("save");}
    if(!MoveFileExW(temp.c_str(),dest.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("save");
}catch(...){status=L"Settings could not be saved";logEvent("Settings save failed");}}
void loadState(){try{
    std::ifstream f(dataDir/L"state.json");if(!f)return;auto j=Json::parse(f);if(j.value("version",0)!=1)return;
    snapshot=decode(j.at("snapshot"));alerts=AlertState::load(j.at("alerts"));quiet=j.value("quiet",false);topmost=j.value("topmost",true);savedVisible=j.value("visible",true);savedX=j.at("x").get<int>();savedY=j.at("y").get<int>();status=L"Cached readings - refreshing...";
    if(j.contains("expressionThresholds")){try{expressionThresholds=ExpressionThresholds::load(j["expressionThresholds"]);}catch(...){expressionThresholds={};logEvent("Invalid expression settings; expression defaults restored");}}
}catch(...){snapshot={};alerts={};logEvent("Settings invalid; defaults restored");}}
int px(int v){return MulDiv(v,dpi,96);}
void animationTimer(){KillTimer(windowHandle,1);if(IsWindowVisible(windowHandle)&&!locked)SetTimer(windowHandle,1,125,nullptr);}
std::wstring percent(const Allowance& a){if(!a.valid)return L"--";std::wostringstream s;s<<std::fixed<<std::setprecision(a.remaining==std::floor(a.remaining)?0:1)<<a.remaining<<L"%";return s.str();}
std::wstring duration(long long reset){long long s=reset-utcNow();if(s<=0)return L"Awaiting refresh";long long minutes=(s+59)/60;long long d=minutes/1440,h=minutes/60%24,m=minutes%60;std::wstring r;if(d)r=std::to_wstring(d)+L"d ";if(h||d)r+=std::to_wstring(h)+L"h ";r+=std::to_wstring(m)+L"m";return r;}
std::wstring localTime(long long stamp){if(stamp<=0)return L"Unavailable";time_t t=(time_t)stamp;tm local{};localtime_s(&local,&t);wchar_t s[96];wcsftime(s,96,L"%a %d %b, %H:%M",&local);return s;}
std::wstring label(const Allowance& a,int index){if(!a.valid)return index==0?L"5 hr":L"Weekly";if(a.minutes==300)return L"5 hr";if(a.minutes==10080)return L"Weekly";if(a.minutes%60==0)return std::to_wstring(a.minutes/60)+L"h";return std::to_wstring(a.minutes)+L"m";}
Mood currentMood(){LASTINPUTINFO input{sizeof(input)};bool inactive=GetLastInputInfo(&input)&&DWORD(GetTickCount()-input.dwTime)>=600000;return chooseMood(snapshot,expressionThresholds,stale,busy,GetTickCount64()<resetUntil,inactive);}
void render(){if(!renderer||locked)return;try{RenderData d;d.snapshot=snapshot;d.mood=currentMood();d.stale=stale;d.quiet=quiet;d.demo=demo;d.fetching=busy;d.tick=frame;for(int i=0;i<2;i++){d.labels[i]=label(snapshot.limits[i],i);d.percentages[i]=percent(snapshot.limits[i]);d.countdowns[i]=snapshot.limits[i].valid?duration(snapshot.limits[i].reset):L"Unavailable";}renderer->draw(windowHandle,d);}catch(const std::exception& e){status=wide(e.what());}}

LRESULT CALLBACK settingsProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_COMMAND&&LOWORD(w)==IDOK){ExpressionThresholds t;double* values[]={&t.attentive,&t.worried,&t.critical};bool parsed=true;for(int i=0;i<3;i++){wchar_t buffer[64]{};GetDlgItemTextW(h,201+i,buffer,64);wchar_t* end=nullptr;*values[i]=wcstod(buffer,&end);while(end&&iswspace(*end))end++;if(end==buffer||!end||*end)parsed=false;}
        if(!parsed||!t.valid()){MessageBoxW(h,L"Enter percentages with 0 <= Critical < Worried < Attentive <= 100.\nNotification alerts stay at 25%, 10% and 5%.",L"Check expression thresholds",MB_OK|MB_ICONINFORMATION);return 0;}
        expressionThresholds=t;saveState();render();DestroyWindow(h);return 0;}
    if((msg==WM_COMMAND&&LOWORD(w)==IDCANCEL)||msg==WM_CLOSE){DestroyWindow(h);return 0;}return DefWindowProcW(h,msg,w,l);
}
void showExpressionSettings(){
    HINSTANCE instance=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=settingsProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName=L"NibbleExpressionSettings";RegisterClassW(&wc);
    RECT parent{};GetWindowRect(windowHandle,&parent);RECT size{0,0,px(310),px(215)};DWORD style=WS_CAPTION|WS_SYSMENU;AdjustWindowRectEx(&size,style,FALSE,WS_EX_DLGMODALFRAME);
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,wc.lpszClassName,L"Nibble - Expressions",style,parent.left,parent.top,size.right-size.left,size.bottom-size.top,windowHandle,nullptr,instance,nullptr);
    HFONT font=CreateFontW(-px(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto control=[&](const wchar_t* cls,const std::wstring& text,DWORD flags,int x,int y,int width,int height,int id){auto c=CreateWindowExW(cls==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,cls,text.c_str(),WS_CHILD|WS_VISIBLE|flags,px(x),px(y),px(width),px(height),h,(HMENU)(INT_PTR)id,instance,nullptr);SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);return c;};
    control(L"STATIC",L"Use the lower remaining allowance.",0,16,12,278,20,0);
    const wchar_t* names[]={L"Attentive at or below",L"Worried at or below",L"Critical at or below"};double values[]={expressionThresholds.attentive,expressionThresholds.worried,expressionThresholds.critical};
    for(int i=0;i<3;i++){control(L"STATIC",names[i],0,16,44+i*32,180,22,0);std::wostringstream value;value<<values[i];control(L"EDIT",value.str(),WS_TABSTOP|ES_AUTOHSCROLL,204,42+i*32,66,24,201+i);control(L"STATIC",L"%",0,277,45+i*32,20,20,0);}
    control(L"STATIC",L"Popup alerts remain at 25%, 10% and 5%.",0,16,145,280,20,0);control(L"BUTTON",L"Save",WS_TABSTOP|BS_DEFPUSHBUTTON,130,177,75,26,IDOK);control(L"BUTTON",L"Cancel",WS_TABSTOP,215,177,75,26,IDCANCEL);
    EnableWindow(windowHandle,FALSE);ShowWindow(h,SW_SHOW);SetFocus(GetDlgItem(h,201));MSG message{};while(IsWindow(h)&&GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(h,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}EnableWindow(windowHandle,TRUE);SetForegroundWindow(windowHandle);DeleteObject(font);
}
void updateTray(){
    std::wstring tip=L"Nibble | "+label(snapshot.limits[0],0)+L" "+percent(snapshot.limits[0])+L" | "+label(snapshot.limits[1],1)+L" "+percent(snapshot.limits[1])+L" remaining";
    if(stale)tip+=L" (stale)";if(quiet)tip+=L" [quiet]";
    wcsncpy_s(tray.szTip,tip.c_str(),_TRUNCATE);tray.uFlags=NIF_TIP;Shell_NotifyIconW(NIM_MODIFY,&tray);
}
void addTray(){tray={};tray.cbSize=sizeof(tray);tray.hWnd=windowHandle;tray.uID=1;tray.uCallbackMessage=TrayMessage;tray.hIcon=nibbleIcon;tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;wcscpy_s(tray.szTip,L"Nibble - Codex usage companion");Shell_NotifyIconW(NIM_ADD,&tray);updateTray();}
void flushNotice(){if(quiet||notices.empty()||GetTickCount64()<nextNotice)return;auto n=notices.front();notices.pop_front();tray.uFlags=NIF_INFO;tray.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;wcsncpy_s(tray.szInfoTitle,wide(n.title).c_str(),_TRUNCATE);wcsncpy_s(tray.szInfo,wide(n.body).c_str(),_TRUNCATE);Shell_NotifyIconW(NIM_MODIFY,&tray);nextNotice=GetTickCount64()+8000;}
void startFetch(){if(demo||prototype||stopping||busy.exchange(true))return;if(worker.joinable())worker.join();lastFetchAttempt=utcNow();status=snapshot.fetched?L"Refreshing usage...":L"Connecting to Codex...";InvalidateRect(windowHandle,nullptr,FALSE);
    worker=std::thread([]{auto r=new FetchResult(fetchUsage(stopping));if(!PostMessageW(windowHandle,FetchMessage,0,(LPARAM)r))delete r;});
}
void summary(){std::wstring s=L"NIBBLE  /  CODEX USAGE\n\n";if(demo)s+=L"DEMO DATA - not your account\n\n";
    for(int i=0;i<2;i++){auto& a=snapshot.limits[i];s+=label(a,i)+L" remaining: "+percent(a)+L"\n";s+=a.valid?L"Resets: "+localTime(a.reset)+L" ("+duration(a.reset)+L")\n\n":L"Allowance unavailable\n\n";}
    s+=L"Reset credits: "+(snapshot.credits<0?std::wstring(L"Unavailable"):std::to_wstring(snapshot.credits))+L"\nRedeem eligible credits in Codex.\n\nLast successful update: "+localTime(snapshot.fetched)+L"\nStatus: "+status;
    if(stale)s+=L"\nCached values may have changed. Refresh to confirm.";
    s+=L"\n\nRefresh: every 2 minutes | Quiet mode: "+std::wstring(quiet?L"on":L"off")+L"\nDrag to move. Right-click for controls.";
    MessageBoxW(windowHandle,s.c_str(),L"Nibble - Usage summary",MB_OK|MB_ICONINFORMATION);
}
COLORREF meterColour(double value){return value<=5?RGB(255,101,116):value<=10?RGB(255,154,95):value<=25?RGB(250,209,109):RGB(111,236,181);}
void rounded(HDC dc,int x,int y,int w,int h,int radius,COLORREF fill,COLORREF stroke){HBRUSH b=CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,stroke);auto ob=SelectObject(dc,b),op=SelectObject(dc,p);RoundRect(dc,x,y,x+w,y+h,radius,radius);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);}
void ellipse(HDC dc,int x,int y,int w,int h,COLORREF fill){HBRUSH b=CreateSolidBrush(fill);auto ob=SelectObject(dc,b),op=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,x,y,x+w,y+h);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);}
void text(HDC dc,const std::wstring& s,int x,int y,int w,int h,int size,COLORREF colour,bool bold=false,UINT flags=DT_LEFT){HFONT f=CreateFontW(-size,0,0,0,bold?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(dc,f);SetTextColor(dc,colour);SetBkMode(dc,TRANSPARENT);RECT r{x,y,x+w,y+h};DrawTextW(dc,s.c_str(),-1,&r,flags|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc,old);DeleteObject(f);}
HICON makeIcon(){
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);HBITMAP colour=CreateCompatibleBitmap(screen,32,32),mask=CreateBitmap(32,32,1,1,nullptr);auto old=SelectObject(dc,colour);
    RECT r{0,0,32,32};FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));ellipse(dc,2,4,28,25,RGB(125,235,169));ellipse(dc,10,14,3,5,RGB(30,65,59));ellipse(dc,21,14,3,5,RGB(30,65,59));SelectObject(dc,mask);PatBlt(dc,0,0,32,32,BLACKNESS);SelectObject(dc,old);
    ICONINFO info{TRUE,0,0,mask,colour};HICON icon=CreateIconIndirect(&info);DeleteObject(colour);DeleteObject(mask);DeleteDC(dc);ReleaseDC(nullptr,screen);return icon;
}
void menu(){POINT p;GetCursorPos(&p);HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,ShowPet,IsWindowVisible(windowHandle)?L"Hide Nibble":L"Show Nibble");AppendMenuW(m,MF_STRING|(busy?MF_GRAYED:0),Refresh,L"Refresh now");AppendMenuW(m,MF_STRING,Summary,L"Usage summary");AppendMenuW(m,MF_STRING,ExpressionSettings,L"Expression thresholds...");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING|(quiet?MF_CHECKED:0),Quiet,L"Quiet mode");AppendMenuW(m,MF_STRING|(topmost?MF_CHECKED:0),Topmost,L"Always on top");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,ExitApp,L"Exit Nibble");SetForegroundWindow(windowHandle);TrackPopupMenu(m,TPM_RIGHTBUTTON,p.x,p.y,0,windowHandle,nullptr);PostMessageW(windowHandle,WM_NULL,0,0);DestroyMenu(m);}
void setVisible(bool show){ShowWindow(windowHandle,show?SW_SHOWNOACTIVATE:SW_HIDE);animationTimer();saveState();}
void ensureOnScreen(){RECT r;GetWindowRect(windowHandle,&r);HMONITOR monitor=MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(monitor,&mi);int x=std::clamp((int)r.left,(int)mi.rcWork.left,std::max((int)mi.rcWork.left,(int)(mi.rcWork.right-(r.right-r.left))));int y=std::clamp((int)r.top,(int)mi.rcWork.top,std::max((int)mi.rcWork.top,(int)(mi.rcWork.bottom-(r.bottom-r.top))));SetWindowPos(windowHandle,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(msg==taskbarCreated&&taskbarCreated){addTray();return 0;}
    switch(msg){
    case WM_CREATE:windowHandle=h;return 0;
    case RevealMessage:setVisible(true);SetForegroundWindow(h);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);render();return 0;}
    case WM_LBUTTONDOWN:ReleaseCapture();SendMessageW(h,WM_NCLBUTTONDOWN,HTCAPTION,0);saveState();return 0;
    case WM_LBUTTONDBLCLK:summary();return 0;
    case WM_RBUTTONUP:menu();return 0;
    case WM_CLOSE:setVisible(false);return 0;
    case WM_EXITSIZEMOVE:ensureOnScreen();saveState();return 0;
    case WM_DISPLAYCHANGE:ensureOnScreen();return 0;
    case WM_DPICHANGED:{dpi=HIWORD(w);auto r=(RECT*)l;SetWindowPos(h,nullptr,r->left,r->top,px(LogicalW),px(LogicalH),SWP_NOZORDER|SWP_NOACTIVATE);ensureOnScreen();InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_WTSSESSION_CHANGE:if(w==WTS_SESSION_LOCK)locked=true;else if(w==WTS_SESSION_UNLOCK){locked=false;startFetch();}animationTimer();return 0;
    case WM_POWERBROADCAST:if(w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND){stale=true;startFetch();}return TRUE;
    case WM_TIMER:
        if(w==1){frame++;render();}else if(w==2){if(!busy&&utcNow()-lastFetchAttempt>=120)startFetch();flushNotice();if(!demo&&!busy&&snapshot.fetched&&utcNow()-snapshot.fetched>150&&!stale){stale=true;status=L"Readings are stale - refresh needed";updateTray();}if(IsWindowVisible(h)&&!locked)render();}return 0;
    case FetchMessage:{std::unique_ptr<FetchResult> r((FetchResult*)l);if(worker.joinable())worker.join();busy=false;measuredPeak=std::max(measuredPeak,r->peakTreeWorkingSet);
        if(r->error.empty()&&r->snapshot.fetched){if(confirmedReset(r->snapshot,snapshot)&&!quiet)resetUntil=GetTickCount64()+4000;auto pending=alerts.apply(r->snapshot,snapshot,quiet);snapshot=r->snapshot;stale=false;status=L"Connected";lastError.clear();for(auto& n:pending)notices.push_back(n);saveState();logEvent("Usage refreshed; owned helper cleanup="+std::string(r->clean?"confirmed":"pending"));}
        else {stale=true;status=wide(r->error.empty()?"Refresh cancelled":r->error);if(lastError!=r->error){logEvent(r->error);lastError=r->error;}}
        updateTray();InvalidateRect(h,nullptr,FALSE);flushNotice();return 0;}
    case TrayMessage:if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU)menu();else if(l==WM_LBUTTONDBLCLK){setVisible(true);SetForegroundWindow(h);}else if(l==NIN_BALLOONUSERCLICK)summary();return 0;
    case WM_COMMAND:switch(LOWORD(w)){
        case ShowPet:setVisible(!IsWindowVisible(h));break;
        case Refresh:startFetch();break;
        case Summary:summary();break;
        case ExpressionSettings:showExpressionSettings();break;
        case Quiet:quiet=!quiet;if(quiet)notices.clear();saveState();updateTray();InvalidateRect(h,nullptr,FALSE);break;
        case Topmost:topmost=!topmost;SetWindowPos(h,topmost?HWND_TOPMOST:HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);saveState();break;
        case ExitApp:saveState();DestroyWindow(h);break;
        }return 0;
    case WM_QUERYENDSESSION:saveState();return TRUE;
    case WM_DESTROY:stopping=true;KillTimer(h,1);KillTimer(h,2);if(worker.joinable())worker.join();{MSG pending;while(PeekMessageW(&pending,h,FetchMessage,FetchMessage,PM_REMOVE))delete (FetchResult*)pending.lParam;}WTSUnRegisterSessionNotification(h);tray.uFlags=0;Shell_NotifyIconW(NIM_DELETE,&tray);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    std::wstring cli=args;exportMode=cli.find(L"--export-previews")!=std::wstring::npos;demo=exportMode||cli.find(L"--demo")!=std::wstring::npos;prototype=cli.find(L"--prototype")!=std::wstring::npos;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Handle mutex(CreateMutexW(nullptr,FALSE,demo?L"Local\\NibbleDemo":L"Local\\NibbleUsageCompanion"));if(GetLastError()==ERROR_ALREADY_EXISTS){HWND existing=FindWindowW(L"NibbleCompanion",nullptr);if(existing)PostMessageW(existing,RevealMessage,0,0);return 0;}
    auto root=env(L"NIBBLE_DATA_DIR");dataDir=root.empty()?fs::path(env(L"LOCALAPPDATA"))/L"Nibble":fs::path(root);std::error_code ec;fs::create_directories(dataDir,ec);if(!demo&&!prototype)loadState();
    if(demo){snapshot.fetched=utcNow();snapshot.limits={Allowance{true,68,utcNow()+7200,300},Allowance{true,65,utcNow()+5*86400,10080}};snapshot.credits=0;stale=false;status=L"Demo data";}
    WNDCLASSW wc{};wc.style=CS_DBLCLKS;wc.lpfnWndProc=proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.lpszClassName=L"NibbleCompanion";nibbleIcon=makeIcon();wc.hIcon=nibbleIcon;RegisterClassW(&wc);
    POINT at{savedX==INT_MIN?100:savedX,savedY==INT_MIN?100:savedY};HMONITOR monitor=MonitorFromPoint(at,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(monitor,&mi);
    dpi=GetDpiForSystem();int x=savedX==INT_MIN?mi.rcWork.right-px(LogicalW)-24:savedX,y=savedY==INT_MIN?mi.rcWork.bottom-px(LogicalH)-24:savedY;
    windowHandle=CreateWindowExW(WS_EX_LAYERED|(demo?WS_EX_APPWINDOW:WS_EX_TOOLWINDOW)|(topmost?WS_EX_TOPMOST:0),wc.lpszClassName,L"Nibble",WS_POPUP,x,y,px(LogicalW),px(LogicalH),nullptr,nullptr,instance,nullptr);
    if(!windowHandle)return 1;dpi=GetDpiForWindow(windowHandle);SetWindowPos(windowHandle,nullptr,0,0,px(LogicalW),px(LogicalH),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    try{wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);renderer=std::make_unique<Renderer>();renderer->initialize(fs::path(path).parent_path()/L"assets");renderer->resize(dpi);}catch(const std::exception& e){MessageBoxW(nullptr,wide(e.what()).c_str(),L"Nibble artwork unavailable",MB_OK|MB_ICONERROR);renderer.reset();DestroyWindow(windowHandle);CoUninitialize();return 1;}
    if(exportMode){wchar_t path[32768];GetModuleFileNameW(nullptr,path,32768);fs::path previews=fs::path(path).parent_path()/L"previews";fs::create_directories(previews);for(auto mood:{Mood::Happy,Mood::Moderate,Mood::Low,Mood::Critical,Mood::Reset,Mood::Processing,Mood::Sleeping,Mood::Unknown}){RenderData d;d.snapshot=snapshot;d.mood=mood;d.demo=true;d.stale=mood==Mood::Unknown;d.fetching=mood==Mood::Processing;if(mood==Mood::Moderate)d.snapshot.limits[0].remaining=50;if(mood==Mood::Low)d.snapshot.limits[0].remaining=25;if(mood==Mood::Critical)d.snapshot.limits[0].remaining=10;for(int i=0;i<2;i++){d.labels[i]=label(d.snapshot.limits[i],i);d.percentages[i]=percent(d.snapshot.limits[i]);d.countdowns[i]=duration(d.snapshot.limits[i].reset);}renderer->draw(windowHandle,d);renderer->exportPng(previews/(std::string(moodName(mood))+".png"));if(mood==Mood::Happy){for(int testDpi:{96,120,144,192}){auto dir=previews/("dpi-"+std::to_string(testDpi*100/96));fs::create_directories(dir);renderer->draw(windowHandle,d,testDpi);renderer->exportPng(dir/"Happy.png");}}}renderer.reset();DestroyWindow(windowHandle);DestroyIcon(nibbleIcon);CoUninitialize();return 0;}
    ensureOnScreen();taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");addTray();WTSRegisterSessionNotification(windowHandle,NOTIFY_FOR_THIS_SESSION);SetTimer(windowHandle,2,1000,nullptr);ShowWindow(windowHandle,savedVisible?SW_SHOWNOACTIVATE:SW_HIDE);render();animationTimer();if(!demo&&!prototype)startFetch();
    MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}renderer.reset();DestroyIcon(nibbleIcon);CoUninitialize();return 0;
}




