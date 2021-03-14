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


#define DIALOG_WIDTH 600
#define DIALOG_HEIGHT 460

static bool dialogFinished = false;

static gcn::Window *wndShowDiskInfo;
static gcn::Button* cmdOK;
static gcn::ListBox* lstInfo;
static gcn::ScrollArea* scrAreaInfo;


static gcn::GenericListModel *infoList;


class ShowDiskInfoActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      dialogFinished = true;
    }
};
static ShowDiskInfoActionListener* showDiskInfoActionListener;


static void InitShowDiskInfo(const std::vector<std::string>& infotext)
{
	wndShowDiskInfo = new gcn::Window("DiskInfo");
	wndShowDiskInfo->setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
  wndShowDiskInfo->setPosition((GUI_WIDTH - DIALOG_WIDTH) / 2, (GUI_HEIGHT - DIALOG_HEIGHT) / 2);
  wndShowDiskInfo->setBaseColor(gui_baseCol + 0x202020);
  wndShowDiskInfo->setTitleBarHeight(TITLEBAR_HEIGHT);

  showDiskInfoActionListener = new ShowDiskInfoActionListener();

  infoList = new gcn::GenericListModel(infotext);

  lstInfo = new gcn::ListBox(infoList);
  lstInfo->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4 - 20, DIALOG_HEIGHT - 3 * DISTANCE_BORDER - BUTTON_HEIGHT - DISTANCE_NEXT_Y - 10);
  lstInfo->setPosition(DISTANCE_BORDER, DISTANCE_BORDER);
  lstInfo->setBaseColor(gui_baseCol + 0x202020);
  lstInfo->setBackgroundColor(gui_baseCol + 0x202020);
  lstInfo->setWrappingEnabled(true);
  lstInfo->setFont(gui_fixedfont);
  
  scrAreaInfo = new gcn::ScrollArea(lstInfo);
#ifdef USE_SDL2
	scrAreaInfo->setBorderSize(1);
#else
  scrAreaInfo->setFrameSize(1);
#endif
  scrAreaInfo->setPosition(DISTANCE_BORDER, 10 + TEXTFIELD_HEIGHT + 10);
  scrAreaInfo->setSize(DIALOG_WIDTH - 2 * DISTANCE_BORDER - 4, DIALOG_HEIGHT - 3 * DISTANCE_BORDER - BUTTON_HEIGHT - DISTANCE_NEXT_Y - 10);
  scrAreaInfo->setScrollbarWidth(20);
  scrAreaInfo->setBaseColor(gui_baseCol + 0x202020);
  scrAreaInfo->setBackgroundColor(gui_baseCol + 0x202020);

	cmdOK = new gcn::Button("Ok");
	cmdOK->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	cmdOK->setPosition(DIALOG_WIDTH - DISTANCE_BORDER - BUTTON_WIDTH, DIALOG_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - 10);
  cmdOK->setBaseColor(gui_baseCol + 0x202020);
  cmdOK->addActionListener(showDiskInfoActionListener);
  
  wndShowDiskInfo->add(scrAreaInfo, DISTANCE_BORDER, DISTANCE_BORDER);
  wndShowDiskInfo->add(cmdOK);

  gui_top->add(wndShowDiskInfo);
  
  cmdOK->requestFocus();
  wndShowDiskInfo->requestModalFocus();
}


static void ExitShowDiskInfo(void)
{
  wndShowDiskInfo->releaseModalFocus();
  gui_top->remove(wndShowDiskInfo);

  delete lstInfo;
  delete scrAreaInfo;
  delete cmdOK;
  
  delete infoList;
  delete showDiskInfoActionListener;

  delete wndShowDiskInfo;
}


static void ShowDiskInfoLoop(void)
{
#ifndef USE_SDL2
  FocusBugWorkaround(wndShowDiskInfo);  
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
      gui_input->pushInput(event);
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


void ShowDiskInfo(const char *title, const std::vector<std::string>& text)
{
  dialogFinished = false;

  InitShowDiskInfo(text);
  
  wndShowDiskInfo->setCaption(title);
  cmdOK->setCaption("Ok");
  ShowDiskInfoLoop();
  ExitShowDiskInfo();
}
