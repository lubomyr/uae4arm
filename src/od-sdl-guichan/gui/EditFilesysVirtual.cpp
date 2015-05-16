#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#include "SelectorEntry.hpp"
#include "UaeRadioButton.hpp"
#include "UaeDropDown.hpp"
#include "UaeCheckBox.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "memory-uae.h"
#include "uae.h"
#include "autoconf.h"
#include "filesys.h"
#include "gui.h"
#include "target.h"
#include "gui_handling.h"


#define DIALOG_WIDTH 520
#define DIALOG_HEIGHT 202

static bool dialogResult = false;
static bool dialogFinished = false;

static gcn::Window *wndEditFilesysVirtual;
static gcn::Button* cmdOK;
static gcn::Button* cmdCancel;
static gcn::Label *lblDevice;
static gcn::TextField *txtDevice;
static gcn::Label *lblVolume;
static gcn::TextField *txtVolume;
static gcn::Label *lblPath;
static gcn::TextField *txtPath;
static gcn::Button* cmdPath;
static gcn::UaeCheckBox* chkReadWrite;
static gcn::Label *lblBootPri;
static gcn::TextField *txtBootPri;


class FilesysVirtualActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if(actionEvent.getSource() == cmdPath)
      {
        char tmp[MAX_PATH];
        strncpy(tmp, txtPath->getText().c_str(), MAX_PATH);
        wndEditFilesysVirtual->releaseModalFocus();
        if(SelectFolder("Select folder", tmp))
          txtPath->setText(tmp);
        wndEditFilesysVirtual->requestModalFocus();
        cmdPath->requestFocus();
      }
      else
      {
        if (actionEvent.getSource() == cmdOK)
        {
          if(txtDevice->getText().length() <= 0)
          {
            // ToDo: Message to user
            return;
          }
          if(txtVolume->getText().length() <= 0)
          {
            // ToDo: Message to user
            return;
          }
          dialogResult = true;
        }
        dialogFinished = true;
      }
    }
};
static FilesysVirtualActionListener* filesysVirtualActionListener;


