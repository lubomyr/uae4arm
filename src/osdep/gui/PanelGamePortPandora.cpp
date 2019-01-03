#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#include "SelectorEntry.hpp"
#include "UaeDropDown.hpp"
#include "UaeCheckBox.hpp"

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

#ifdef ANDROIDSDL
#include <SDL_android.h>
#endif

static int total_devices;


static const char *mousespeed_list[] = { ".25", ".5", "1x", "2x", "4x" };
static const int mousespeed_values[] = { 2, 5, 10, 20, 40 };

static gcn::Label *lblPort0;
static gcn::Label *lblPort1;

static gcn::Label* lblMouseSpeed;
static gcn::Label* lblMouseSpeedInfo;
static gcn::Slider* sldMouseSpeed;
static gcn::Label* lblAutofireRate;
static gcn::Label* lblAutofireRateInfo;
static gcn::Slider* sldAutofireRate;


static gcn::Label *lblTapDelay;
static gcn::UaeDropDown* cboTapDelay;
static gcn::UaeCheckBox* chkMouseHack;
  

static const char *inputport_list[12] = { "<none>", NULL };
static gcn::GenericListModel ctrlPortList;

static const char *inputmode_list[] = { "Default", "Mouse", "Joystick", "CD32 pad" };
static const int inputmode_val[] = { JSEM_MODE_DEFAULT, JSEM_MODE_MOUSE, JSEM_MODE_JOYSTICK, JSEM_MODE_JOYSTICK_CD32 };
static gcn::GenericListModel ctrlPortModeList(inputmode_list, 4);

const char *autofireValues[] = { "No autofire", "Autofire", "Autofire (toggle)", "Autofire (always)" };
static gcn::GenericListModel autofireList(autofireValues, 4);

static gcn::UaeDropDown* cboPorts[4] = { NULL, NULL, NULL, NULL };
static gcn::UaeDropDown* cboPortModes[4] = { NULL, NULL, NULL, NULL };
static gcn::UaeDropDown* cboAutofires[4] = { NULL, NULL, NULL, NULL };


const char *tapDelayValues[] = { "Normal", "Short", "None" };
static gcn::GenericListModel tapDelayList(tapDelayValues, 3);
  
static void RefreshPanelGamePort(void)
{
  int i, idx;
  TCHAR tmp[100];
  
  // Set current device in port 0
  idx = inputdevice_getjoyportdevice (0, workprefs.jports[0].id);
	if (idx >= 0)
		idx += 1;
	else
		idx = 0;
	if (idx >= total_devices)
		idx = 0;
  cboPorts[0]->setSelected(idx); 

  for(i = 0; i < 4; ++i) {
    if(workprefs.jports[0].mode == inputmode_val[i]) {
      cboPortModes[0]->setSelected(i);
      break;
    }
  }
  
  // Set current device in port 1
  idx = inputdevice_getjoyportdevice (1, workprefs.jports[1].id);
	if (idx >= 0)
		idx += 1;
	else
		idx = 0;
	if (idx >= total_devices)
		idx = 0;
  cboPorts[1]->setSelected(idx); 

  for(i = 0; i < 4; ++i) {
    if(workprefs.jports[1].mode == inputmode_val[i]) {
      cboPortModes[1]->setSelected(i);
      break;
    }
  }

  cboAutofires[0]->setSelected(workprefs.jports[0].autofire);
  cboAutofires[1]->setSelected(workprefs.jports[1].autofire);

  for(i = 0; i < 5; ++i) {
    if(workprefs.input_joymouse_multiplier == mousespeed_values[i]) {
      sldMouseSpeed->setValue(i);
      lblMouseSpeedInfo->setCaption(mousespeed_list[i]);
      break;
    }
  }

  sldAutofireRate->setValue(workprefs.input_autofire_linecnt / 100);
  _stprintf (tmp, _T("%d lines"), workprefs.input_autofire_linecnt);
  lblAutofireRateInfo->setCaption(tmp);

	if (workprefs.pandora_tapDelay == 10)
    cboTapDelay->setSelected(0);
  else if (workprefs.pandora_tapDelay == 5)
    cboTapDelay->setSelected(1);
  else
    cboTapDelay->setSelected(2);
  
  chkMouseHack->setSelected(workprefs.input_tablet == TABLET_MOUSEHACK);
}


