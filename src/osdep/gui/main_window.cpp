#ifdef USE_SDL2
#include <guisan.hpp>
#include <SDL_ttf.h>
#include <guisan/sdl.hpp>
#include <guisan/sdl/sdltruetypefont.hpp>
#else
#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#endif
#include "SelectorEntry.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "gui.h"
#include "gui_handling.h"
#include "include/memory-uae.h"
#include "autoconf.h"

#if defined(ANDROIDSDL)
#include "androidsdl_event.h"
#include <SDL_screenkeyboard.h>
#include <SDL_android.h>
#include <android/log.h>
#endif

#ifdef USE_SDL2
extern SDL_Renderer* renderer;
extern SDL_DisplayMode sdlMode;
extern void check_error_sdl(bool check, const char* message);
#endif

bool gui_running = false;
static int last_active_panel = 2;

#define MAX_STARTUP_TITLE 64
#define MAX_STARTUP_MESSAGE 256
static TCHAR startup_title[MAX_STARTUP_TITLE] = _T("");
static TCHAR startup_message[MAX_STARTUP_MESSAGE] = _T("");
char last_active_config[MAX_PATH] = { '\0' };


void SetLastActiveConfig(const char *filename)
{
  extractFileName(filename, last_active_config);
  removeFileExtension(last_active_config);
}


void target_startup_msg(const TCHAR* title, const TCHAR* msg)
{
  _tcsncpy(startup_title, title, MAX_STARTUP_TITLE);
  _tcsncpy(startup_message, msg, MAX_STARTUP_MESSAGE);
}


ConfigCategory categories[] = {
  { "Paths",            "data/paths.ico",     NULL, NULL, InitPanelPaths,     ExitPanelPaths,     HelpPanelPaths },
  { "Quickstart",       "data/quickstart.ico",  NULL, NULL, InitPanelQuickstart,  ExitPanelQuickstart,  HelpPanelQuickstart },
  { "Configurations",   "data/file.ico",      NULL, NULL, InitPanelConfig,    ExitPanelConfig,    HelpPanelConfig },
  { "CPU and FPU",      "data/cpu.ico",       NULL, NULL, InitPanelCPU,       ExitPanelCPU,       HelpPanelCPU },
  { "Chipset",          "data/cpu.ico",       NULL, NULL, InitPanelChipset,   ExitPanelChipset,   HelpPanelChipset },
  { "ROM",              "data/chip.ico",      NULL, NULL, InitPanelROM,       ExitPanelROM,       HelpPanelROM },
  { "RAM",              "data/chip.ico",      NULL, NULL, InitPanelRAM,       ExitPanelRAM,       HelpPanelRAM },
  { "Floppy drives",    "data/35floppy.ico",  NULL, NULL, InitPanelFloppy,    ExitPanelFloppy,    HelpPanelFloppy },
  { "Hard drives / CD", "data/drive.ico",     NULL, NULL, InitPanelHD,        ExitPanelHD,        HelpPanelHD },
  { "Display",          "data/screen.ico",    NULL, NULL, InitPanelDisplay,   ExitPanelDisplay,   HelpPanelDisplay },
  { "Sound",            "data/sound.ico",     NULL, NULL, InitPanelSound,     ExitPanelSound,     HelpPanelSound },
  { "Game ports",       "data/joystick.ico",  NULL, NULL, InitPanelGamePort,  ExitPanelGamePort,  HelpPanelGamePort },
  { "Input",            "data/joystick.ico",  NULL, NULL, InitPanelInput,     ExitPanelInput,     HelpPanelInput },
  { "Miscellaneous",    "data/misc.ico",      NULL, NULL, InitPanelMisc,      ExitPanelMisc,      HelpPanelMisc },
  { "Savestates",       "data/savestate.png", NULL, NULL, InitPanelSavestate, ExitPanelSavestate, HelpPanelSavestate },
#ifdef ANDROIDSDL  
  { "OnScreen",         "data/screen.ico",    NULL, NULL, InitPanelOnScreen,  ExitPanelOnScreen,  HelpPanelOnScreen },
#endif
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};
#ifdef ANDROIDSDL
enum { PANEL_PATHS, PANEL_QUICKSTART, PANEL_CONFIGURATIONS, PANEL_CPU, PANEL_CHIPSET, PANEL_ROM, PANEL_RAM,
       PANEL_FLOPPY, PANEL_HD, PANEL_DISPLAY, PANEL_SOUND, PANEL_GAMEPORT, PANEL_INPUT, PANEL_MISC, PANEL_SAVESTATES, 
       PANEL_ONSCREEN, NUM_PANELS };