static void InitEditFilesysVirtual(void)
{
	wndEditFilesysVirtual = new gcn::Window("Edit");
	wndEditFilesysVirtual->setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
  wndEditFilesysVirtual->setPosition((GUI_WIDTH - DIALOG_WIDTH) / 2, (GUI_HEIGHT - DIALOG_HEIGHT) / 2);
  wndEditFilesysVirtual->setBaseColor(gui_baseCol + 0x202020);
  wndEditFilesysVirtual->setCaption("Volume settings");
  wndEditFilesysVirtual->setTitleBarHeight(TITLEBAR_HEIGHT);
  
  filesysVirtualActionListener = new FilesysVirtualActionListener();
  
	cmdOK = new gcn::Button("Ok");
	cmdOK->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	cmdOK->setPosition(DIALOG_WIDTH - DISTANCE_BORDER - 2 * BUTTON_WIDTH - DISTANCE_NEXT_X, DIALOG_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - 10);
  cmdOK->setBaseColor(gui_baseCol + 0x202020);
  cmdOK->setId("virtOK");
  cmdOK->addActionListener(filesysVirtualActionListener);
  
	cmdCancel = new gcn::Button("Cancel");
	cmdCancel->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
	cmdCancel->setPosition(DIALOG_WIDTH - DISTANCE_BORDER - BUTTON_WIDTH, DIALOG_HEIGHT - 2 * DISTANCE_BORDER - BUTTON_HEIGHT - 10);
  cmdCancel->setBaseColor(gui_baseCol + 0x202020);
  cmdCancel->setId("virtCancel");
  cmdCancel->addActionListener(filesysVirtualActionListener);

  lblDevice = new gcn::Label("Device Name:");
  lblDevice->setSize(100, LABEL_HEIGHT);
  lblDevice->setAlignment(gcn::Graphics::RIGHT);
  txtDevice = new gcn::TextField();
  txtDevice->setSize(80, TEXTFIELD_HEIGHT);
  txtDevice->setId("virtDev");

  lblVolume = new gcn::Label("Volume Label:");
  lblVolume->setSize(100, LABEL_HEIGHT);
  lblVolume->setAlignment(gcn::Graphics::RIGHT);
  txtVolume = new gcn::TextField();
  txtVolume->setSize(80, TEXTFIELD_HEIGHT);
  txtVolume->setId("virtVol");

  lblPath = new gcn::Label("Path:");
  lblPath->setSize(100, LABEL_HEIGHT);
  lblPath->setAlignment(gcn::Graphics::RIGHT);
  txtPath = new gcn::TextField();
  txtPath->setSize(338, TEXTFIELD_HEIGHT);
  txtPath->setEnabled(false);
  cmdPath = new gcn::Button("...");
  cmdPath->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
  cmdPath->setBaseColor(gui_baseCol + 0x202020);
  cmdPath->setId("virtPath");
  cmdPath->addActionListener(filesysVirtualActionListener);

	chkReadWrite = new gcn::UaeCheckBox("Read/Write", true);
  chkReadWrite->setId("virtRW");

  lblBootPri = new gcn::Label("Boot priority:");
  lblBootPri->setSize(84, LABEL_HEIGHT);
  lblBootPri->setAlignment(gcn::Graphics::RIGHT);
  txtBootPri = new gcn::TextField();
  txtBootPri->setSize(40, TEXTFIELD_HEIGHT);
  txtBootPri->setId("virtBootpri");
  
  int posY = DISTANCE_BORDER;
  wndEditFilesysVirtual->add(lblDevice, DISTANCE_BORDER, posY);
  wndEditFilesysVirtual->add(txtDevice, DISTANCE_BORDER + lblDevice->getWidth() + 8, posY);
  wndEditFilesysVirtual->add(chkReadWrite, 260, posY);
  posY += txtDevice->getHeight() + DISTANCE_NEXT_Y;
  wndEditFilesysVirtual->add(lblVolume, DISTANCE_BORDER, posY);
  wndEditFilesysVirtual->add(txtVolume, DISTANCE_BORDER + lblDevice->getWidth() + 8, posY);
  wndEditFilesysVirtual->add(lblBootPri, 260, posY);
  wndEditFilesysVirtual->add(txtBootPri, 260 + lblBootPri->getWidth() + 8, posY);
  posY += txtDevice->getHeight() + DISTANCE_NEXT_Y;
  wndEditFilesysVirtual->add(lblPath, DISTANCE_BORDER, posY);
  wndEditFilesysVirtual->add(txtPath, DISTANCE_BORDER + lblDevice->getWidth() + 8, posY);
  wndEditFilesysVirtual->add(cmdPath, wndEditFilesysVirtual->getWidth() - DISTANCE_BORDER - SMALL_BUTTON_WIDTH, posY);
  posY += txtDevice->getHeight() + DISTANCE_NEXT_Y;

  wndEditFilesysVirtual->add(cmdOK);
  wndEditFilesysVirtual->add(cmdCancel);

  gui_top->add(wndEditFilesysVirtual);
  
  txtDevice->requestFocus();
  wndEditFilesysVirtual->requestModalFocus();
}


static void ExitEditFilesysVirtual(void)
{
  wndEditFilesysVirtual->releaseModalFocus();
  gui_top->remove(wndEditFilesysVirtual);

  delete lblDevice;
  delete txtDevice;
  delete lblVolume;
  delete txtVolume;
  delete lblPath;
  delete txtPath;
  delete cmdPath;
  delete chkReadWrite;
  delete lblBootPri;
  delete txtBootPri;
  
  delete cmdOK;
  delete cmdCancel;
  delete filesysVirtualActionListener;
  
  delete wndEditFilesysVirtual;
}


static void EditFilesysVirtualLoop(void)
{
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

          case SDLK_PAGEDOWN:
          case SDLK_HOME:
            event.key.keysym.sym = SDLK_RETURN;
            gui_input->pushInput(event); // Fire key down
            event.type = SDL_KEYUP;  // and the key up
            break;
        }
      }

      //-------------------------------------------------
      // Send event to guichan-controls
      //-------------------------------------------------
