#include <sstream>

#ifdef _WIN32
#define BITMAP WINDOWS_BITMAP
#include "wx-app.h"
#include <wx/xrc/xmlres.h>
#include <wx/event.h>

#include <windows.h>
#include <windowsx.h>
#undef BITMAP
#else
#include "wx-app.h"
#include <wx/xrc/xmlres.h>
#include <wx/event.h>
#endif

#include "wx-config.h"
#include "wx-config_sel.h"

extern "C"
{
        #include "arc.h"
        #include "plat_video.h"
        #include "video.h"
}

extern void InitXmlResource();

wxDEFINE_EVENT(WX_STOP_EMULATION_EVENT, wxCommandEvent);
wxDEFINE_EVENT(WX_POPUP_MENU_EVENT, PopupMenuEvent);
wxDEFINE_EVENT(WX_UPDATE_MENU_EVENT, UpdateMenuEvent);
#ifdef _WIN32
wxDEFINE_EVENT(WX_WIN_SEND_MESSAGE_EVENT, WinSendMessageEvent);
#endif

wxBEGIN_EVENT_TABLE(Frame, wxFrame)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP_NO_MAIN(App);

App::App()
{
        this->frame = NULL;
}

bool App::OnInit()
{
        wxImage::AddHandler( new wxPNGHandler );
        wxXmlResource::Get()->InitAllHandlers();
        InitXmlResource();

        frame = new Frame(this, "null frame", wxPoint(500, 500),
                        wxSize(100, 100));
        frame->Start();
        return true;
}

static void *main_frame = NULL;
static void *main_menu = NULL;

Frame::Frame(App* app, const wxString& title, const wxPoint& pos,
                const wxSize& size) :
                wxFrame(NULL, wxID_ANY, title, pos, size, 0)
{
        main_frame = this;
        
        this->menu = wxXmlResource::Get()->LoadMenu(wxT("main_menu"));
        main_menu = this->menu;

        Bind(wxEVT_MENU, &Frame::OnMenuCommand, this);
        Bind(WX_POPUP_MENU_EVENT, &Frame::OnPopupMenuEvent, this);
        Bind(WX_UPDATE_MENU_EVENT, &Frame::OnUpdateMenuEvent, this);
        Bind(WX_STOP_EMULATION_EVENT, &Frame::OnStopEmulationEvent, this);
#ifdef _WIN32
        Bind(WX_WIN_SEND_MESSAGE_EVENT, &Frame::OnWinSendMessageEvent, this);
#endif

        CenterOnScreen();
}

void Frame::Start()
{
        if (!ShowConfigSelection())
                arc_start_main_thread(this, this->menu);
        else
                Quit(0);
}

wxMenu* Frame::GetMenu()
{
        return menu;
}

void Frame::Quit(bool stop_emulator)
{
        Destroy();
}

void Frame::OnStopEmulationEvent(wxCommandEvent &event)
{
        arc_stop_main_thread();
        if (!ShowConfigSelection())
                arc_start_main_thread(this, this->menu);
        else
                Quit(0);
}

void Frame::UpdateMenu(wxMenu *menu)
{
        wxMenuItem *item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DISC_FAST"));
        item->Check(fastdisc);
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_OPTIONS_SOUND"));
        item->Check(soundena);
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_OPTIONS_STEREO"));
        item->Check(stereo);

        if (dblscan)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_BLIT_SCALE"));
        else
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_BLIT_SCAN"));
        item->Check(true);
        if (hires)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_MONITOR_HIRES"));
        else
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_MONITOR_NORMAL"));
        item->Check(true);
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_RESIZEABLE"));
        item->Check(video_window_resizeable);
        if (display_mode == DISPLAY_MODE_NO_BORDERS)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_NO_BORDERS"));
        else if (display_mode == DISPLAY_MODE_NATIVE_BORDERS)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_NATIVE_BORDERS"));
        else
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_TV"));
        item->Check(true);
        if (video_fullscreen_scale == FULLSCR_SCALE_FULL)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_FULL"));
        else if (video_fullscreen_scale == FULLSCR_SCALE_43)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_43"));
        else if (video_fullscreen_scale == FULLSCR_SCALE_SQ)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_SQ"));
        else if (video_fullscreen_scale == FULLSCR_SCALE_INT)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_FS_INT"));
        item->Check(true);
        if (video_linear_filtering)
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_SCALE_LINEAR"));
        else
                item = ((wxMenu*)menu)->FindItem(XRCID("IDM_VIDEO_SCALE_NEAREST"));
        item->Check(true);
        
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_AUTO"));
        item->Enable(video_renderer_available(RENDERER_AUTO) ? true : false);
        item->Check((selected_video_renderer == RENDERER_AUTO) ? true : false);
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_DIRECT3D"));
        item->Enable(video_renderer_available(RENDERER_DIRECT3D) ? true : false);
        item->Check((selected_video_renderer == RENDERER_DIRECT3D) ? true : false);
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_OPENGL"));
        item->Enable(video_renderer_available(RENDERER_OPENGL) ? true : false);
        item->Check((selected_video_renderer == RENDERER_OPENGL) ? true : false);
        item = ((wxMenu*)menu)->FindItem(XRCID("IDM_DRIVER_SOFTWARE"));
        item->Enable(video_renderer_available(RENDERER_SOFTWARE) ? true : false);
        item->Check((selected_video_renderer == RENDERER_SOFTWARE) ? true : false);
}

