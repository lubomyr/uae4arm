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
#include "UaeDropDown.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "include/memory-uae.h"
#include "uae.h"
#include "autoconf.h"
#include "filesys.h"
#include "gui.h"
#include "gui_handling.h"
#include "keyboard.h"
#include "inputdevice.h"
#include "GenericListModel.h"


#define MAX_TOTAL_DEVICES 12
static int total_devices;
static int total_devicesPara;


static const char *mousespeed_list[] = { ".25", ".5", "1x", "2x", "4x" };
static const int mousespeed_values[] = { 2, 5, 10, 20, 40 };

static gcn::Label *lblPort0;
static gcn::Label *lblPort1;
static gcn::Label *lblPort2;
static gcn::Label *lblPort3;

static gcn::Label* lblMouseSpeed;
static gcn::Label* lblMouseSpeedInfo;
static gcn::Slider* sldMouseSpeed;
static gcn::Label* lblAutofireRate;
static gcn::Label* lblAutofireRateInfo;
static gcn::Slider* sldAutofireRate;
  
static gcn::Label *lblNumLock;
static gcn::UaeDropDown* cboKBDLed_num;
static gcn::Label *lblScrLock;
static gcn::UaeDropDown* cboKBDLed_scr;
static gcn::Label *lblCapLock;
static gcn::UaeDropDown* cboKBDLed_cap;

static const char *inputport_list[MAX_TOTAL_DEVICES] = { "<none>", "Keyboard Layout A", "Keyboard Layout B", "Keyboard Layout C", NULL };
static gcn::GenericListModel ctrlPortList;
static gcn::GenericListModel ctrlPortListPara;

#define MAX_INPUT_MODES 6
static const char *inputmode_list[] = { "Default", "Mouse", "Joystick", "Gamepad", "Analog joystick", "CD32 pad" };
static const int inputmode_val[] = { JSEM_MODE_DEFAULT, JSEM_MODE_MOUSE, JSEM_MODE_JOYSTICK, JSEM_MODE_GAMEPAD, JSEM_MODE_JOYSTICK_ANALOG, JSEM_MODE_JOYSTICK_CD32 };
static gcn::GenericListModel ctrlPortModeList(inputmode_list, MAX_INPUT_MODES);

const char *autofireValues[] = { "No autofire", "Autofire", "Autofire (toggle)", "Autofire (always)" };
static gcn::GenericListModel autofireList(autofireValues, 4);

static gcn::UaeDropDown* cboPorts[MAX_JPORTS] = { NULL, NULL, NULL, NULL };
static gcn::UaeDropDown* cboPortModes[MAX_JPORTS] = { NULL, NULL, NULL, NULL };
static gcn::UaeDropDown* cboAutofires[MAX_JPORTS] = { NULL, NULL, NULL, NULL };


static const char *listValues[] = { "none", "POWER", "DF0", "DF1", "DF2", "DF3", "DF*", "HD", "CD" };
static gcn::GenericListModel KBDLedList(listValues, 9);


static void updatejoyport (int changedport)
{
  int i;
  TCHAR tmp[32];
  
  for(i = 0; i < 5; ++i) {
    if(workprefs.input_joymouse_multiplier == mousespeed_values[i]) {
      sldMouseSpeed->setValue(i);
      lblMouseSpeedInfo->setCaption(mousespeed_list[i]);
      break;
    }
  }

	for (i = 0; i < MAX_JPORTS; i++) {
		int v = workprefs.jports[i].id;
		int vm = workprefs.jports[i].mode;

    if (i < 2) {
      for(int idx = 0; idx < MAX_INPUT_MODES; ++idx) {
        if(workprefs.jports[i].mode == inputmode_val[idx]) {
          cboPortModes[i]->setSelected(idx);
          break;
        }
      }
    }
     
		int idx = inputdevice_getjoyportdevice (i, v);
		if (idx >= 0)
			idx += 1;
		else
			idx = 0;
		if (idx >= (i < 2 ? total_devices : total_devicesPara))
			idx = 0;
    cboPorts[i]->setSelected(idx); 

		if (i < 2)
      cboAutofires[i]->setSelected(workprefs.jports[i].autofire);
  }

  sldAutofireRate->setValue(workprefs.input_autofire_linecnt / 100);
  _stprintf (tmp, _T("%d lines"), workprefs.input_autofire_linecnt);
  lblAutofireRateInfo->setCaption(tmp);

	cboKBDLed_num->setSelected(workprefs.kbd_led_num);
	cboKBDLed_scr->setSelected(workprefs.kbd_led_scr);
}