#else
enum { PANEL_PATHS, PANEL_QUICKSTART, PANEL_CONFIGURATIONS, PANEL_CPU, PANEL_CHIPSET, PANEL_ROM, PANEL_RAM,
       PANEL_FLOPPY, PANEL_HD, PANEL_DISPLAY, PANEL_SOUND, PANEL_GAMEPORT, PANEL_INPUT, PANEL_MISC, PANEL_SAVESTATES, 
       NUM_PANELS };
#endif

#ifdef USE_SDL2
SDL_Texture* gui_texture;
SDL_Cursor* cursor;
SDL_Surface* cursorSurface;
gcn::SDLTrueTypeFont* gui_font;
#else
gcn::contrib::SDLTrueTypeFont* gui_font;
#endif

gcn::Container* gui_top;
gcn::Container* selectors;
gcn::Gui* uae_gui;
gcn::Color gui_baseCol;
gcn::Color gui_baseColLabel;
gcn::Color colSelectorInactive;
gcn::Color colSelectorActive;
SDL_Surface* gui_screen;
gcn::SDLGraphics* gui_graphics;
gcn::SDLInput* gui_input;
gcn::SDLImageLoader* gui_imageLoader;
SDL_Event gui_event;

namespace widgets 
{
  // Main buttons
  gcn::Button* cmdQuit;
  gcn::Button* cmdReset;
  gcn::Button* cmdRestart;
  gcn::Button* cmdStart;
  gcn::Button* cmdHelp;
	gcn::Button* cmdShutdown;
}