void Frame::OnPopupMenuEvent(PopupMenuEvent &event)
{
        wxWindow *window = event.GetWindow();
        wxMenu *menu = event.GetMenu();

        UpdateMenu(menu);
        
        window->PopupMenu(menu);
}

void Frame::ChangeDisc(int drive)
{
        wxString old_fn(discname[drive]);

        wxFileDialog dlg(NULL, "Select a disc image", "", old_fn,
                        "All disc images|*.adf;*.img;*.fdi;*.apd;*.jfd|FDI Disc Image|*.fdi|APD Disc Image|*.apd|ADFS Disc Image|*.adf|DOS Disc Image|*.img|All Files|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        if (dlg.ShowModal() == wxID_OK)
        {
                char new_fn[512];
                wxString new_fn_str = dlg.GetPath();

                strcpy(new_fn, new_fn_str.mb_str());
                arc_disc_change(drive, new_fn);
        }
}

void Frame::OnMenuCommand(wxCommandEvent &event)
{
        if (event.GetId() == XRCID("IDM_FILE_EXIT"))
        {
                arc_stop_emulation();
        }
        else if (event.GetId() == XRCID("IDM_FILE_RESET"))
        {
                arc_do_reset();
        }
        else if (event.GetId() == XRCID("IDM_DISC_CHANGE_0"))
        {
                ChangeDisc(0);
        }
        else if (event.GetId() == XRCID("IDM_DISC_CHANGE_1"))
        {
                ChangeDisc(1);
        }
        else if (event.GetId() == XRCID("IDM_DISC_CHANGE_2"))
        {
                ChangeDisc(2);
        }
        else if (event.GetId() == XRCID("IDM_DISC_CHANGE_3"))
        {
                ChangeDisc(3);
        }
        else if (event.GetId() == XRCID("IDM_DISC_EJECT_0"))
        {
                arc_disc_eject(0);
        }
        else if (event.GetId() == XRCID("IDM_DISC_EJECT_1"))
        {
                arc_disc_eject(1);
        }
        else if (event.GetId() == XRCID("IDM_DISC_EJECT_2"))
        {
                arc_disc_eject(2);
        }
        else if (event.GetId() == XRCID("IDM_DISC_EJECT_3"))
        {
                arc_disc_eject(3);
        }
        else if (event.GetId() == XRCID("IDM_DISC_FAST"))
        {
                fastdisc ^= 1;

                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(fastdisc);
        }
        else if (event.GetId() == XRCID("IDM_OPTIONS_SOUND"))
        {
                soundena ^= 1;

                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(soundena);
        }
        else if (event.GetId() == XRCID("IDM_OPTIONS_STEREO"))
        {
                stereo ^= 1;

                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(stereo);
        }
        else if (event.GetId() == XRCID("IDM_MACHINE_CONFIGURE"))
        {
                arc_pause_main_thread();
                ShowConfig(true);
                arc_resume_main_thread();
        }
        else if (event.GetId() == XRCID("IDM_MONITOR_NORMAL"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);
                
                arc_set_hires(0);
        }
        else if (event.GetId() == XRCID("IDM_MONITOR_MONO"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);
                
                arc_set_hires(1);
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_FULLSCR"))
        {
                if (firstfull)
                {
                        firstfull = 0;
                        
                        arc_pause_main_thread();
                        wxMessageBox("Use CTRL + END to return to windowed mode", "Arculator", wxOK | wxCENTRE | wxSTAY_ON_TOP);
                        arc_resume_main_thread();
                }
                arc_enter_fullscreen();
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_RESIZEABLE"))
        {
                video_window_resizeable ^= 1;
                arc_set_resizeable();

                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(video_window_resizeable);
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_NO_BORDERS"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);
                
                arc_set_display_mode(DISPLAY_MODE_NO_BORDERS);
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_NATIVE_BORDERS"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);
                
                arc_set_display_mode(DISPLAY_MODE_NATIVE_BORDERS);
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_TV"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                arc_set_display_mode(DISPLAY_MODE_TV);
        }
        else if (event.GetId() == XRCID("IDM_DRIVER_AUTO"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                selected_video_renderer = RENDERER_AUTO;
                arc_renderer_reset();
        }
        else if (event.GetId() == XRCID("IDM_DRIVER_DIRECT3D"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                selected_video_renderer = RENDERER_DIRECT3D;
                arc_renderer_reset();
        }
        else if (event.GetId() == XRCID("IDM_DRIVER_OPENGL"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                selected_video_renderer = RENDERER_OPENGL;
                arc_renderer_reset();
        }
        else if (event.GetId() == XRCID("IDM_DRIVER_SOFTWARE"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                selected_video_renderer = RENDERER_SOFTWARE;
                arc_renderer_reset();
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_SCALE_NEAREST"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                video_linear_filtering = 0;
                arc_renderer_reset();
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_SCALE_LINEAR"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                video_linear_filtering = 1;
                arc_renderer_reset();
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_FS_FULL"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                video_fullscreen_scale = FULLSCR_SCALE_FULL;
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_FS_43"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                video_fullscreen_scale = FULLSCR_SCALE_43;
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_FS_SQ"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                video_fullscreen_scale = FULLSCR_SCALE_SQ;
        }
        else if (event.GetId() == XRCID("IDM_VIDEO_FS_INT"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);
                
                video_fullscreen_scale = FULLSCR_SCALE_INT;
        }
        else if (event.GetId() == XRCID("IDM_BLIT_SCAN"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);
                
                arc_set_dblscan(0);
        }
        else if (event.GetId() == XRCID("IDM_BLIT_SCALE"))
        {
                wxMenuItem *item = ((wxMenu*)menu)->FindItem(event.GetId());
                item->Check(true);

                arc_set_dblscan(1);
        }
}

extern "C" void arc_stop_emulation()
{
        wxCommandEvent* event = new wxCommandEvent(WX_STOP_EMULATION_EVENT, wxID_ANY);
        event->SetEventObject((wxWindow*)main_frame);
        wxQueueEvent((wxWindow*)main_frame, event);
}

extern "C" void arc_popup_menu()
{
        PopupMenuEvent *event = new PopupMenuEvent((wxWindow *)main_frame, (wxMenu *)main_menu);
        wxQueueEvent((wxWindow *)main_frame, event);
}

extern "C" void *wx_getnativemenu(void *menu)
{
#ifdef _WIN32
        return ((wxMenu*)menu)->GetHMenu();
#endif
        return 0;
}

extern "C" void arc_update_menu()
{
        UpdateMenuEvent *event = new UpdateMenuEvent((wxMenu *)main_menu);
        wxQueueEvent((wxWindow *)main_frame, event);
}

void Frame::OnUpdateMenuEvent(UpdateMenuEvent &event)
{
        wxMenu *menu = event.GetMenu();

        UpdateMenu(menu);
}

#ifdef _WIN32
static void *wx_getnativewindow(void *window)
{
        return ((wxWindow *)window)->GetHWND();
}

extern "C" void wx_winsendmessage(void *window, int msg, INT_PARAM wParam, LONG_PARAM lParam)
{
        WinSendMessageEvent *event = new WinSendMessageEvent(wx_getnativewindow(window), msg, wParam, lParam);
        wxQueueEvent((wxWindow *)window, event);
}

void Frame::OnWinSendMessageEvent(WinSendMessageEvent& event)
{
        SendMessage((HWND)event.GetHWND(), event.GetMessage(), event.GetWParam(), event.GetLParam());
}
#endif