static void values_from_gameportsdlg(int changedport)
{
	int i;
	int changed = 0;

	for (i = 0; i < MAX_JPORTS; i++) {
		int idx = 0;
		int *port = &workprefs.jports[i].id;
		int *portm = &workprefs.jports[i].mode;
		int prevport = *port;
    
    int v = cboPorts[i]->getSelected();

		int max = JSEM_LASTKBD + inputdevice_get_device_total (IDTYPE_JOYSTICK);
		if (i < 2)
			max += inputdevice_get_device_total (IDTYPE_MOUSE);
		v -= 1;
		if (v < 0) {
			*port = JPORT_NONE;
		} else if (v >= max) {
			*port = JPORT_NONE;
		} else if (v < JSEM_LASTKBD) {
			*port = JSEM_KBDLAYOUT + (int)v;
		} else if (v >= JSEM_LASTKBD + inputdevice_get_device_total (IDTYPE_JOYSTICK)) {
			*port = JSEM_MICE + (int)v - inputdevice_get_device_total (IDTYPE_JOYSTICK) - JSEM_LASTKBD;
		} else {
			*port = JSEM_JOYS + (int)v - JSEM_LASTKBD;
		}

		if (i < 2) {
		  v = inputmode_val[cboPortModes[i]->getSelected()];
			*portm = v;
      int af = cboAutofires[i]->getSelected();
      workprefs.jports[i].autofire = af;
		}

		if (*port != prevport)
			changed = 1;
	}
	if (changed)
		inputdevice_validate_jports (&workprefs, changedport, NULL);
}


static void processport (bool reset, int port)
{
	if (reset)
		inputdevice_compa_clear (&workprefs, port);
	values_from_gameportsdlg (port);
	updatejoyport (port);
	inputdevice_updateconfig (NULL, &workprefs);
	inputdevice_config_change ();
}


class GamePortActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cboPorts[0] || actionEvent.getSource() == cboPortModes[0]) {
				processport (true, 0);
				inputdevice_forget_unplugged_device(0);
      }
      
      else if (actionEvent.getSource() == cboPorts[1] || actionEvent.getSource() == cboPortModes[1]) {
				processport (true, 1);
				inputdevice_forget_unplugged_device(1);
      }
      
      else if (actionEvent.getSource() == cboPorts[2]) {
				processport (true, 2);
				inputdevice_forget_unplugged_device(2);
      }

      else if (actionEvent.getSource() == cboPorts[3]) {
				processport (true, 3);
				inputdevice_forget_unplugged_device(3);
      }

      else if (actionEvent.getSource() == cboAutofires[0]) {
        processport (false, 0);
      }
      
      else if (actionEvent.getSource() == cboAutofires[1]) {
        processport (false, 1);
      }
      
 	    else if (actionEvent.getSource() == sldMouseSpeed) {
    		workprefs.input_joymouse_multiplier = mousespeed_values[(int)(sldMouseSpeed->getValue())];
        lblMouseSpeedInfo->setCaption(mousespeed_list[(int)sldMouseSpeed->getValue()]);
    	}

 	    else if (actionEvent.getSource() == sldAutofireRate) {
        TCHAR tmp[32];
    		workprefs.input_autofire_linecnt = (int)(sldAutofireRate->getValue()) * 100;
        _stprintf (tmp, _T("%d lines"), workprefs.input_autofire_linecnt);
        lblAutofireRateInfo->setCaption(tmp);
    	}

      else if (actionEvent.getSource() == cboKBDLed_num)
        workprefs.kbd_led_num = cboKBDLed_num->getSelected();

      else if (actionEvent.getSource() == cboKBDLed_scr)
  			workprefs.kbd_led_scr = cboKBDLed_scr->getSelected();
    }
};
static GamePortActionListener* gameportActionListener;