#ifdef ANDROID
            /*
             * Now that we are done polling and using SDL events we pass
             * the leftovers to the SDLInput object to later be handled by
             * the Gui. (This example doesn't require us to do this 'cause a
             * label doesn't use input. But will do it anyway to show how to
             * set up an SDL application with Guichan.)
             */
            if (event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP) {
                // Filter emulated mouse events for Guichan, we wand absolute input
            } else {
                // Convert multitouch event to SDL mouse event
                static int x = 0, y = 0, buttons = 0, wx=0, wy=0, pr=0;
                SDL_Event event2;
                memcpy(&event2, &event, sizeof(event));
                if (event.type == SDL_JOYBALLMOTION &&
                    event.jball.which == 0 &&
                    event.jball.ball == 0) {
                    event2.type = SDL_MOUSEMOTION;
                    event2.motion.which = 0;
                    event2.motion.state = buttons;
                    event2.motion.xrel = event.jball.xrel - x;
                    event2.motion.yrel = event.jball.yrel - y;
                    if (event.jball.xrel!=0) {
                        x = event.jball.xrel;
                        y = event.jball.yrel;
                    }
                    event2.motion.x = x;
                    event2.motion.y = y;
                    //__android_log_print(ANDROID_LOG_INFO, "GUICHAN","Mouse motion %d %d btns %d", x, y, buttons);
                    if (buttons == 0) {
                        // Push mouse motion event first, then button down event
                        gui_input->pushInput(event2);
                        buttons = SDL_BUTTON_LMASK;
                        event2.type = SDL_MOUSEBUTTONDOWN;
                        event2.button.which = 0;
                        event2.button.button = SDL_BUTTON_LEFT;
                        event2.button.state =  SDL_PRESSED;
                        event2.button.x = x;
                        event2.button.y = y;
                        //__android_log_print(ANDROID_LOG_INFO, "GUICHAN","Mouse button %d coords %d %d", buttons, x, y);
                    }
                }
                if (event.type == SDL_JOYBUTTONUP &&
                    event.jbutton.which == 0 &&
                    event.jbutton.button == 0) {
                    // Do not push button down event here, because we need mouse motion event first
                    buttons = 0;
                    event2.type = SDL_MOUSEBUTTONUP;
                    event2.button.which = 0;
                    event2.button.button = SDL_BUTTON_LEFT;
                    event2.button.state = SDL_RELEASED;
                    event2.button.x = x;
                    event2.button.y = y;
                    //__android_log_print(ANDROID_LOG_INFO, "GUICHAN","Mouse button %d coords %d %d", buttons, x, y);
                }
                gui_input->pushInput(event2);
            }
#else
            gui_input->pushInput(event);
#endif
    }

    // Now we let the Gui object perform its logic.
    uae_gui->logic();
    // Now we let the Gui object draw itself.
    uae_gui->draw();
    // Finally we update the screen.
    SDL_Flip(gui_screen);
  }  
}


bool EditFilesysVirtual(int unit_no)
{
  char *volname, *devname, *rootdir, *filesys;
  int secspertrack, surfaces, cylinders, reserved, blocksize, readonly, bootpri;
  uae_u64 size;
  const char *failure;
  
  dialogResult = false;
  dialogFinished = false;

  InitEditFilesysVirtual();

  if(unit_no >= 0)
  {
    char tmp[32];

    failure = get_filesys_unit(currprefs.mountinfo, unit_no, 
      &devname, &volname, &rootdir, &readonly, &secspertrack, &surfaces, &reserved, 
      &cylinders, &size, &blocksize, &bootpri, &filesys, 0);

    txtDevice->setText(devname);
    txtVolume->setText(volname);
    txtPath->setText(rootdir);
    chkReadWrite->setSelected(!readonly);
    snprintf(tmp, 32, "%d", bootpri);
    txtBootPri->setText(tmp);
  }
  else
  {
    txtDevice->setText("");
    txtVolume->setText("");
    txtPath->setText(currentDir);
    chkReadWrite->setSelected(true);
    txtBootPri->setText("0");
  }

  EditFilesysVirtualLoop();
  ExitEditFilesysVirtual();
  
  if(dialogResult)
  {
    if(unit_no >= 0)
      kill_filesys_unit(currprefs.mountinfo, unit_no);
    else
      extractPath((char *) txtPath->getText().c_str(), currentDir);
    failure = add_filesys_unit(currprefs.mountinfo, (char *) txtDevice->getText().c_str(), 
      (char *) txtVolume->getText().c_str(), (char *) txtPath->getText().c_str(), 
      !chkReadWrite->isSelected(), 0, 0, 0, 0, atoi(txtBootPri->getText().c_str()), 0, 0);
  }

  return dialogResult;
}
