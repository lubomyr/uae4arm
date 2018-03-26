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
#include "UaeRadioButton.hpp"
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


static const char *mousespeed_list[] = { ".25", ".5", "1x", "2x", "4x" };
static const int mousespeed_values[] = { 2, 5, 10, 20, 40 };

static gcn::Label *lblPort0;
static gcn::UaeDropDown* cboPort0;
static gcn::Label *lblPort1;
static gcn::UaeDropDown* cboPort1;

static gcn::Label *lblAutofire;
static gcn::UaeDropDown* cboAutofire;
static gcn::Label* lblMouseSpeed;
static gcn::Label* lblMouseSpeedInfo;
static gcn::Slider* sldMouseSpeed;
  
static gcn::Label *lblKeyForMenu;
static gcn::UaeDropDown* KeyForMenu;
static gcn::Label *lblButtonForMenu;
static gcn::UaeDropDown* ButtonForMenu;
static gcn::Label *lblKeyForQuit;
static gcn::UaeDropDown* KeyForQuit;
static gcn::Label *lblButtonForQuit;
static gcn::UaeDropDown* ButtonForQuit;

static gcn::Label *lblNumLock;
static gcn::UaeDropDown* cboKBDLed_num;
static gcn::Label *lblScrLock;
static gcn::UaeDropDown* cboKBDLed_scr;
static gcn::Label *lblCapLock;
static gcn::UaeDropDown* cboKBDLed_cap;


class StringListModel : public gcn::ListModel
{
private:
  std::vector<std::string> values;
public:
  StringListModel(const char *entries[], int count)
  {
    for(int i=0; i<count; ++i)
      values.push_back(entries[i]);
  }

  int getNumberOfElements()
  {
    return values.size();
  }

  int AddElement(const char * Elem)
  {
    values.push_back(Elem);
    return 0;
  }

  std::string getElementAt(int i)
  {
    if(i < 0 || i >= values.size())
      return "---";
    return values[i];
  }
};


static const char *listValues[] = { "none", "POWER", "DF0", "DF1", "DF2", "DF3", "DF*", "HD", "CD" };
static StringListModel KBDLedList(listValues, 9);

static const int ControlKey_SDLKeyValues[] = { 0, SDLK_F11, SDLK_F12 };
static const char *ControlKeyValues[] = { "------------------", "F11", "F12" };
static StringListModel ControlKeyList(ControlKeyValues, 3);

static int GetControlKeyIndex(int key)
{
	int ControlKey_SDLKeyValues_Length = sizeof(ControlKey_SDLKeyValues) / sizeof(int);
	for (int i = 0; i < (ControlKey_SDLKeyValues_Length + 1); ++i)
	{
		if (ControlKey_SDLKeyValues[i] == key)
			return i;
	}
	return 0; // Default: no key
}

static const int ControlButton_SDLButtonValues[] = { -1, 0, 1, 2, 3 };
static const char *ControlButtonValues[] = { "------------------", "JoyButton0", "JoyButton1", "JoyButton2", "JoyButton3" };
static StringListModel ControlButtonList(ControlButtonValues, 5);

static int GetControlButtonIndex(int button)
{
	int ControlButton_SDLButtonValues_Length = sizeof(ControlButton_SDLButtonValues) / sizeof(int);
	for (int i = 0; i < (ControlButton_SDLButtonValues_Length + 1); ++i)
	{
		if (ControlButton_SDLButtonValues[i] == button)
			return i;
	}
	return 0; // Default: no key
}

static StringListModel ctrlPortList(NULL, 0);
static int portListIDs[MAX_INPUT_DEVICES];
static int portListModes[MAX_INPUT_DEVICES];

static const char *autofireValues[] = { "Off", "Slow", "Medium", "Fast" };
static StringListModel autofireList(autofireValues, 4);