void InitPanelGamePort(const struct _ConfigCategory& category)
{
  int j;

  total_devices = 1 + JSEM_LASTKBD;
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_JOYSTICK) && total_devices < MAX_TOTAL_DEVICES; j++, total_devices++)
	  inputport_list[total_devices] = inputdevice_get_device_name (IDTYPE_JOYSTICK, j);
  total_devicesPara = total_devices;
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_MOUSE) && total_devices < MAX_TOTAL_DEVICES; j++, total_devices++)
		inputport_list[total_devices] = inputdevice_get_device_name (IDTYPE_MOUSE, j);

  ctrlPortList.clear();
  for(j = 0; j < total_devices; ++j)
    ctrlPortList.add(inputport_list[j]);

  ctrlPortListPara.clear();
  for(j = 0; j < total_devicesPara; ++j)
    ctrlPortListPara.add(inputport_list[j]);
  
  gameportActionListener = new GamePortActionListener();

  lblPort0 = new gcn::Label("Port 1:");
  lblPort0->setSize(95, LABEL_HEIGHT);
  lblPort0->setAlignment(gcn::Graphics::RIGHT);
	cboPorts[0] = new gcn::UaeDropDown(&ctrlPortList);
  cboPorts[0]->setSize(230, DROPDOWN_HEIGHT);
  cboPorts[0]->setBaseColor(gui_baseCol);
  cboPorts[0]->setId("cboPort0");
  cboPorts[0]->addActionListener(gameportActionListener);

  lblPort1 = new gcn::Label("Port 2:");
  lblPort1->setSize(95, LABEL_HEIGHT);
  lblPort1->setAlignment(gcn::Graphics::RIGHT);
	cboPorts[1] = new gcn::UaeDropDown(&ctrlPortList);
  cboPorts[1]->setSize(230, DROPDOWN_HEIGHT);
  cboPorts[1]->setBaseColor(gui_baseCol);
  cboPorts[1]->setId("cboPort1");
  cboPorts[1]->addActionListener(gameportActionListener);

  lblPort2 = new gcn::Label("paral. Port 1:");
  lblPort2->setSize(95, LABEL_HEIGHT);
  lblPort2->setAlignment(gcn::Graphics::RIGHT);
	cboPorts[2] = new gcn::UaeDropDown(&ctrlPortListPara);
  cboPorts[2]->setSize(230, DROPDOWN_HEIGHT);
  cboPorts[2]->setBaseColor(gui_baseCol);
  cboPorts[2]->setId("cboPort2");
  cboPorts[2]->addActionListener(gameportActionListener);

  lblPort3 = new gcn::Label("paral. Port 2:");
  lblPort3->setSize(95, LABEL_HEIGHT);
  lblPort3->setAlignment(gcn::Graphics::RIGHT);
	cboPorts[3] = new gcn::UaeDropDown(&ctrlPortListPara);
  cboPorts[3]->setSize(230, DROPDOWN_HEIGHT);
  cboPorts[3]->setBaseColor(gui_baseCol);
  cboPorts[3]->setId("cboPort3");
  cboPorts[3]->addActionListener(gameportActionListener);

	cboPortModes[0] = new gcn::UaeDropDown(&ctrlPortModeList);
  cboPortModes[0]->setSize(90, DROPDOWN_HEIGHT);
  cboPortModes[0]->setBaseColor(gui_baseCol);
  cboPortModes[0]->setId("cboPortMode0");
  cboPortModes[0]->addActionListener(gameportActionListener);

	cboPortModes[1] = new gcn::UaeDropDown(&ctrlPortModeList);
  cboPortModes[1]->setSize(90, DROPDOWN_HEIGHT);
  cboPortModes[1]->setBaseColor(gui_baseCol);
  cboPortModes[1]->setId("cboPortMode1");
  cboPortModes[1]->addActionListener(gameportActionListener);

	cboAutofires[0] = new gcn::UaeDropDown(&autofireList);
  cboAutofires[0]->setSize(130, DROPDOWN_HEIGHT);
  cboAutofires[0]->setBaseColor(gui_baseCol);
  cboAutofires[0]->setId("cboAutofire0");
  cboAutofires[0]->addActionListener(gameportActionListener);

	cboAutofires[1] = new gcn::UaeDropDown(&autofireList);
  cboAutofires[1]->setSize(130, DROPDOWN_HEIGHT);
  cboAutofires[1]->setBaseColor(gui_baseCol);
  cboAutofires[1]->setId("cboAutofire1");
  cboAutofires[1]->addActionListener(gameportActionListener);

	lblMouseSpeed = new gcn::Label("Mouse Speed:");
  lblMouseSpeed->setSize(95, LABEL_HEIGHT);
  lblMouseSpeed->setAlignment(gcn::Graphics::RIGHT);
  sldMouseSpeed = new gcn::Slider(0, 4);
  sldMouseSpeed->setSize(110, SLIDER_HEIGHT);
  sldMouseSpeed->setBaseColor(gui_baseCol);
	sldMouseSpeed->setMarkerLength(20);
	sldMouseSpeed->setStepLength(1);
	sldMouseSpeed->setId("MouseSpeed");
  sldMouseSpeed->addActionListener(gameportActionListener);
  lblMouseSpeedInfo = new gcn::Label(".25");

	lblAutofireRate = new gcn::Label("Autofire rate:");
  lblAutofireRate->setSize(82, LABEL_HEIGHT);
  lblAutofireRate->setAlignment(gcn::Graphics::RIGHT);
  sldAutofireRate = new gcn::Slider(1, 40);
  sldAutofireRate->setSize(117, SLIDER_HEIGHT);
  sldAutofireRate->setBaseColor(gui_baseCol);
	sldAutofireRate->setMarkerLength(20);
	sldAutofireRate->setStepLength(1);
	sldAutofireRate->setId("AutofireRate");
  sldAutofireRate->addActionListener(gameportActionListener);
  lblAutofireRateInfo = new gcn::Label("XXXX lines.");