static void values_to_dialog(void)
{
	inputdevice_updateconfig (NULL, &workprefs);

  RefreshPanelGamePort();
}


static void values_from_dialog(int changedport, bool reset)
{
	int i;
	int changed = 0;
	int prevport;

  if(reset)
    inputdevice_compa_clear (&workprefs, changedport);

	for (i = 0; i < MAX_JPORTS; i++) {
	  if (i >= 2)
	    continue; // parallel port not yet available
	  
		int prevport = workprefs.jports[i].id;
		int max = inputdevice_get_device_total (IDTYPE_JOYSTICK);
		if (i < 2)
			max += inputdevice_get_device_total (IDTYPE_MOUSE);
    int id = cboPorts[i]->getSelected() - 1;
		if (id < 0) {
			workprefs.jports[i].id = JPORT_NONE;
		} else if (id >= max) {
			workprefs.jports[i].id = JPORT_NONE;
		} else if (id >= inputdevice_get_device_total (IDTYPE_JOYSTICK)) {
			workprefs.jports[i].id = JSEM_MICE + id - inputdevice_get_device_total (IDTYPE_JOYSTICK);
		} else {
			workprefs.jports[i].id = JSEM_JOYS + id;
		}

		if(cboPortModes[i] != NULL)
		  workprefs.jports[i].mode = inputmode_val[cboPortModes[i]->getSelected()];
		if(cboAutofires[i] != NULL)
		  workprefs.jports[i].autofire = cboAutofires[i]->getSelected();

		if (workprefs.jports[i].id != prevport)
			changed = 1;
	}

  if (changed)
    inputdevice_validate_jports (&workprefs, changedport, NULL);        

	RefreshPanelGamePort();

  inputdevice_config_change();
  if(reset)
    inputdevice_forget_unplugged_device(changedport);
}


class GamePortActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cboPorts[0] || actionEvent.getSource() == cboPortModes[0]) {
        values_from_dialog(0, true);
      }
      
      else if (actionEvent.getSource() == cboPorts[1] || actionEvent.getSource() == cboPortModes[1]) {
        values_from_dialog(1, true);
      }
      
      else if (actionEvent.getSource() == cboAutofires[0]) {
        values_from_dialog(0, false);
      }
      
      else if (actionEvent.getSource() == cboAutofires[1]) {
        values_from_dialog(1, false);
      }

 	    else if (actionEvent.getSource() == sldMouseSpeed) {
    		workprefs.input_joymouse_multiplier = mousespeed_values[(int)(sldMouseSpeed->getValue())];
    		RefreshPanelGamePort();
    	}
    	
 	    else if (actionEvent.getSource() == sldAutofireRate) {
    		workprefs.input_autofire_linecnt = (int)(sldAutofireRate->getValue()) * 100;
    		RefreshPanelGamePort();
    	}

      else if (actionEvent.getSource() == cboTapDelay) {
        if(cboTapDelay->getSelected() == 0)
          workprefs.pandora_tapDelay = 10;
        else if (cboTapDelay->getSelected() == 1)
          workprefs.pandora_tapDelay = 5;
        else
          workprefs.pandora_tapDelay = 2;

      } else if (actionEvent.getSource() == chkMouseHack) {
#ifdef ANDROIDSDL
        if (chkMouseHack->isSelected())
             SDL_ANDROID_SetMouseEmulationMode(0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        else
             SDL_ANDROID_SetMouseEmulationMode(1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
#endif
  	    workprefs.input_tablet = chkMouseHack->isSelected() ? TABLET_MOUSEHACK : TABLET_OFF;
  	  }
    }
};
static GamePortActionListener* gameportActionListener;


void InitPanelGamePort(const struct _ConfigCategory& category)
{
  int j;

  total_devices = 1;
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_JOYSTICK) && total_devices < 12; j++, total_devices++)
	  inputport_list[total_devices] = inputdevice_get_device_name (IDTYPE_JOYSTICK, j);
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_MOUSE) && total_devices < 12; j++, total_devices++)
		inputport_list[total_devices] = inputdevice_get_device_name (IDTYPE_MOUSE, j);

  ctrlPortList.clear();
  for(j = 0; j < total_devices; ++j)
    ctrlPortList.add(inputport_list[j]);
  
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
#ifdef ANDROID
  lblMouseSpeed->setSize(105, LABEL_HEIGHT);
