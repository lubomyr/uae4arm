#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#include "SelectorEntry.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "gui.h"
#include "gui_handling.h"
#include "memory-uae.h"
#include "autoconf.h"

#if defined(ANDROIDSDL)
#include "androidsdl_event.h"
#include <SDL_screenkeyboard.h>
#include <SDL_android.h>
#include <android/log.h>
#endif

bool gui_running = false;
static int last_active_panel = 2;

#define MAX_STARTUP_TITLE 64
#define MAX_STARTUP_MESSAGE 256
static TCHAR startup_title[MAX_STARTUP_TITLE] = _T("");
static TCHAR startup_message[MAX_STARTUP_MESSAGE] = _T("");


void target_startup_msg(TCHAR *title, TCHAR *msg)
{
  _tcsncpy(startup_title, title, MAX_STARTUP_TITLE);
  _tcsncpy(startup_message, msg, MAX_STARTUP_MESSAGE);
}


ConfigCategory categories[] = {
  { "Paths",            "data/paths.ico",     NULL, NULL, InitPanelPaths,     ExitPanelPaths,     RefreshPanelPaths,      HelpPanelPaths },
  { "Quickstart",       "data/quickstart.ico",  NULL, NULL, InitPanelQuickstart,  ExitPanelQuickstart,  RefreshPanelQuickstart, HelpPanelQuickstart },
  { "Configurations",   "data/file.ico",      NULL, NULL, InitPanelConfig,    ExitPanelConfig,    RefreshPanelConfig,     HelpPanelConfig },
  { "CPU and FPU",      "data/cpu.ico",       NULL, NULL, InitPanelCPU,       ExitPanelCPU,       RefreshPanelCPU,        HelpPanelCPU },
  { "Chipset",          "data/cpu.ico",       NULL, NULL, InitPanelChipset,   ExitPanelChipset,   RefreshPanelChipset,    HelpPanelChipset },
  { "ROM",              "data/chip.ico",      NULL, NULL, InitPanelROM,       ExitPanelROM,       RefreshPanelROM,        HelpPanelROM },
  { "RAM",              "data/chip.ico",      NULL, NULL, InitPanelRAM,       ExitPanelRAM,       RefreshPanelRAM,        HelpPanelRAM },
  { "Floppy drives",    "data/35floppy.ico",  NULL, NULL, InitPanelFloppy,    ExitPanelFloppy,    RefreshPanelFloppy,     HelpPanelFloppy },
  { "Hard drives / CD", "data/drive.ico",     NULL, NULL, InitPanelHD,        ExitPanelHD,        RefreshPanelHD,         HelpPanelHD },
  { "Display",          "data/screen.ico",    NULL, NULL, InitPanelDisplay,   ExitPanelDisplay,   RefreshPanelDisplay,    HelpPanelDisplay },
  { "Sound",            "data/sound.ico",     NULL, NULL, InitPanelSound,     ExitPanelSound,     RefreshPanelSound,      HelpPanelSound },
  { "Input",            "data/joystick.ico",  NULL, NULL, InitPanelInput,     ExitPanelInput,     RefreshPanelInput,      HelpPanelInput },
  { "Miscellaneous",    "data/misc.ico",      NULL, NULL, InitPanelMisc,      ExitPanelMisc,      RefreshPanelMisc,       HelpPanelMisc },
  { "Savestates",       "data/savestate.png", NULL, NULL, InitPanelSavestate, ExitPanelSavestate, RefreshPanelSavestate,  HelpPanelSavestate },
#ifdef ANDROIDSDL  
  { "OnScreen",         "data/screen.ico",    NULL, NULL, InitPanelOnScreen,  ExitPanelOnScreen, RefreshPanelOnScreen,  HelpPanelOnScreen },
#endif
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};
#ifdef ANDROIDSDL
enum { PANEL_PATHS, PANEL_QUICKSTART, PANEL_CONFIGURATIONS, PANEL_CPU, PANEL_CHIPSET, PANEL_ROM, PANEL_RAM, PANEL_FLOPPY, PANEL_HD, PANEL_DISPLAY, PANEL_SOUND, PANEL_INPUT, PANEL_MISC, PANEL_SAVESTATES, 
       PANEL_ONSCREEN, NUM_PANELS };