//  lblNumLock = new gcn::Label("NumLock LED:");
//  lblNumLock->setSize(100, LABEL_HEIGHT);
//  lblNumLock->setAlignment(gcn::Graphics::RIGHT);
//  cboKBDLed_num = new gcn::UaeDropDown(&KBDLedList);
//  cboKBDLed_num->setSize(150, DROPDOWN_HEIGHT);
//  cboKBDLed_num->setBaseColor(gui_baseCol);
//  cboKBDLed_num->setId("numlock");
//  cboKBDLed_num->addActionListener(gameportActionListener);

//  lblCapLock = new gcn::Label("CapsLock LED:");
//  lblCapLock->setSize(100, LABEL_HEIGHT);
//  lblCapLock->setAlignment(gcn::Graphics::RIGHT);
//  cboKBDLed_cap = new gcn::UaeDropDown(&KBDLedList);
//  cboKBDLed_cap->setSize(150, DROPDOWN_HEIGHT);
//  cboKBDLed_cap->setBaseColor(gui_baseCol);
//  cboKBDLed_cap->setId("capslock");
//  cboKBDLed_cap->addActionListener(gameportActionListener);

//  lblScrLock = new gcn::Label("ScrollLock LED:");
//  lblScrLock->setSize(100, LABEL_HEIGHT);
//  lblScrLock->setAlignment(gcn::Graphics::RIGHT);
//  cboKBDLed_scr = new gcn::UaeDropDown(&KBDLedList);
//  cboKBDLed_scr->setSize(150, DROPDOWN_HEIGHT);
//  cboKBDLed_scr->setBaseColor(gui_baseCol);
//  cboKBDLed_scr->setId("scrolllock");
//  cboKBDLed_scr->addActionListener(gameportActionListener);

  int posY = DISTANCE_BORDER;
  category.panel->add(lblPort0, DISTANCE_BORDER, posY);
  category.panel->add(cboPorts[0], DISTANCE_BORDER + lblPort0->getWidth() + 8, posY);
  category.panel->add(cboPortModes[0], cboPorts[0]->getX() + cboPorts[0]->getWidth() + 8, posY);
  category.panel->add(cboAutofires[0], cboPortModes[0]->getX() + cboPortModes[0]->getWidth() + 8, posY);
  posY += cboPorts[0]->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblPort1, DISTANCE_BORDER, posY);
  category.panel->add(cboPorts[1], DISTANCE_BORDER + lblPort1->getWidth() + 8, posY);
  category.panel->add(cboPortModes[1], cboPorts[1]->getX() + cboPorts[1]->getWidth() + 8, posY);
  category.panel->add(cboAutofires[1], cboPortModes[1]->getX() + cboPortModes[1]->getWidth() + 8, posY);
  posY += cboPorts[1]->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(lblPort2, DISTANCE_BORDER, posY);
  category.panel->add(cboPorts[2], DISTANCE_BORDER + lblPort2->getWidth() + 8, posY);
  posY += cboPorts[2]->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblPort3, DISTANCE_BORDER, posY);
  category.panel->add(cboPorts[3], DISTANCE_BORDER + lblPort3->getWidth() + 8, posY);
  posY += cboPorts[3]->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(lblMouseSpeed, DISTANCE_BORDER, posY);
  category.panel->add(sldMouseSpeed, DISTANCE_BORDER + lblMouseSpeed->getWidth() + 9, posY);
  category.panel->add(lblMouseSpeedInfo, sldMouseSpeed->getX() + sldMouseSpeed->getWidth() + 12, posY);
  category.panel->add(lblAutofireRate, 300, posY);
  category.panel->add(sldAutofireRate, 300 + lblAutofireRate->getWidth() + 9, posY);
  category.panel->add(lblAutofireRateInfo, sldAutofireRate->getX() + sldAutofireRate->getWidth() + 12, posY);
  posY += sldAutofireRate->getHeight() + DISTANCE_NEXT_Y;