#else
  lblMouseSpeed->setSize(95, LABEL_HEIGHT);
#endif
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
#ifdef ANDROID
  lblAutofireRate->setSize(94, LABEL_HEIGHT);
#else
  lblAutofireRate->setSize(82, LABEL_HEIGHT);
#endif
  lblAutofireRate->setAlignment(gcn::Graphics::RIGHT);
  sldAutofireRate = new gcn::Slider(1, 40);
  sldAutofireRate->setSize(117, SLIDER_HEIGHT);
  sldAutofireRate->setBaseColor(gui_baseCol);
	sldAutofireRate->setMarkerLength(20);
	sldAutofireRate->setStepLength(1);
	sldAutofireRate->setId("AutofireRate");
  sldAutofireRate->addActionListener(gameportActionListener);
  lblAutofireRateInfo = new gcn::Label("XXXX lines.");

  lblTapDelay = new gcn::Label("Tap Delay:");
  lblTapDelay->setSize(82, LABEL_HEIGHT);
  lblTapDelay->setAlignment(gcn::Graphics::RIGHT);
	cboTapDelay = new gcn::UaeDropDown(&tapDelayList);
  cboTapDelay->setSize(80, DROPDOWN_HEIGHT);
  cboTapDelay->setBaseColor(gui_baseCol);
  cboTapDelay->setId("cboTapDelay");
  cboTapDelay->addActionListener(gameportActionListener);
  
  chkMouseHack = new gcn::UaeCheckBox("Enable mousehack");
  chkMouseHack->setId("MouseHack");
  chkMouseHack->addActionListener(gameportActionListener);
  if(emulating)
    chkMouseHack->setEnabled(false);
    
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

  category.panel->add(lblMouseSpeed, DISTANCE_BORDER, posY);
  category.panel->add(sldMouseSpeed, DISTANCE_BORDER + lblMouseSpeed->getWidth() + 9, posY);
  category.panel->add(lblMouseSpeedInfo, sldMouseSpeed->getX() + sldMouseSpeed->getWidth() + 12, posY);
#ifdef ANDROID
  category.panel->add(lblAutofireRate, 290, posY);
  category.panel->add(sldAutofireRate, 290 + lblAutofireRate->getWidth() + 9, posY);
#else
  category.panel->add(lblAutofireRate, 300, posY);
  category.panel->add(sldAutofireRate, 300 + lblAutofireRate->getWidth() + 9, posY);
#endif
  category.panel->add(lblAutofireRateInfo, sldAutofireRate->getX() + sldAutofireRate->getWidth() + 12, posY);
  posY += sldAutofireRate->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(chkMouseHack, cboPorts[0]->getX(), posY);
  category.panel->add(lblTapDelay, 300, posY);
  category.panel->add(cboTapDelay, 300 + lblTapDelay->getWidth() + 8, posY);
  posY += cboTapDelay->getHeight() + DISTANCE_NEXT_Y;

  values_to_dialog();
}


void ExitPanelGamePort(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete lblPort0;
  delete cboPorts[0];
  delete lblPort1;
  delete cboPorts[1];

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
  delete lblTapDelay;
  delete cboTapDelay;
  delete chkMouseHack;
  
  delete gameportActionListener;
}


bool HelpPanelGamePort(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("You can select the control type for both ports and the rate for autofire.");
  helptext.push_back(" ");
  helptext.push_back("Set the emulated mouse speed to .25x, .5x, 1x, 2x and 4x to slow down or speed up the mouse.");
  helptext.push_back(" ");
  helptext.push_back("When \"Enable mousehack\" is activated, you can use the stylus to set the mouse pointer to the exact position.");
  helptext.push_back("This works very well on Workbench, but many games using there own mouse handling and will not profit from");
  helptext.push_back("this code.");
  helptext.push_back(" ");
  helptext.push_back("\"Tap Delay\" specifies the time between taping the screen and an emulated mouse button click.");
  return true;
}
