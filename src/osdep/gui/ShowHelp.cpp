#include <algorithm>
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
#include <iostream>
#include <sstream>
#include "SelectorEntry.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "gui.h"
#include "gui_handling.h"
#include "GenericListModel.h"

#ifdef ANDROIDSDL
#include "androidsdl_event.h"
#endif

#define DIALOG_WIDTH 760
#define DIALOG_HEIGHT 420

static bool dialogFinished = false;

static gcn::Window *wndShowHelp;
static gcn::Button* cmdOK;
static gcn::ListBox* lstHelp;
static gcn::ScrollArea* scrAreaHelp;


static gcn::GenericListModel *helpList;


class ShowHelpActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      dialogFinished = true;
    }
};
static ShowHelpActionListener* showHelpActionListener;


static void InitShowHelp(const std::vector<std::string>& helptext)
{
	wndShowHelp = new gcn::Window("Help");
	wndShowHelp->setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
  wndShowHelp->setPosition((GUI_WIDTH - DIALOG_WIDTH) / 2, (GUI_HEIGHT - DIALOG_HEIGHT) / 2);
  wndShowHelp->setBaseColor(gui_baseCol + 0x202020);
  wndShowHelp->setTitleBarHeight(TITLEBAR_HEIGHT);

  showHelpActionListener = new ShowHelpActionListener();

  helpList = new gcn::GenericListModel(helptext);

  lstHelp = new gcn::ListBox(helpList);
  lstHelp->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4, DIALOG_HEIGHT - 3 * DISTANCE_BORDER - BUTTON_HEIGHT - DISTANCE_NEXT_Y - 10);
  lstHelp->setPosition(DISTANCE_BORDER, DISTANCE_BORDER);
  lstHelp->setBaseColor(gui_baseCol + 0x202020);
  lstHelp->setBackgroundColor(gui_baseCol + 0x202020);
  lstHelp->setWrappingEnabled(true);

  scrAreaHelp = new gcn::ScrollArea(lstHelp);
#ifdef USE_SDL2
	scrAreaHelp->setBorderSize(1);
#else
  scrAreaHelp->setFrameSize(1);
#endif
  scrAreaHelp->setPosition(DISTANCE_BORDER, 10 + TEXTFIELD_HEIGHT + 10);
  scrAreaHelp->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4, DIALOG_HEIGHT - 3 * DISTANCE_BORDER - BUTTON_HEIGHT - DISTANCE_NEXT_Y - 10);
  scrAreaHelp->setScrollbarWidth(20);
  scrAreaHelp->setBaseColor(gui_baseCol + 0x202020);
  scrAreaHelp->setBackgroundColor(gui_baseCol + 0x202020);

	cmdOK = new gcn::Button("Ok");
	cmdOK->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	cmdOK->setPosition(DIALOG_WIDTH - DISTANCE_BORDER - BUTTON_WIDTH, DIALOG_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - 10);
  cmdOK->setBaseColor(gui_baseCol + 0x202020);
  cmdOK->addActionListener(showHelpActionListener);
  
  wndShowHelp->add(scrAreaHelp, DISTANCE_BORDER, DISTANCE_BORDER);
  wndShowHelp->add(cmdOK);

  gui_top->add(wndShowHelp);
  
  cmdOK->requestFocus();
  wndShowHelp->requestModalFocus();
}


static void ExitShowHelp(void)
{
  wndShowHelp->releaseModalFocus();
  gui_top->remove(wndShowHelp);

  delete lstHelp;
  delete scrAreaHelp;
  delete cmdOK;
  
  delete helpList;
  delete showHelpActionListener;

  delete wndShowHelp;
}


static void ShowHelpLoop(void)
{
#ifndef USE_SDL2
  FocusBugWorkaround(wndShowHelp);  
#endif

  while(!dialogFinished)
  {
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      if (event.type == SDL_KEYDOWN)
      {
        switch(event.key.keysym.sym)
        {
          case SDLK_ESCAPE:
            dialogFinished = true;
            break;
            
          case SDLK_PAGEDOWN:
          case SDLK_HOME:
            event.key.keysym.sym = SDLK_RETURN;
            gui_input->pushInput(event); // Fire key down
            event.type = SDL_KEYUP;  // and the key up
            break;
				  default:
					  break;
        }
      }

      //-------------------------------------------------
      // Send event to guichan/guisan-controls
      //-------------------------------------------------
#ifdef ANDROIDSDL
        androidsdl_event(event, gui_input);
#else
        gui_input->pushInput(event);
#endif
    }

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
  }  
}


void ShowHelp(const char *title, const std::vector<std::string>& text)
{
  dialogFinished = false;

  InitShowHelp(text);
  
  wndShowHelp->setCaption(title);
  cmdOK->setCaption("Ok");
  ShowHelpLoop();
  ExitShowHelp();
}