static const char *mappingValues[] = {
  "CD32 rwd", "CD32 ffw", "CD32 play", "CD32 yellow", "CD32 green",
  "Joystick Right", "Joystick Left", "Joystick Down", "Joystick Up", 
  "Joystick fire but.2", "Joystick fire but.1", "Mouse right button", "Mouse left button",
  "------------------",
  "Arrow Up", "Arrow Down", "Arrow Left", "Arrow Right", "Numpad 0", "Numpad 1", "Numpad 2",
  "Numpad 3", "Numpad 4", "Numpad 5", "Numpad 6", "Numpad 7", "Numpad 8", "Numpad 9",
  "Numpad Enter", "Numpad /", "Numpad *", "Numpad -", "Numpad +",
  "Numpad Delete", "Numpad (", "Numpad )",
  "Space", "Backspace", "Tab", "Return", "Escape", "Delete",
  "Left Shift", "Right Shift", "CAPS LOCK", "CTRL", "Left ALT", "Right ALT",
  "Left Amiga Key", "Right Amiga Key", "Help", "Left Bracket", "Right Bracket",
  "Semicolon", "Comma", "Period", "Slash", "Backslash", "Quote", "#", 
  "</>", "Backquote", "-", "=",
  "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", 
  "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
  "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "NULL"
};
static StringListModel mappingList(mappingValues, 110);
static int amigaKey[] = 
 { REMAP_CD32_RWD,  REMAP_CD32_FFW, REMAP_CD32_PLAY, REMAP_CD32_YELLOW, REMAP_CD32_GREEN,
   REMAP_JOY_RIGHT, REMAP_JOY_LEFT, REMAP_JOY_DOWN,  REMAP_JOY_UP,      REMAP_JOYBUTTON_TWO, REMAP_JOYBUTTON_ONE, REMAP_MOUSEBUTTON_RIGHT, REMAP_MOUSEBUTTON_LEFT,
   0,             AK_UP,    AK_DN,      AK_LF,    AK_RT,        AK_NP0,       AK_NP1,         AK_NP2,       /*  13 -  20 */
   AK_NP3,        AK_NP4,   AK_NP5,     AK_NP6,   AK_NP7,       AK_NP8,       AK_NP9,         AK_ENT,       /*  21 -  28 */
   AK_NPDIV,      AK_NPMUL, AK_NPSUB,   AK_NPADD, AK_NPDEL,     AK_NPLPAREN,  AK_NPRPAREN,    AK_SPC,       /*  29 -  36 */
   AK_BS,         AK_TAB,   AK_RET,     AK_ESC,   AK_DEL,       AK_LSH,       AK_RSH,         AK_CAPSLOCK,  /*  37 -  44 */
   AK_CTRL,       AK_LALT,  AK_RALT,    AK_LAMI,  AK_RAMI,      AK_HELP,      AK_LBRACKET,    AK_RBRACKET,  /*  45 -  52 */
   AK_SEMICOLON,  AK_COMMA, AK_PERIOD,  AK_SLASH, AK_BACKSLASH, AK_QUOTE,     AK_NUMBERSIGN,  AK_LTGT,      /*  53 -  60 */
   AK_BACKQUOTE,  AK_MINUS, AK_EQUAL,   AK_A,     AK_B,         AK_C,         AK_D,           AK_E,         /*  61 -  68 */
   AK_F,          AK_G,     AK_H,       AK_I,     AK_J,         AK_K,         AK_L,           AK_M,         /*  69 -  76 */
   AK_N,          AK_O,     AK_P,       AK_Q,     AK_R,         AK_S,         AK_T,           AK_U,         /*  77 -  84 */
   AK_V,          AK_W,     AK_X,       AK_Y,     AK_Z,         AK_1,         AK_2,           AK_3,         /*  85 -  92 */
   AK_4,          AK_5,     AK_6,       AK_7,     AK_8,         AK_9,         AK_0,           AK_F1,        /*  93 - 100 */
   AK_F2,         AK_F3,    AK_F4,      AK_F5,    AK_F6,        AK_F7,        AK_F8,          AK_F9,        /* 101 - 108 */
   AK_F10,        0 }; /*  109 - 110 */