#else
enum { PANEL_PATHS, PANEL_QUICKSTART, PANEL_CONFIGURATIONS, PANEL_CPU, PANEL_CHIPSET, PANEL_ROM, PANEL_RAM, PANEL_FLOPPY, PANEL_HD, PANEL_DISPLAY, PANEL_SOUND, PANEL_INPUT, PANEL_MISC, PANEL_SAVESTATES, 
       NUM_PANELS };
#endif


gcn::Gui* uae_gui;
gcn::Color gui_baseCol;
gcn::Color gui_baseColLabel;
gcn::Color colSelectorInactive;
gcn::Color colSelectorActive;
gcn::Container* gui_top;
gcn::Container* selectors;
gcn::contrib::SDLTrueTypeFont* gui_font;
SDL_Surface* gui_screen;
gcn::SDLGraphics* gui_graphics;
gcn::SDLInput* gui_input;
gcn::SDLImageLoader* gui_imageLoader;

namespace widgets 
{
  // Main buttons
  gcn::Button* cmdQuit;
  gcn::Button* cmdReset;
  gcn::Button* cmdRestart;
  gcn::Button* cmdStart;
  gcn::Button* cmdHelp;
}


/* Flag for changes in rtarea:
  Bit 0: any HD in config?
  Bit 1: force because add/remove HD was clicked or new config loaded
  Bit 2: socket_emu on
  Bit 3: mousehack on
  Bit 4: rtgmem on
  Bit 5: chipmem larger than 2MB
  
  gui_rtarea_flags_onenter is set before GUI is shown, bit 1 may change during GUI display.
*/
static int gui_rtarea_flags_onenter;

static int gui_create_rtarea_flag(struct uae_prefs *p)
{
  int flag = 0;
  
  if(count_HDs(p) > 0)
    flag |= 1;
  
	if (p->socket_emu)
		flag |= 4;

  if (p->input_tablet > 0)
    flag |= 8;

  if(p->rtgboards[0].rtgmem_size)
    flag |= 16;

  if (p->chipmem_size > 2 * 1024 * 1024)
    flag |= 32;

  return flag;
}

void gui_force_rtarea_hdchange(void)
{
  gui_rtarea_flags_onenter |= 2;
}

void gui_restart(void)
{
  gui_running = false;
}

static void (*refreshFuncAfterDraw)(void) = NULL;

void RegisterRefreshFunc(void (*func)(void))
{
  refreshFuncAfterDraw = func;
}

void FocusBugWorkaround(gcn::Window *wnd)
{
  // When modal dialog opens via mouse, the dialog will not
  // has the focus unless one mouse click. We simulate the click...
  SDL_Event event;
  event.type = SDL_MOUSEBUTTONDOWN;
  event.button.button = SDL_BUTTON_LEFT;
  event.button.state = SDL_PRESSED;
  event.button.x = wnd->getX() + 2;
  event.button.y = wnd->getY() + 2;
  gui_input->pushInput(event);
  event.type = SDL_MOUSEBUTTONUP;
  gui_input->pushInput(event);
}


static void ShowHelpRequested()
{
  std::vector<std::string> helptext;
  if(categories[last_active_panel].HelpFunc != NULL && categories[last_active_panel].HelpFunc(helptext))
  {
    //------------------------------------------------
    // Show help for current panel
    //------------------------------------------------
    char title[128];
    snprintf(title, 128, "Help for %s", categories[last_active_panel].category);
    ShowHelp(title, helptext);
  }
}


