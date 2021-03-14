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
#include "gui_handling.h"

#ifdef ANDROIDSDL
#include "androidsdl_event.h"
#endif

#define DIALOG_WIDTH 520
#define DIALOG_HEIGHT 202

extern std::string volName;

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
static gcn::UaeCheckBox* chkAutoboot;
static gcn::Label *lblBootPri;
static gcn::TextField *txtBootPri;


class FilesysVirtualActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if(actionEvent.getSource() == cmdPath) {
        char tmp[MAX_PATH];
        strncpy(tmp, txtPath->getText().c_str(), MAX_PATH - 1);
        wndEditFilesysVirtual->releaseModalFocus();
        if(SelectFolder("Select folder", tmp)) {
          txtPath->setText(tmp);
          txtVolume->setText(volName); // lubomyr
          default_fsvdlg(&current_fsvdlg);
          CreateDefaultDevicename(current_fsvdlg.ci.devname);
          _tcscpy (current_fsvdlg.ci.volname, current_fsvdlg.ci.devname);
          _tcscpy (current_fsvdlg.ci.rootdir, tmp);
	    }
        wndEditFilesysVirtual->requestModalFocus();
        cmdPath->requestFocus();

      } else if (actionEvent.getSource() == chkAutoboot) {
        char tmp[32];
        if (chkAutoboot->isSelected()) {
  				current_fsvdlg.ci.bootpri = 0;
        } else {
  				current_fsvdlg.ci.bootpri = BOOTPRI_NOAUTOBOOT;
        }
      	snprintf(tmp, sizeof (tmp) - 1, "%d", current_fsvdlg.ci.bootpri);
        txtBootPri->setText(tmp);

      } else {
        if (actionEvent.getSource() == cmdOK) {
          if(txtDevice->getText().length() <= 0) {
            wndEditFilesysVirtual->setCaption("Please enter a device name.");
            return;
          }
          if(txtVolume->getText().length() <= 0) {
            wndEditFilesysVirtual->setCaption("Please enter a volume name.");
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

	chkAutoboot = new gcn::UaeCheckBox("Bootable", true);
  chkAutoboot->setId("virtAutoboot");
  chkAutoboot->addActionListener(filesysVirtualActionListener);

  lblBootPri = new gcn::Label("Boot priority:");
  lblBootPri->setSize(84, LABEL_HEIGHT);
  lblBootPri->setAlignment(gcn::Graphics::RIGHT);
  txtBootPri = new gcn::TextField();
  txtBootPri->setSize(40, TEXTFIELD_HEIGHT);
  txtBootPri->setId("virtBootpri");
  
  int posY = DISTANCE_BORDER;
	int posX = DISTANCE_BORDER;

  wndEditFilesysVirtual->add(lblDevice, DISTANCE_BORDER, posY);
	posX += lblDevice->getWidth() + 8;

  wndEditFilesysVirtual->add(txtDevice, posX, posY);
	posX += txtDevice->getWidth() + DISTANCE_BORDER * 2;

  wndEditFilesysVirtual->add(chkReadWrite, posX, posY + 1);
  posY += txtDevice->getHeight() + DISTANCE_NEXT_Y;

  wndEditFilesysVirtual->add(lblVolume, DISTANCE_BORDER, posY);
	wndEditFilesysVirtual->add(txtVolume, txtDevice->getX(), posY);

	wndEditFilesysVirtual->add(chkAutoboot, chkReadWrite->getX(), posY + 1);
	posX += chkAutoboot->getWidth() + DISTANCE_BORDER * 2;

	wndEditFilesysVirtual->add(lblBootPri, posX, posY);
	wndEditFilesysVirtual->add(txtBootPri, posX + lblBootPri->getWidth() + 8, posY);
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
  delete chkAutoboot;
  delete lblBootPri;
  delete txtBootPri;
  
  delete cmdOK;
  delete cmdCancel;
  delete filesysVirtualActionListener;
  
  delete wndEditFilesysVirtual;
}


static void EditFilesysVirtualLoop(void)
{
#ifndef USE_SDL2
  FocusBugWorkaround(wndEditFilesysVirtual);  
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
          case VK_ESCAPE:
            dialogFinished = true;
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

          case VK_X:
          case VK_A:
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


bool EditFilesysVirtual(int unit_no)
{
  struct mountedinfo mi;
  struct uaedev_config_data *uci;
  std::string strdevname, strvolname, strroot;
  char tmp[32];
  
  dialogResult = false;
  dialogFinished = false;

  InitEditFilesysVirtual();

  if(unit_no >= 0)
  {
    struct uaedev_config_info *ci;

    uci = &workprefs.mountconfig[unit_no];
    get_filesys_unitconfig(&workprefs, unit_no, &mi);
		memcpy (&current_fsvdlg.ci, uci, sizeof (struct uaedev_config_info));
  }
  else
  {
		default_fsvdlg (&current_fsvdlg);
    CreateDefaultDevicename(current_fsvdlg.ci.devname);
    _tcscpy (current_fsvdlg.ci.volname, current_fsvdlg.ci.devname);
  }
  strdevname.assign(current_fsvdlg.ci.devname);
  txtDevice->setText(strdevname);
  strvolname.assign(current_fsvdlg.ci.volname);
  txtVolume->setText(strvolname);
  strroot.assign(current_fsvdlg.ci.rootdir);
  txtPath->setText(strroot);
  chkReadWrite->setSelected(!current_fsvdlg.ci.readonly);
  chkAutoboot->setSelected(current_fsvdlg.ci.bootpri != BOOTPRI_NOAUTOBOOT);
	snprintf(tmp, sizeof (tmp) - 1, "%d", current_fsvdlg.ci.bootpri >= -127 ? current_fsvdlg.ci.bootpri : -127);
  txtBootPri->setText(tmp);

  EditFilesysVirtualLoop();
  
  if(dialogResult)
  {
    struct uaedev_config_info ci;

    strncpy(current_fsvdlg.ci.devname, (char *) txtDevice->getText().c_str(), MAX_DPATH - 1);
    strncpy(current_fsvdlg.ci.volname, (char *) txtVolume->getText().c_str(), MAX_DPATH - 1);
    current_fsvdlg.ci.readonly = !chkReadWrite->isSelected();
    current_fsvdlg.ci.bootpri = tweakbootpri(atoi(txtBootPri->getText().c_str()), chkAutoboot->isSelected() ? 1 : 0, 0);

   	memcpy (&ci, &current_fsvdlg.ci, sizeof (struct uaedev_config_info));
    uci = add_filesys_config(&workprefs, unit_no, &ci);
    if (uci) {
  		if (uci->ci.rootdir[0])
  			filesys_media_change (uci->ci.rootdir, unit_no, uci);
  		else if (uci->configoffset >= 0)
  			filesys_eject (uci->configoffset);
    }

    extractPath((char *) txtPath->getText().c_str(), currentDir);
  }

  ExitEditFilesysVirtual();

  return dialogResult;
}