static int GetAmigaKeyIndex(int key)
{
  for(int i=0; i < 110; ++i) {
    if(amigaKey[i] == key)
      return i;
  }
  return 13; // Default: no key
}


class InputActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cboPort0) {
        // Handle new device in port 0
        int sel = cboPort0->getSelected();
        changed_prefs.jports[0].id = portListIDs[sel];
        changed_prefs.jports[0].mode = portListModes[sel];
        inputdevice_updateconfig(NULL, &changed_prefs);
      }
      
      else if (actionEvent.getSource() == cboPort1) {
        // Handle new device in port 1
        int sel = cboPort1->getSelected();
        changed_prefs.jports[1].id = portListIDs[sel];
        changed_prefs.jports[1].mode = portListModes[sel];
        inputdevice_updateconfig(NULL, &changed_prefs);
      }
      
      else if (actionEvent.getSource() == cboAutofire)
      {
        if(cboAutofire->getSelected() == 0)
          changed_prefs.input_autofire_linecnt = 0;
        else if(cboAutofire->getSelected() == 1)
          changed_prefs.input_autofire_linecnt = 12 * 312;
        else if (cboAutofire->getSelected() == 2)
          changed_prefs.input_autofire_linecnt = 8 * 312;
        else
          changed_prefs.input_autofire_linecnt = 4 * 312;
      }
      
 	    else if (actionEvent.getSource() == sldMouseSpeed)
 	    {
    		changed_prefs.input_joymouse_multiplier = mousespeed_values[(int)(sldMouseSpeed->getValue())];
    		RefreshPanelInput();
    	}

	    else if (actionEvent.getSource() == KeyForMenu)
		    changed_prefs.key_for_menu = ControlKey_SDLKeyValues[KeyForMenu->getSelected()];
        
	    else if (actionEvent.getSource() == KeyForQuit)
		    changed_prefs.key_for_quit = ControlKey_SDLKeyValues[KeyForQuit->getSelected()];

	    else if (actionEvent.getSource() == ButtonForMenu)
		    changed_prefs.button_for_menu = ControlButton_SDLButtonValues[ButtonForMenu->getSelected()];
        
	    else if (actionEvent.getSource() == ButtonForQuit)
		    changed_prefs.button_for_quit = ControlButton_SDLButtonValues[ButtonForQuit->getSelected()];

      else if (actionEvent.getSource() == cboKBDLed_num)
        changed_prefs.kbd_led_num = cboKBDLed_num->getSelected();

      else if (actionEvent.getSource() == cboKBDLed_scr)
  			changed_prefs.kbd_led_scr = cboKBDLed_scr->getSelected();
    }
};
static InputActionListener* inputActionListener;