namespace sdl
{
  void gui_init()
  {
    //-------------------------------------------------
    // Set layer for GUI screen
    //-------------------------------------------------
  	char tmp[20];
  	snprintf(tmp, 20, "%dx%d", GUI_WIDTH, GUI_HEIGHT);
  	setenv("SDL_OMAP_LAYER_SIZE", tmp, 1);
  	snprintf(tmp, 20, "0,0,0,0");
  	setenv("SDL_OMAP_BORDER_CUT", tmp, 1);

    //-------------------------------------------------
    // Create new screen for GUI
    //-------------------------------------------------
    gui_screen = SDL_SetVideoMode(GUI_WIDTH, GUI_HEIGHT, 16, SDL_SWSURFACE);
    SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#ifdef ANDROIDSDL
    // Enable Android multitouch
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickOpen(0);
#endif
    SDL_ShowCursor(SDL_ENABLE);

    //-------------------------------------------------
    // Create helpers for guichan
    //-------------------------------------------------
    gui_imageLoader = new gcn::SDLImageLoader();
    gcn::Image::setImageLoader(gui_imageLoader);
    gui_graphics = new gcn::SDLGraphics();
    gui_graphics->setTarget(gui_screen);
    gui_input = new gcn::SDLInput();
    uae_gui = new gcn::Gui();
    uae_gui->setGraphics(gui_graphics);
    uae_gui->setInput(gui_input);
  }

  void gui_halt()
  {
    delete uae_gui;
    delete gui_imageLoader;
    delete gui_input;
    delete gui_graphics;
    
    SDL_FreeSurface(gui_screen);
    gui_screen = NULL;
  }

  void gui_run()
  {
    //-------------------------------------------------
    // The main loop
    //-------------------------------------------------
    while(gui_running)
    {
      //-------------------------------------------------
      // Check user input
      //-------------------------------------------------
      SDL_Event event;
      while(SDL_PollEvent(&event))
      {
    		if (event.type == SDL_QUIT)
    		{
          //-------------------------------------------------
          // Quit entire program via SQL-Quit
          //-------------------------------------------------
    			uae_quit();
    			gui_running = false;
    			break;
    		}

        else if (event.type == SDL_KEYDOWN)
        {
          gcn::FocusHandler* focusHdl;
          gcn::Widget* activeWidget;
            
          switch(event.key.keysym.sym)
          {
#ifndef ANDROID
            case SDLK_q:
              //-------------------------------------------------
              // Quit entire program via Q on keyboard
              //-------------------------------------------------
              focusHdl = gui_top->_getFocusHandler();
              activeWidget = focusHdl->getFocused();
              if(dynamic_cast<gcn::TextField*>(activeWidget) == NULL) {
          			// ...but only if we are not in a Textfield...
          			uae_quit();
          			gui_running = false;
          		}
        			break;

            case SDLK_ESCAPE:
            case SDLK_RCTRL:
              //-------------------------------------------------
              // Reset Amiga
              //-------------------------------------------------
        			uae_reset(1, 1);
        			gui_running = false;
        			break;

            case SDLK_LCTRL:
    			    if(emulating && widgets::cmdStart->isEnabled())
    		      {
                //------------------------------------------------
                // Continue emulation
                //------------------------------------------------
                gui_running = false;
    		      }
              else
              {
                //------------------------------------------------
                // First start of emulator -> reset Amiga
                //------------------------------------------------
          			uae_reset(0, 1);
          			gui_running = false;
              }
              break;              
#endif
            case SDLK_PAGEDOWN:
            case SDLK_HOME:
              //------------------------------------------------
              // Simulate press of enter when 'X' pressed
              //------------------------------------------------
              event.key.keysym.sym = SDLK_RETURN;
              gui_input->pushInput(event); // Fire key down
              event.type = SDL_KEYUP;  // and the key up
              break;

            case SDLK_UP:
              if(HandleNavigation(DIRECTION_UP))
                continue; // Don't change value when enter ComboBox -> don't send event to control
              break;
              
            case SDLK_DOWN:
              if(HandleNavigation(DIRECTION_DOWN))
                continue; // Don't change value when enter ComboBox -> don't send event to control
              break;
              
            case SDLK_LEFT:
              if(HandleNavigation(DIRECTION_LEFT))
                continue; // Don't change value when enter Slider -> don't send event to control
              break;
              
            case SDLK_RIGHT:
              if(HandleNavigation(DIRECTION_RIGHT))
                continue; // Don't change value when enter Slider -> don't send event to control
              break;
          
            case SDLK_F1:
              ShowHelpRequested();
              widgets::cmdHelp->requestFocus();
              break;
          }
        }

        //-------------------------------------------------
        // Send event to guichan-controls
        //-------------------------------------------------
#ifdef ANDROIDSDL
        androidsdl_event(event, gui_input);
#else
        gui_input->pushInput(event);
#endif
      }

  		if(gui_rtarea_flags_onenter != gui_create_rtarea_flag(&changed_prefs))
        DisableResume();

      // Now we let the Gui object perform its logic.
      uae_gui->logic();
      // Now we let the Gui object draw itself.
      uae_gui->draw();
      // Finally we update the screen.
      wait_for_vsync();
      SDL_Flip(gui_screen);
      
      if(refreshFuncAfterDraw != NULL)
      {
        void (*currFunc)(void) = refreshFuncAfterDraw;
        refreshFuncAfterDraw = NULL;
        currFunc();
      }
    }
  }

}