static int count_HDs(struct uae_prefs *p)
{
  return p->mountitems;
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

#ifndef USE_SDL2
void FocusBugWorkaround(gcn::Window *wnd)
{
  // When modal dialog opens via mouse, the dialog will not
	// have the focus unless there is a mouse click. We simulate the click...
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
#endif


static void ShowHelpRequested()
{
  std::vector<std::string> helptext;
  if(categories[last_active_panel].HelpFunc != NULL && categories[last_active_panel].HelpFunc(helptext))
  {
    //------------------------------------------------
    // Show help for current panel
    //------------------------------------------------
    char title[128];
    snprintf(title, 127, "Help for %s", categories[last_active_panel].category);
    ShowHelp(title, helptext);
  }
}

#ifdef USE_SDL2
void UpdateGuiScreen()
{
	// Update the texture from the surface
	SDL_UpdateTexture(gui_texture, nullptr, gui_screen->pixels, gui_screen->pitch);
	// Copy the texture on the renderer
	SDL_RenderCopy(renderer, gui_texture, nullptr, nullptr);
	// Update the window surface (show the renderer)
	SDL_RenderPresent(renderer);
}
#endif

namespace sdl
{
#ifdef USE_SDL2
	// Sets the cursor image up
	void setup_cursor() 
	{
		// Detect resolution and load appropiate cursor image
		if (sdlMode.w > 1280)
		{
			cursorSurface = SDL_LoadBMP("data/cursor-x2.bmp");
		}
		else
		{
			cursorSurface = SDL_LoadBMP("data/cursor.bmp");
		}
		
		if (cursorSurface == nullptr)
		{
			// Load failed. Log error.
			printf("Could not load cursor bitmap: %d\n", SDL_GetError());
			return;
		}

		// Create new cursor with surface
		cursor = SDL_CreateColorCursor(cursorSurface, 0, 0);
		if (cursor == nullptr)
		{
			// Cursor creation failed. Log error and free surface
			printf("Could not create color cursor: %d\n", SDL_GetError());
			SDL_FreeSurface(cursorSurface);
			cursorSurface = nullptr;
			return;
		}

		SDL_SetCursor(cursor);
	}
#endif

  void gui_init()
  {
    //-------------------------------------------------
    // Set layer for GUI screen
    //-------------------------------------------------
#ifdef PANDORA
  	char tmp[21];
  	snprintf(tmp, 20, "%dx%d", GUI_WIDTH, GUI_HEIGHT);
  	setenv("SDL_OMAP_LAYER_SIZE", tmp, 1);
  	snprintf(tmp, 20, "0,0,0,0");
  	setenv("SDL_OMAP_BORDER_CUT", tmp, 1);
#endif

    //-------------------------------------------------
    // Create new screen for GUI
    //-------------------------------------------------
#ifdef USE_SDL2
		setup_cursor();

		// make the scaled rendering look smoother (linear scaling).
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

		gui_screen = SDL_CreateRGBSurface(0, GUI_WIDTH, GUI_HEIGHT, 32, 0, 0, 0, 0);
		check_error_sdl(gui_screen == NULL, "Unable to create a surface");

		SDL_RenderSetLogicalSize(renderer, GUI_WIDTH, GUI_HEIGHT);

		gui_texture = SDL_CreateTextureFromSurface(renderer, gui_screen);
		check_error_sdl(gui_texture == NULL, "Unable to create texture");

		if (cursor)
		{
			SDL_ShowCursor(SDL_ENABLE);
		}
#else
		gui_screen = SDL_SetVideoMode(GUI_WIDTH, GUI_HEIGHT, 16, SDL_SWSURFACE | SDL_FULLSCREEN);
    SDL_EnableUNICODE(1);
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
#ifdef ANDROIDSDL
    // Enable Android multitouch
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickOpen(0);
#endif
    SDL_ShowCursor(SDL_ENABLE);
#endif

    //-------------------------------------------------
    // Create helpers for guichan/guisan
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
    
    if(gui_screen != NULL) {
      SDL_FreeSurface(gui_screen);
      gui_screen = NULL;
    }
#ifdef USE_SDL2
		SDL_DestroyTexture(gui_texture);
		gui_texture = NULL;

		if (cursor)
		{
			SDL_FreeCursor(cursor);
			cursor = nullptr;
		}
		if (cursorSurface)
		{
			SDL_FreeSurface(cursorSurface);
			cursorSurface = nullptr;
		}
#endif
  }

	void checkInput()
	{
    while(SDL_PollEvent(&gui_event))
    {
  		if (gui_event.type == SDL_QUIT) {
        //-------------------------------------------------
        // Quit entire program via SQL-Quit
        //-------------------------------------------------
  			uae_quit();
  			gui_running = false;
  			break;
#ifndef ANDROIDSDL
  		} else if (gui_event.type == SDL_JOYAXISMOTION) {
        //-------------------------------------------------
        // Navigate with joystick
        //-------------------------------------------------
        const int trigger = 1000;
        SDLKey sym = SDLK_UNKNOWN;
        if(gui_event.jaxis.axis) {
          if(gui_event.jaxis.value > trigger) {
            sym = VK_DOWN;
          } else if(gui_event.jaxis.value < -trigger) {
            sym = VK_UP;
          }
        } else {
          if(gui_event.jaxis.value > trigger) {
            sym = VK_RIGHT;
          } else if(gui_event.jaxis.value < -trigger) {
            sym = VK_LEFT;
          }
        }
        if(sym != SDLK_UNKNOWN) {
          SDL_Event newEventDown;
          newEventDown.type = SDL_KEYDOWN;
          newEventDown.key.keysym.sym = sym;
          newEventDown.key.state = SDL_PRESSED;
          SDL_PushEvent(&newEventDown); // Fire key down

          SDL_Event newEventUp;
          newEventUp.type = SDL_KEYUP;  
          newEventUp.key.keysym.sym = sym;
          newEventUp.key.state = SDL_RELEASED;
          SDL_PushEvent(&newEventUp); // and the key up
        }

  		} else if (gui_event.type == SDL_JOYBUTTONDOWN) {
        //-------------------------------------------------
        // Enter/select with joystick (any button)
        //-------------------------------------------------
        gui_event.type = SDL_KEYDOWN;
        gui_event.key.keysym.sym = SDLK_RETURN;
        gui_input->pushInput(gui_event); // Fire key down
        gui_event.type = SDL_KEYUP;  // and the key up
#endif          
  		} else if (gui_event.type == SDL_KEYDOWN) {
        gcn::FocusHandler* focusHdl;
        gcn::Widget* activeWidget;
          
#ifndef PANDORA
				if (gui_event.key.keysym.sym == SDLK_F12)
#else
				if (gui_event.key.keysym.sym == SDLK_LCTRL)
#endif
				{
			    if(emulating && widgets::cmdStart->isEnabled()) {
            //------------------------------------------------
            // Continue emulation
            //------------------------------------------------
            gui_running = false;
		      } else {
            //------------------------------------------------
            // First start of emulator -> reset Amiga
            //------------------------------------------------
      			uae_reset(0, 1);
      			gui_running = false;
          }
        } else {
          switch(gui_event.key.keysym.sym)
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

            case VK_ESCAPE:
            case VK_R:
              //-------------------------------------------------
              // Reset Amiga
              //-------------------------------------------------
        			uae_reset(1, 1);
        			gui_running = false;
        			break;
#endif
					  case VK_X:
					  case VK_A:
              //------------------------------------------------
              // Simulate press of enter when 'X' pressed
              //------------------------------------------------
              gui_event.key.keysym.sym = SDLK_RETURN;
              gui_input->pushInput(gui_event); // Fire key down
              gui_event.type = SDL_KEYUP;  // and the key up
              break;

            case VK_UP:
              if(HandleNavigation(DIRECTION_UP))
                continue; // Don't change value when enter ComboBox -> don't send event to control
              break;
              
            case VK_DOWN:
              if(HandleNavigation(DIRECTION_DOWN))
                continue; // Don't change value when enter ComboBox -> don't send event to control
              break;
              
            case VK_LEFT:
              if(HandleNavigation(DIRECTION_LEFT))
                continue; // Don't change value when enter Slider -> don't send event to control
              break;
              
            case VK_RIGHT:
              if(HandleNavigation(DIRECTION_RIGHT))
                continue; // Don't change value when enter Slider -> don't send event to control
              break;
          
            case SDLK_F1:
              ShowHelpRequested();
              widgets::cmdHelp->requestFocus();
              break;
          }
        }
      }

      //-------------------------------------------------
      // Send event to guichan/guisan-controls
      //-------------------------------------------------
#ifdef ANDROIDSDL
        androidsdl_event(gui_event, gui_input);
#else
        gui_input->pushInput(gui_event);
#endif
    }
  }

  void gui_run()
  {
    //-------------------------------------------------
    // The main loop
    //-------------------------------------------------
    while(gui_running)
    {
			// Poll input
			checkInput();

  		if(gui_rtarea_flags_onenter != gui_create_rtarea_flag(&workprefs))
        DisableResume();

      // Now we let the Gui object perform its logic.
      uae_gui->logic();
      // Now we let the Gui object draw itself.
      uae_gui->draw();
      // Finally we update the screen.
      wait_for_vsync();
#ifdef USE_SDL2
			UpdateGuiScreen();
#else
      SDL_Flip(gui_screen);
#endif
      
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
	      if (actionEvent.getSource() == cmdShutdown) {
				  // ------------------------------------------------
				  // Shutdown the host (power off)
				  // ------------------------------------------------
				  uae_quit();
				  gui_running = false;
				  host_poweroff = true;

			  } else if (actionEvent.getSource() == cmdQuit) {
          //-------------------------------------------------
          // Quit entire program via click on Quit-button
          //-------------------------------------------------
	        uae_quit();
  			  gui_running = false;

  			}	else if(actionEvent.getSource() == cmdReset) {
          //-------------------------------------------------
          // Reset Amiga via click on Reset-button
          //-------------------------------------------------
    			uae_reset(1, 1);
    			gui_running = false;

        } else if(actionEvent.getSource() == cmdRestart) {
          //-------------------------------------------------
          // Restart emulator
          //-------------------------------------------------
          char tmp[MAX_PATH];
          fetch_configurationpath (tmp, sizeof (tmp));
          if(strlen(last_loaded_config) > 0)
            strncat (tmp, last_loaded_config, MAX_PATH - 1);
          else {
            strncat (tmp, OPTIONSFILENAME, MAX_PATH - 1);
            strncat (tmp, ".uae", MAX_PATH - 1);
          }
        	uae_restart(-1, tmp);
    			gui_running = false;

			  } else if(actionEvent.getSource() == cmdStart) {
			    if(emulating && widgets::cmdStart->isEnabled()) {
            //------------------------------------------------
            // Continue emulation
            //------------------------------------------------
            gui_running = false;
		      } else {
            //------------------------------------------------
            // First start of emulator -> reset Amiga
            //------------------------------------------------
      			uae_reset(0, 1);
      			gui_running = false;
          }

        } else if(actionEvent.getSource() == cmdHelp) {
          ShowHelpRequested();
          cmdHelp->requestFocus();
        }
      }
  };
  static MainButtonActionListener* mainButtonActionListener;


  class PanelFocusListener : public gcn::FocusListener
  {
    public:
      void focusGained(const gcn::Event& event)
      {
        int i;
        int activate = -1;
        int deactivate = -1;
        
        for(i = 0; categories[i].category != NULL; ++i) {
          if(event.getSource() == categories[i].selector)
            activate = i;
          else if(categories[i].panel->isVisible())
            deactivate = i;
        }
        if (activate >= 0) {
          categories[activate].selector->setActive(true);
          if(categories[activate].InitFunc != NULL)
            (*categories[activate].InitFunc) (categories[activate]);
          categories[activate].panel->setVisible(true);
          last_active_panel = activate;
          cmdHelp->setVisible(categories[activate].HelpFunc != NULL);
        }
        if (deactivate >= 0) {
          categories[deactivate].selector->setActive(false);
          categories[deactivate].panel->setVisible(false);
          (*categories[deactivate].ExitFunc)(categories[deactivate]);
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
#ifdef USE_SDL2
		gui_top->setDimension(gcn::Rectangle(0, 0, GUI_WIDTH, GUI_HEIGHT));
#else
    gui_top->setDimension(gcn::Rectangle((gui_screen->w - GUI_WIDTH) / 2, (gui_screen->h - GUI_HEIGHT) / 2, GUI_WIDTH, GUI_HEIGHT));
#endif
    gui_top->setBaseColor(gui_baseCol);
    uae_gui->setTop(gui_top);

    //-------------------------------------------------
    // Initialize fonts
    //-------------------------------------------------
	  TTF_Init();
#ifdef USE_SDL2
		gui_font = new gcn::SDLTrueTypeFont("data/FreeSans.ttf", 14);
#else
#ifdef ANDROID
	  gui_font = new gcn::contrib::SDLTrueTypeFont("data/FreeSans.ttf", 16);	  
#else
	  gui_font = new gcn::contrib::SDLTrueTypeFont("data/FreeSans.ttf", 14);
#endif
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

	  cmdShutdown = new gcn::Button("Shutdown");
	  cmdShutdown->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	  cmdShutdown->setBaseColor(gui_baseCol);
	  cmdShutdown->setId("Shutdown");
	  cmdShutdown->addActionListener(mainButtonActionListener);

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
#ifdef USE_SDL2
		selectors->setBorderSize(1);
#else
    selectors->setFrameSize(1);
#endif
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
#ifdef USE_SDL2
			categories[i].panel->setBorderSize(1);
#else
      categories[i].panel->setFrameSize(1);
#endif
      categories[i].panel->setVisible(false);
    }

  	//--------------------------------------------------
    // Place everything on main form
  	//--------------------------------------------------
    gui_top->add(cmdReset, DISTANCE_BORDER, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    gui_top->add(cmdQuit, DISTANCE_BORDER + BUTTON_WIDTH + DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
#ifndef ANDROID
    gui_top->add(cmdShutdown, DISTANCE_BORDER + 2 * BUTTON_WIDTH + 2 * DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
#endif
    gui_top->add(cmdHelp, DISTANCE_BORDER + 3 * BUTTON_WIDTH + 3 * DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    if(emulating)
      gui_top->add(cmdRestart, GUI_WIDTH - DISTANCE_BORDER - 2 * BUTTON_WIDTH - DISTANCE_NEXT_X, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);
    gui_top->add(cmdStart, GUI_WIDTH - DISTANCE_BORDER - BUTTON_WIDTH, GUI_HEIGHT - DISTANCE_BORDER - BUTTON_HEIGHT);

    gui_top->add(selectors, DISTANCE_BORDER + 1, DISTANCE_BORDER + 1);
    for(i=0, yPos=0; categories[i].category != NULL; ++i, yPos += 24)
    {
      selectors->add(categories[i].selector,  0,  yPos);
      gui_top->add(categories[i].panel, panelStartX, DISTANCE_BORDER + 1);
    }

  	//--------------------------------------------------
  	// Init list of configurations
  	//--------------------------------------------------
    ReadConfigFileList();

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

    (*categories[last_active_panel].ExitFunc)(categories[last_active_panel]);

    for(i=0; categories[i].category != NULL; ++i)
      delete categories[i].selector;
    delete panelFocusListener;
    delete selectors;

    delete cmdQuit;
	  delete cmdShutdown;
    delete cmdReset;
    delete cmdRestart;
    delete cmdStart;
    delete cmdHelp;
    
    delete mainButtonActionListener;
    
    delete gui_font;
    delete gui_top;
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
  // Catch all Guichan/Guisan exceptions.
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

  cfgfile_compatibility_romtype(&workprefs);
  
  if(quit_program > UAE_QUIT || quit_program < -UAE_QUIT)
  {
  	//--------------------------------------------------
    // Prepare everything for Reset of Amiga
  	//--------------------------------------------------
		currprefs.nr_floppies = workprefs.nr_floppies;
		screen_is_picasso = 0;
		
		if(gui_rtarea_flags_onenter != gui_create_rtarea_flag(&workprefs))
	    quit_program = -UAE_RESET_HARD; // Hardreset required...
  }

  // Reset counter for access violations
  init_max_signals();
}