void InitPanelInput(const struct _ConfigCategory& category)
{
  inputActionListener = new InputActionListener();

  if(ctrlPortList.getNumberOfElements() == 0) {
    int i, idx = 0;
    ctrlPortList.AddElement("none");
    portListIDs[idx] = -1;
    portListModes[idx] = JSEM_MODE_DEFAULT;
    for(i = 0; i < inputdevice_get_device_total (IDTYPE_MOUSE); i++)
    {
      ctrlPortList.AddElement(inputdevice_get_device_name(IDTYPE_MOUSE, i));
      idx++;
      portListIDs[idx] = JSEM_MICE + i;
      portListModes[idx] = JSEM_MODE_MOUSE;
    }
    for(i=0; i < inputdevice_get_device_total (IDTYPE_JOYSTICK); i++)
    {
      ctrlPortList.AddElement(inputdevice_get_device_name(IDTYPE_JOYSTICK, i));
      idx++;
      portListIDs[idx] = JSEM_JOYS + i;
      portListModes[idx] = JSEM_MODE_JOYSTICK;
    }
  }
  
  lblPort0 = new gcn::Label("Port0:");
  lblPort0->setSize(100, LABEL_HEIGHT);
  lblPort0->setAlignment(gcn::Graphics::RIGHT);
	cboPort0 = new gcn::UaeDropDown(&ctrlPortList);
  cboPort0->setSize(435, DROPDOWN_HEIGHT);
  cboPort0->setBaseColor(gui_baseCol);
  cboPort0->setId("cboPort0");
  cboPort0->addActionListener(inputActionListener);

  lblPort1 = new gcn::Label("Port1:");
  lblPort1->setSize(100, LABEL_HEIGHT);
  lblPort1->setAlignment(gcn::Graphics::RIGHT);
	cboPort1 = new gcn::UaeDropDown(&ctrlPortList);
  cboPort1->setSize(435, DROPDOWN_HEIGHT);
  cboPort1->setBaseColor(gui_baseCol);
  cboPort1->setId("cboPort1");
  cboPort1->addActionListener(inputActionListener);

  lblAutofire = new gcn::Label("Autofire Rate:");
  lblAutofire->setSize(100, LABEL_HEIGHT);
  lblAutofire->setAlignment(gcn::Graphics::RIGHT);
	cboAutofire = new gcn::UaeDropDown(&autofireList);
  cboAutofire->setSize(80, DROPDOWN_HEIGHT);
  cboAutofire->setBaseColor(gui_baseCol);
  cboAutofire->setId("cboAutofire");
  cboAutofire->addActionListener(inputActionListener);

	lblMouseSpeed = new gcn::Label("Mouse Speed:");
  lblMouseSpeed->setSize(100, LABEL_HEIGHT);
  lblMouseSpeed->setAlignment(gcn::Graphics::RIGHT);
  sldMouseSpeed = new gcn::Slider(0, 4);
  sldMouseSpeed->setSize(110, SLIDER_HEIGHT);
  sldMouseSpeed->setBaseColor(gui_baseCol);
	sldMouseSpeed->setMarkerLength(20);
	sldMouseSpeed->setStepLength(1);
	sldMouseSpeed->setId("MouseSpeed");
  sldMouseSpeed->addActionListener(inputActionListener);
  lblMouseSpeedInfo = new gcn::Label(".25");

  lblNumLock = new gcn::Label("NumLock LED:");
  lblNumLock->setSize(100, LABEL_HEIGHT);
  lblNumLock->setAlignment(gcn::Graphics::RIGHT);
  cboKBDLed_num = new gcn::UaeDropDown(&KBDLedList);
  cboKBDLed_num->setSize(150, DROPDOWN_HEIGHT);
  cboKBDLed_num->setBaseColor(gui_baseCol);
  cboKBDLed_num->setId("numlock");
  cboKBDLed_num->addActionListener(inputActionListener);

//  lblCapLock = new gcn::Label("CapsLock LED:");
//  lblCapLock->setSize(100, LABEL_HEIGHT);
//  lblCapLock->setAlignment(gcn::Graphics::RIGHT);
//  cboKBDLed_cap = new gcn::UaeDropDown(&KBDLedList);
//  cboKBDLed_cap->setSize(150, DROPDOWN_HEIGHT);
//  cboKBDLed_cap->setBaseColor(gui_baseCol);
//  cboKBDLed_cap->setId("capslock");
//  cboKBDLed_cap->addActionListener(inputActionListener);

  lblScrLock = new gcn::Label("ScrollLock LED:");
  lblScrLock->setSize(100, LABEL_HEIGHT);
  lblScrLock->setAlignment(gcn::Graphics::RIGHT);
  cboKBDLed_scr = new gcn::UaeDropDown(&KBDLedList);
  cboKBDLed_scr->setSize(150, DROPDOWN_HEIGHT);
  cboKBDLed_scr->setBaseColor(gui_baseCol);
  cboKBDLed_scr->setId("scrolllock");
  cboKBDLed_scr->addActionListener(inputActionListener);

	lblKeyForMenu = new gcn::Label("Menu Key:");
	lblKeyForMenu->setSize(100, LABEL_HEIGHT);
	lblKeyForMenu->setAlignment(gcn::Graphics::RIGHT);
	KeyForMenu = new gcn::UaeDropDown(&ControlKeyList);
	KeyForMenu->setSize(150, DROPDOWN_HEIGHT);
	KeyForMenu->setBaseColor(gui_baseCol);
	KeyForMenu->setId("KeyForMenu");
	KeyForMenu->addActionListener(inputActionListener);

	lblKeyForQuit = new gcn::Label("Quit Key:");
	lblKeyForQuit->setSize(100, LABEL_HEIGHT);
	lblKeyForQuit->setAlignment(gcn::Graphics::RIGHT);
	KeyForQuit = new gcn::UaeDropDown(&ControlKeyList);
	KeyForQuit->setSize(150, DROPDOWN_HEIGHT);
	KeyForQuit->setBaseColor(gui_baseCol);
	KeyForQuit->setId("KeyForQuit");
	KeyForQuit->addActionListener(inputActionListener);

	lblButtonForMenu = new gcn::Label("Menu Button:");
	lblButtonForMenu->setSize(100, LABEL_HEIGHT);
	lblButtonForMenu->setAlignment(gcn::Graphics::RIGHT);
	ButtonForMenu = new gcn::UaeDropDown(&ControlButtonList);
	ButtonForMenu->setSize(150, DROPDOWN_HEIGHT);
	ButtonForMenu->setBaseColor(gui_baseCol);
	ButtonForMenu->setId("ButtonForMenu");
	ButtonForMenu->addActionListener(inputActionListener);

	lblButtonForQuit = new gcn::Label("Quit Button:");
	lblButtonForQuit->setSize(100, LABEL_HEIGHT);
	lblButtonForQuit->setAlignment(gcn::Graphics::RIGHT);
	ButtonForQuit = new gcn::UaeDropDown(&ControlButtonList);
	ButtonForQuit->setSize(150, DROPDOWN_HEIGHT);
	ButtonForQuit->setBaseColor(gui_baseCol);
	ButtonForQuit->setId("ButtonForQuit");
	ButtonForQuit->addActionListener(inputActionListener);

  int posY = DISTANCE_BORDER;
  category.panel->add(lblPort0, DISTANCE_BORDER, posY);
  category.panel->add(cboPort0, DISTANCE_BORDER + lblPort0->getWidth() + 8, posY);
  posY += cboPort0->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblPort1, DISTANCE_BORDER, posY);
  category.panel->add(cboPort1, DISTANCE_BORDER + lblPort1->getWidth() + 8, posY);
  posY += cboPort1->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblAutofire, DISTANCE_BORDER, posY);
  category.panel->add(cboAutofire, DISTANCE_BORDER + lblAutofire->getWidth() + 8, posY);
  posY += cboAutofire->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(lblMouseSpeed, DISTANCE_BORDER, posY);
  category.panel->add(sldMouseSpeed, DISTANCE_BORDER + lblMouseSpeed->getWidth() + 8, posY);
  category.panel->add(lblMouseSpeedInfo, sldMouseSpeed->getX() + sldMouseSpeed->getWidth() + 12, posY);
  posY += sldMouseSpeed->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(lblNumLock, DISTANCE_BORDER, posY);
	category.panel->add(cboKBDLed_num, DISTANCE_BORDER + lblNumLock->getWidth() + 8, posY);