//  category.panel->add(lblNumLock, DISTANCE_BORDER, posY);
//	category.panel->add(cboKBDLed_num, DISTANCE_BORDER + lblNumLock->getWidth() + 8, posY);
//  category.panel->add(lblCapLock, lblNumLock->getX() + lblNumLock->getWidth() + DISTANCE_NEXT_X, posY);
//  category.panel->add(cboKBDLed_cap, cboKBDLed_num->getX() + cboKBDLed_num->getWidth() + DISTANCE_NEXT_X, posY);
//	category.panel->add(lblScrLock, cboKBDLed_num->getX() + cboKBDLed_num->getWidth() + DISTANCE_NEXT_X, posY);
//	category.panel->add(cboKBDLed_scr, lblScrLock->getX() + lblScrLock->getWidth() + 8, posY);
//  posY += cboKBDLed_scr->getHeight() + DISTANCE_NEXT_Y;

	inputdevice_updateconfig (NULL, &workprefs);
	updatejoyport (-1);
}


void ExitPanelGamePort(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete lblPort0;
  delete cboPorts[0];
  delete lblPort1;
  delete cboPorts[1];
  delete lblPort2;
  delete cboPorts[2];
  delete lblPort3;
  delete cboPorts[3];
  
  delete cboPortModes[0];  
  delete cboPortModes[1];
  delete cboAutofires[0];
  delete cboAutofires[1];
  delete lblMouseSpeed;
  delete sldMouseSpeed;
  delete lblMouseSpeedInfo;
  delete lblAutofireRate;
  delete sldAutofireRate;
  delete lblAutofireRateInfo;

  delete lblCapLock;
  delete lblScrLock;
  delete lblNumLock;
  delete cboKBDLed_num;
  delete cboKBDLed_cap;
  delete cboKBDLed_scr;

  delete gameportActionListener;
}


bool HelpPanelGamePort(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("You can select the control type, mode and autofire for all ports.");
  helptext.push_back("");
  helptext.push_back("The \"Autofire rate\" is used for all ports with active autofire.");
  helptext.push_back("");
  helptext.push_back("Set the emulated mouse speed to .25x, .5x, 1x, 2x and 4x to slow down or speed up the mouse.");
  return true;
}