namespace widgets 
{
  class MainButtonActionListener : public gcn::ActionListener
  {
    public:
      void action(const gcn::ActionEvent& actionEvent)
      {
	      if (actionEvent.getSource() == cmdQuit)
	      {
          //-------------------------------------------------
          // Quit entire program via click on Quit-button
          //-------------------------------------------------
	        uae_quit();
  			  gui_running = false;
  			}
  			else if(actionEvent.getSource() == cmdReset)
			  {
          //-------------------------------------------------
          // Reset Amiga via click on Reset-button
          //-------------------------------------------------
    			uae_reset(1, 1);
    			gui_running = false;
        }
  			else if(actionEvent.getSource() == cmdRestart)
			  {
          //-------------------------------------------------
          // Restart emulator
          //-------------------------------------------------
          char tmp[MAX_PATH];
          fetch_configurationpath (tmp, sizeof (tmp));
          if(strlen(last_loaded_config) > 0)
            strncat (tmp, last_loaded_config, MAX_PATH - 1);
          else
          {
            strncat (tmp, OPTIONSFILENAME, MAX_PATH);
            strncat (tmp, ".uae", MAX_PATH);
          }
    			uae_restart(-1, tmp);
    			gui_running = false;
			  }
			  else if(actionEvent.getSource() == cmdStart)
			  {
			    if(emulating && widgets::cmdStart->isEnabled())
		      {
            //------------------------------------------------
            // Continue emulation
            //------------------------------------------------
            gui_running = false;
		      }
          else
          {
            //------------------------------------------------
            // First start of emulator -> reset Amiga
            //------------------------------------------------
      			uae_reset(0, 1);
      			gui_running = false;
          }
        }
        else if(actionEvent.getSource() == cmdHelp)
        {
          ShowHelpRequested();
          cmdHelp->requestFocus();
        }
      }
  };
  MainButtonActionListener* mainButtonActionListener;


  class PanelFocusListener : public gcn::FocusListener
  {
    public:
      void focusGained(const gcn::Event& event)
      {
        int i;
        for(i=0; categories[i].category != NULL; ++i)
        {
          if(event.getSource() == categories[i].selector)
          {
            categories[i].selector->setActive(true);
            categories[i].panel->setVisible(true);
            last_active_panel = i;
            cmdHelp->setVisible(categories[last_active_panel].HelpFunc != NULL);
          }
          else
          {
            categories[i].selector->setActive(false);
            categories[i].panel->setVisible(false);
          }
        }
      }
  };
  PanelFocusListener* panelFocusListener;