//  category.panel->add(lblCapLock, lblNumLock->getX() + lblNumLock->getWidth() + DISTANCE_NEXT_X, posY);
//  category.panel->add(cboKBDLed_cap, cboKBDLed_num->getX() + cboKBDLed_num->getWidth() + DISTANCE_NEXT_X, posY);
	category.panel->add(lblScrLock, cboKBDLed_num->getX() + cboKBDLed_num->getWidth() + DISTANCE_NEXT_X, posY);
	category.panel->add(cboKBDLed_scr, lblScrLock->getX() + lblScrLock->getWidth() + 8, posY);
  posY += cboKBDLed_scr->getHeight() + DISTANCE_NEXT_Y;

	category.panel->add(lblKeyForMenu, DISTANCE_BORDER, posY);
	category.panel->add(KeyForMenu, DISTANCE_BORDER + lblKeyForMenu->getWidth() + 8, posY);
	category.panel->add(lblKeyForQuit, KeyForMenu->getX() + KeyForMenu->getWidth() + DISTANCE_NEXT_X, posY);
	category.panel->add(KeyForQuit, lblKeyForQuit->getX() + lblKeyForQuit->getWidth() + 8, posY);
	posY += KeyForMenu->getHeight() + DISTANCE_NEXT_Y;

	category.panel->add(lblButtonForMenu, DISTANCE_BORDER, posY);
	category.panel->add(ButtonForMenu, DISTANCE_BORDER + lblButtonForMenu->getWidth() + 8, posY);
	category.panel->add(lblButtonForQuit, ButtonForMenu->getX() + ButtonForMenu->getWidth() + DISTANCE_NEXT_X, posY);
	category.panel->add(ButtonForQuit, lblButtonForQuit->getX() + lblButtonForQuit->getWidth() + 8, posY);
	posY += ButtonForMenu->getHeight() + DISTANCE_NEXT_Y;


  RefreshPanelInput();
}


void ExitPanelInput(void)
{
  delete lblPort0;
  delete cboPort0;
  delete lblPort1;
  delete cboPort1;
  
  delete lblAutofire;
  delete cboAutofire;
  delete lblMouseSpeed;
  delete sldMouseSpeed;
  delete lblMouseSpeedInfo;

  delete lblCapLock;
  delete lblScrLock;
  delete lblNumLock;
  delete cboKBDLed_num;
  delete cboKBDLed_cap;
  delete cboKBDLed_scr;

	delete lblKeyForMenu;
	delete KeyForMenu;
	delete lblKeyForQuit;
	delete KeyForQuit;
	delete lblButtonForMenu;
	delete ButtonForMenu;
	delete lblButtonForQuit;
	delete ButtonForQuit;

  delete inputActionListener;
}


void RefreshPanelInput(void)
{
  int i, idx;

  // Set current device in port 0
  idx = 0;
  for(i = 0; i < ctrlPortList.getNumberOfElements(); ++i) {
    if(changed_prefs.jports[0].id == portListIDs[i]) {
      idx = i;
      break;
    }
  }
  cboPort0->setSelected(idx); 
  
  // Set current device in port 1
  idx = 0;
  for(i = 0; i < ctrlPortList.getNumberOfElements(); ++i) {
    if(changed_prefs.jports[1].id == portListIDs[i]) {
      idx = i;
      break;
    }
  }
  cboPort1->setSelected(idx); 

	if (changed_prefs.input_autofire_linecnt == 0)
	  cboAutofire->setSelected(0);
	else if (changed_prefs.input_autofire_linecnt > 10 * 312)
    cboAutofire->setSelected(1);
  else if (changed_prefs.input_autofire_linecnt > 6 * 312)
    cboAutofire->setSelected(2);
  else
    cboAutofire->setSelected(3);

  for(i=0; i<5; ++i)
  {
    if(changed_prefs.input_joymouse_multiplier == mousespeed_values[i])
    {
      sldMouseSpeed->setValue(i);
      lblMouseSpeedInfo->setCaption(mousespeed_list[i]);
      break;
    }
  }

	cboKBDLed_num->setSelected(changed_prefs.kbd_led_num);
	cboKBDLed_scr->setSelected(changed_prefs.kbd_led_scr);
	KeyForMenu->setSelected(GetControlKeyIndex(changed_prefs.key_for_menu));
	KeyForQuit->setSelected(GetControlKeyIndex(changed_prefs.key_for_quit));
	ButtonForMenu->setSelected(GetControlButtonIndex(changed_prefs.button_for_menu));
	ButtonForQuit->setSelected(GetControlButtonIndex(changed_prefs.button_for_quit));
}


bool HelpPanelInput(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("You can select the control type for both ports and the rate for autofire.");
  helptext.push_back("");
  helptext.push_back("Set the emulated mouse speed to .25x, .5x, 1x, 2x and 4x to slow down or speed up the mouse.");
  return true;
}