  void gui_init()
  {
    int i;
    int yPos;

    //-------------------------------------------------
    // Define base colors
    //-------------------------------------------------
    gui_baseCol.r = 192;
    gui_baseCol.g = 192;
    gui_baseCol.b = 208;
    gui_baseColLabel.r = gui_baseCol.r;
    gui_baseColLabel.g = gui_baseCol.g;
    gui_baseColLabel.b = gui_baseCol.b;
    gui_baseColLabel.a = 192;
    colSelectorInactive.r = 255;
    colSelectorInactive.g = 255;
    colSelectorInactive.b = 255;
    colSelectorActive.r = 192;
    colSelectorActive.g = 192;
    colSelectorActive.b = 255;

    //-------------------------------------------------
    // Create container for main page
    //-------------------------------------------------
    gui_top = new gcn::Container();
    gui_top->setDimension(gcn::Rectangle(0, 0, GUI_WIDTH, GUI_HEIGHT));
    gui_top->setBaseColor(gui_baseCol);
    uae_gui->setTop(gui_top);

    //-------------------------------------------------
    // Initialize fonts
    //-------------------------------------------------
	  TTF_Init();
#ifdef ANDROID
	  gui_font = new gcn::contrib::SDLTrueTypeFont("data/FreeSans.ttf", 16);	  
#else
	  gui_font = new gcn::contrib::SDLTrueTypeFont("data/FreeSans.ttf", 14);
#endif
    gcn::Widget::setGlobalFont(gui_font);
    
  	//--------------------------------------------------
  	// Create main buttons
  	//--------------------------------------------------
    mainButtonActionListener = new MainButtonActionListener();

  	cmdQuit = new gcn::Button("Quit");
  	cmdQuit->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    cmdQuit->setBaseColor(gui_baseCol);
    cmdQuit->setId("Quit");
    cmdQuit->addActionListener(mainButtonActionListener);

   	cmdReset = new gcn::Button("Reset");
  	cmdReset->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    cmdReset->setBaseColor(gui_baseCol);
  	cmdReset->setId("Reset");
    cmdReset->addActionListener(mainButtonActionListener);

   	cmdRestart = new gcn::Button("Restart");
  	cmdRestart->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    cmdRestart->setBaseColor(gui_baseCol);
  	cmdRestart->setId("Restart");
    cmdRestart->addActionListener(mainButtonActionListener);

  	cmdStart = new gcn::Button("Start");
  	if(emulating)
  	  cmdStart->setCaption("Resume");
  	cmdStart->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    cmdStart->setBaseColor(gui_baseCol);
  	cmdStart->setId("Start");
    cmdStart->addActionListener(mainButtonActionListener);

  	cmdHelp = new gcn::Button("Help");
  	cmdHelp->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
    cmdHelp->setBaseColor(gui_baseCol);
  	cmdHelp->setId("Help");
    cmdHelp->addActionListener(mainButtonActionListener);
    
  	//--------------------------------------------------
    // Create selector entries
  	//--------------------------------------------------
  	int workAreaHeight = GUI_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - DISTANCE_NEXT_Y;
    selectors = new gcn::Container();
    selectors->setSize(150, workAreaHeight - 2);
    selectors->setBaseColor(colSelectorInactive);
    selectors->setFrameSize(1);
  	int panelStartX = DISTANCE_BORDER + selectors->getWidth() + 2 + 11;

  	panelFocusListener = new PanelFocusListener();
    for(i=0; categories[i].category != NULL; ++i)
    {
      categories[i].selector = new gcn::SelectorEntry(categories[i].category, categories[i].imagepath);
      categories[i].selector->setActiveColor(colSelectorActive);
      categories[i].selector->setInactiveColor(colSelectorInactive);
      categories[i].selector->setSize(150, 24);
      categories[i].selector->addFocusListener(panelFocusListener);
      
      categories[i].panel = new gcn::Container();
      categories[i].panel->setId(categories[i].category);
      categories[i].panel->setSize(GUI_WIDTH - panelStartX - DISTANCE_BORDER - 1, workAreaHeight - 2);
      categories[i].panel->setBaseColor(gui_baseCol);
      categories[i].panel->setFrameSize(1);
      categories[i].panel->setVisible(false);
    }

  	//--------------------------------------------------
  	// Initialize panels
  	//--------------------------------------------------
    for(i=0; categories[i].category != NULL; ++i)
    {
      if(categories[i].InitFunc != NULL)
        (*categories[i].InitFunc) (categories[i]);
    }

  	//--------------------------------------------------
    // Place everything on main form
  	//--------------------------------------------------
    gui_top->add(cmdReset, DISTANCE_BORDER, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    gui_top->add(cmdQuit, DISTANCE_BORDER + BUTTON_WIDTH + DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    //gui_top->add(cmdRestart, DISTANCE_BORDER + 2 * BUTTON_WIDTH + 2 * DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    gui_top->add(cmdHelp, DISTANCE_BORDER + 2 * BUTTON_WIDTH + 2 * DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    gui_top->add(cmdStart, GUI_WIDTH - DISTANCE_BORDER - BUTTON_WIDTH, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);

    gui_top->add(selectors, DISTANCE_BORDER + 1, DISTANCE_BORDER + 1);
    for(i=0, yPos=0; categories[i].category != NULL; ++i, yPos += 24)
    {
      selectors->add(categories[i].selector,  0,  yPos);
      gui_top->add(categories[i].panel, panelStartX, DISTANCE_BORDER + 1);
    }

  	//--------------------------------------------------
  	// Activate last active panel
  	//--------------------------------------------------
  	if(!emulating && quickstart_start)
  	  last_active_panel = 1;
  	categories[last_active_panel].selector->requestFocus();
  	cmdHelp->setVisible(categories[last_active_panel].HelpFunc != NULL);
  }


  void gui_halt()
  {
    int i;

    for(i=0; categories[i].category != NULL; ++i)
    {
      if(categories[i].ExitFunc != NULL)
        (*categories[i].ExitFunc) ();
    }

    for(i=0; categories[i].category != NULL; ++i)
      delete categories[i].selector;
    delete panelFocusListener;
    delete selectors;

    delete cmdQuit;
    delete cmdReset;
    delete cmdRestart;
    delete cmdStart;
    delete cmdHelp;
    
    delete mainButtonActionListener;
    
    delete gui_font;
    delete gui_top;
  }
}


void RefreshAllPanels(void)
{
  int i;
  
  for(i=0; categories[i].category != NULL; ++i)
  {
    if(categories[i].RefreshFunc != NULL)
      (*categories[i].RefreshFunc) ();
  }
}


void DisableResume(void)
{
// Disable resume button after loading config is very bad idea
#ifndef ANDROID
  if(emulating)
  {
    widgets::cmdStart->setEnabled(false);
    gcn::Color backCol;
    backCol.r = 128;
    backCol.g = 128;
    backCol.b = 128;
    widgets::cmdStart->setForegroundColor(backCol);
  }
#endif
}


void run_gui(void)
{
#ifdef ANDROIDSDL
  SDL_ANDROID_SetScreenKeyboardShown(0);
  SDL_ANDROID_SetSystemMousePointerVisible(1);
#endif
  gui_running = true;
  gui_rtarea_flags_onenter = gui_create_rtarea_flag(&currprefs);
  
  expansion_generate_autoconfig_info(&changed_prefs);

  try
  {
    sdl::gui_init();
    widgets::gui_init();
    if(_tcslen(startup_message) > 0) {
      ShowMessage(startup_title, startup_message, _T(""), _T("Ok"), _T(""));
      _tcscpy(startup_title, _T(""));
      _tcscpy(startup_message, _T(""));
      widgets::cmdStart->requestFocus();
    }
    sdl::gui_run();
    widgets::gui_halt();
    sdl::gui_halt();
#ifdef ANDROIDSDL
    if (currprefs.onScreen!=0)
    {
       SDL_ANDROID_SetScreenKeyboardShown(1);
       SDL_ANDROID_SetSystemMousePointerVisible(0);
    }
#endif
  }
  // Catch all Guichan exceptions.
  catch (gcn::Exception e)
  {
    std::cout << e.getMessage() << std::endl;
    uae_quit();
  }
  // Catch all Std exceptions.
  catch (std::exception e)
  {
    std::cout << "Std exception: " << e.what() << std::endl;
    uae_quit();
  }
  // Catch all unknown exceptions.
  catch (...)
  {
    std::cout << "Unknown exception" << std::endl;
    uae_quit();
  }

  expansion_generate_autoconfig_info(&changed_prefs);
  cfgfile_compatibility_romtype(&changed_prefs);
  
  if(quit_program > UAE_QUIT || quit_program < -UAE_QUIT)
  {
  	//--------------------------------------------------
    // Prepare everything for Reset of Amiga
  	//--------------------------------------------------
		currprefs.nr_floppies = changed_prefs.nr_floppies;
		screen_is_picasso = 0;
		
		if(gui_rtarea_flags_onenter != gui_create_rtarea_flag(&changed_prefs))
	    quit_program = -UAE_RESET_HARD; // Hardreset required...
  }

  // Reset counter for access violations
  init_max_signals();
}
