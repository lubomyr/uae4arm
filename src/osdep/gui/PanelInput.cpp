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
#include "UaeListBox.hpp"
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


static const char *config_list[] = { "Configuration #1", "Configuration #2", "Configuration #3", "Game Ports Panel" };
static gcn::GenericListModel ctrlConfigList(config_list, MAX_INPUT_SETTINGS);

#define MAX_TOTAL_DEVICES 12

static gcn::UaeDropDown* cboConfig;
static gcn::UaeDropDown* cboDevice;
static gcn::UaeCheckBox* chkDevEnabled;
static gcn::Label *lblColSource;
static gcn::Label *lblColTarget;
static gcn::Label *lblColAF;
static gcn::Container* viewList = NULL;
static gcn::UaeListBox* lstWidget;
static gcn::ScrollArea* scrAreaList;
static gcn::Label *lblAmiga;
static gcn::UaeDropDown* cboAmiga;
static gcn::UaeCheckBox* chkAF;


struct listElement {
  TCHAR widget[100];
  TCHAR action[256];
  TCHAR af[32];
  TCHAR toggle[32];
  TCHAR invert[32];
  TCHAR qualifier[100];
};

static struct listElement widgets[256];
static int numWidgets = 0;


class WidgetListModel : public gcn::ListModel
{
private:
  int column;
  
public:
  WidgetListModel(int col)
  {
    column = col;
  }
  
  int getNumberOfElements()
  {
    return numWidgets;
  }
  
  std::string getElementAt(int i)
  {
    if(i < 0 || i >= numWidgets)
      return "";
    switch(column) {
      case 0:
        return widgets[i].widget;
      case 1:
        return widgets[i].action;
      case 2:
        return widgets[i].af;
    }
    return "";
  }
};
static WidgetListModel *widgetList;
static WidgetListModel *actionList;
static WidgetListModel *afList;


static const char *device_list[MAX_TOTAL_DEVICES] = { NULL };
static gcn::GenericListModel *ctrlDeviceList;

static const char *amigalist[1] = { "<none>" };
static gcn::GenericListModel *ctrlAmigaList;


static int input_selected_device = -1;
static int input_selected_widget, input_total_devices;
static int input_selected_event;
static TCHAR *eventnames[INPUTEVENT_END];

static void getqualifiername (TCHAR *p, uae_u64 mask)
{
	*p = 0;
	if (mask == IDEV_MAPPED_QUALIFIER_SPECIAL) {
		_tcscpy (p, _T("*"));
	} else if (mask == (IDEV_MAPPED_QUALIFIER_SPECIAL << 1)) {
		_tcscpy (p, _T("* [R]"));
	} else if (mask == IDEV_MAPPED_QUALIFIER_SHIFT) {
		_tcscpy (p, _T("Shift"));
	} else if (mask == (IDEV_MAPPED_QUALIFIER_SHIFT << 1)) {
		_tcscpy (p, _T("Shift [R]"));
	} else if (mask == IDEV_MAPPED_QUALIFIER_CONTROL) {
		_tcscpy (p, _T("Ctrl"));
	} else if (mask == (IDEV_MAPPED_QUALIFIER_CONTROL << 1)) {
		_tcscpy (p, _T("Ctrl [R]"));
	} else if (mask == IDEV_MAPPED_QUALIFIER_ALT) {
		_tcscpy (p, _T("Alt"));
	} else if (mask == (IDEV_MAPPED_QUALIFIER_ALT << 1)) {
		_tcscpy (p, _T("Alt [R]"));
	} else if (mask == IDEV_MAPPED_QUALIFIER_WIN) {
		_tcscpy (p, _T("Win"));
	} else if (mask == (IDEV_MAPPED_QUALIFIER_WIN << 1)) {
		_tcscpy (p, _T("Win [R]"));
	} else {
		int j;
		uae_u64 i;
		for (i = IDEV_MAPPED_QUALIFIER1, j = 0; i <= (IDEV_MAPPED_QUALIFIER8 << 1); i <<= 1, j++) {
			if (i == mask) {
				_stprintf (p, _T("%d%s"), j / 2 + 1, (j & 1) ? _T(" [R]") : _T(""));
			}
		}
	}
}

static void InitializeListView(void)
{
	if (!input_total_devices)
		return;

  numWidgets = 0;
	for (int index = 0; input_total_devices && index < inputdevice_get_widget_num (input_selected_device); index++) {
  	int i, sub, port;
  	TCHAR widname[100];
  	TCHAR name[256];
  	TCHAR af[32], toggle[32], invert[32];
  	uae_u64 flags;
  
		inputdevice_get_widget_type (input_selected_device, index, widname, true);
  	inputdevice_get_mapping (input_selected_device, index, &flags, &port, name, 0);
  	if (flags & IDEV_MAPPED_AUTOFIRE_SET) {
  		if (flags & IDEV_MAPPED_INVERTTOGGLE)
  			_tcscpy(af, "On");
  		else 
  			_tcscpy(af, "Yes");
  	} else if (flags & IDEV_MAPPED_AUTOFIRE_POSSIBLE) {
  		_tcscpy(af, "No");
  	} else {
  		_tcscpy (af, "-");
  	}
  	if (flags & IDEV_MAPPED_TOGGLE) {
  		_tcscpy(toggle, "Yes");
  	} else if (flags & IDEV_MAPPED_AUTOFIRE_POSSIBLE)
  		_tcscpy(toggle, "No");
  	else
  		_tcscpy (toggle, "-");
  	if (port > 0) {
  		TCHAR tmp[256];
  		_tcscpy (tmp, name);
  		_stprintf (name, "[PORT%d] %s", port, tmp);
  	}
  	_tcscpy (invert, "-");
  	if (flags & IDEV_MAPPED_INVERT)
  		_tcscpy (invert, "Yes");
  	if (flags & IDEV_MAPPED_SET_ONOFF) {
  		_tcscat (name, " (");
  		TCHAR val[32];
  		if ((flags & (IDEV_MAPPED_SET_ONOFF_VAL1 | IDEV_MAPPED_SET_ONOFF_VAL2)) == (IDEV_MAPPED_SET_ONOFF_VAL1 | IDEV_MAPPED_SET_ONOFF_VAL2)) {
  			_tcscpy(val, "onoff");
  		} else if (flags & IDEV_MAPPED_SET_ONOFF_VAL2) {
  			_tcscpy(val, "press");
  		} else if (flags & IDEV_MAPPED_SET_ONOFF_VAL1) {
  			_tcscpy(val, "On");
  		} else {
  			_tcscpy(val, "Off");
  		}
  		_tcscat (name, val);
  		_tcscat (name, ")");
  	}
  	
  	_tcscpy(widgets[index].widget, widname);
  	_tcscpy(widgets[index].action, name);
  	_tcscpy(widgets[index].af, af);
  	_tcscpy(widgets[index].toggle, toggle);
  	_tcscpy(widgets[index].invert, invert);
  	
  	_tcscpy (name, _T("-"));	
  	if (flags & IDEV_MAPPED_QUALIFIER_MASK) {
  		TCHAR *p;
  		p = name;
  		for (i = 0; i < MAX_INPUT_QUALIFIERS * 2; i++) {
  			uae_u64 mask = IDEV_MAPPED_QUALIFIER1 << i;
  			if (flags & mask) {
  				if (p != name)
  					*p++ = ',';
  				getqualifiername (p, mask);
  				p += _tcslen (p);
  			}
  		}
  	}
  	_tcscpy(widgets[index].qualifier, name);
  	numWidgets++;
  }
}

static void values_to_inputdlg (void)
{
  cboConfig->setSelected(workprefs.input_selected_setting);
  cboDevice->setSelected(input_selected_device);
  chkDevEnabled->setSelected(input_total_devices >= 0 && inputdevice_get_device_status (input_selected_device));
}

static void init_inputdlg_2 (void)
{
	TCHAR name1[256], name2[256];
	int cnt, index, af, aftmp, port;

  ctrlAmigaList->clear();
  ctrlAmigaList->add("<none>");
	index = 0; af = 0; port = 0;
	input_selected_event = -1;
	if (input_selected_widget >= 0) {
		inputdevice_get_mapping (input_selected_device, input_selected_widget, NULL, &port, name1, 0);
		cnt = 1;
		while(inputdevice_iterate (input_selected_device, input_selected_widget, name2, &aftmp)) {
			xfree (eventnames[cnt]);
			eventnames[cnt] = my_strdup (name2);
			if (name1 && !_tcscmp (name1, name2)) {
				index = cnt;
				af = aftmp;
			}
			cnt++;
      ctrlAmigaList->add(name2);
		}
		if (index >= 0) {
      cboAmiga->setSelected(index);
			input_selected_event = index;
			if (af)
			  chkAF->setSelected(true);
			else
			  chkAF->setSelected(false);
		}
	}
	if (input_selected_widget < 0 || workprefs.input_selected_setting == GAMEPORT_INPUT_SETTINGS || port > 0) {
    cboAmiga->setEnabled(false);
	} else {
    cboAmiga->setEnabled(true);
	}
}

static void init_inputdlg (void)
{
	int i, num;

	if (input_selected_device < 0) {
		input_selected_device = 0;
	}

	num = 0;
	for (i = 0; i < inputdevice_get_device_total (IDTYPE_JOYSTICK) && num < MAX_TOTAL_DEVICES; i++, num++) {
		TCHAR *name = inputdevice_get_device_name (IDTYPE_JOYSTICK, i);
	  device_list[num] = name;
	}
	for (i = 0; i < inputdevice_get_device_total (IDTYPE_MOUSE) && num < MAX_TOTAL_DEVICES; i++, num++) {
		TCHAR *name = inputdevice_get_device_name (IDTYPE_MOUSE, i);
	  device_list[num] = name;
	}
	for (i = 0; i < inputdevice_get_device_total (IDTYPE_KEYBOARD) && num < MAX_TOTAL_DEVICES; i++, num++) {
		TCHAR *name = inputdevice_get_device_name (IDTYPE_KEYBOARD, i);
	  device_list[num] = name;
	}
	input_total_devices = inputdevice_get_device_total (IDTYPE_JOYSTICK) +
		inputdevice_get_device_total (IDTYPE_MOUSE) +
		inputdevice_get_device_total (IDTYPE_KEYBOARD);
	if (input_selected_device >= input_total_devices || input_selected_device < 0)
		input_selected_device = 0;

	InitializeListView ();
}

static void enable_for_inputdlg (void)
{
	bool v = workprefs.input_selected_setting != GAMEPORT_INPUT_SETTINGS;
  cboAmiga->setEnabled(v);
  chkAF->setEnabled(v);
  chkDevEnabled->setEnabled(v);
}

static void clearinputlistview (void)
{
	numWidgets = 0;
}

static void values_from_inputdlg (int inputchange)
{
	int doselect = 0;
  int item;
  
  item = cboConfig->getSelected();
	if (item != workprefs.input_selected_setting) {
		workprefs.input_selected_setting = item;
		input_selected_widget = -1;
		inputdevice_updateconfig (NULL, &workprefs);
		enable_for_inputdlg ();
		InitializeListView ();
		doselect = 1;
	}
	item = cboDevice->getSelected();
	if (item != input_selected_device) {
		input_selected_device = item;
		input_selected_widget = -1;
		input_selected_event = -1;
		InitializeListView ();
		init_inputdlg_2 ();
		values_to_inputdlg ();
		doselect = 1;
	}
	item = cboAmiga->getSelected();
	if (item != input_selected_event) {
		uae_u64 flags;
		input_selected_event = item;
		doselect = 1;
		inputdevice_get_mapping (input_selected_device, input_selected_widget, &flags, NULL, 0, 0);
	}
	
	if (inputchange && doselect && input_selected_device >= 0 && input_selected_event >= 0) {
		uae_u64 flags;

		inputdevice_get_mapping (input_selected_device, input_selected_widget, &flags, NULL, 0, 0);
		inputdevice_set_mapping (input_selected_device, input_selected_widget,
			eventnames[input_selected_event],	flags, -1, 0);
		InitializeListView ();
		inputdevice_updateconfig (NULL, &workprefs);
	}
}


class InputActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cboConfig) {
				values_from_inputdlg (0);
    		inputdevice_config_change ();
      
      } else if (actionEvent.getSource() == cboDevice) {
				values_from_inputdlg (0);
    		inputdevice_config_change ();

      } else if(actionEvent.getSource() == chkDevEnabled) {
  			inputdevice_set_device_status (input_selected_device, chkDevEnabled->isSelected());
    		inputdevice_config_change ();

      } else if(actionEvent.getSource() == lstWidget) {
        input_selected_widget = lstWidget->getSelected();
        init_inputdlg_2();
        
      } else if(actionEvent.getSource() == cboAmiga) {
				values_from_inputdlg (1);
    		inputdevice_config_change ();
        
      } else if(actionEvent.getSource() == chkAF) {
        if (input_selected_device >= 0 && input_selected_widget >= 0 && input_selected_event >= 0) {
        	int evt;
        	uae_u64 flags;
        	TCHAR name[256];
          evt = inputdevice_get_mapping (input_selected_device, input_selected_widget, &flags, NULL, name, 0);
          if(chkAF->isSelected()) {
        		flags |= IDEV_MAPPED_AUTOFIRE_SET;
          } else {
        		flags &= ~(IDEV_MAPPED_INVERTTOGGLE | IDEV_MAPPED_AUTOFIRE_SET);
          }
          inputdevice_set_mapping (input_selected_device, input_selected_widget, name, flags, -1, 0);
      		InitializeListView ();
      		inputdevice_updateconfig (NULL, &workprefs);
      		inputdevice_config_change ();
        }
      }
    }
};
static InputActionListener* inputActionListener;


void InitPanelInput(const struct _ConfigCategory& category)
{
	inputdevice_updateconfig (NULL, &workprefs);
	inputdevice_config_change ();
	input_selected_widget = -1;
	init_inputdlg ();

  ctrlDeviceList = new gcn::GenericListModel(device_list, input_total_devices);

  inputActionListener = new InputActionListener();

	cboConfig = new gcn::UaeDropDown(&ctrlConfigList);
  cboConfig->setSize(140, DROPDOWN_HEIGHT);
  cboConfig->setBaseColor(gui_baseCol);
  cboConfig->setId("cboConfig");
  cboConfig->addActionListener(inputActionListener);

	cboDevice = new gcn::UaeDropDown(ctrlDeviceList);
  cboDevice->setSize(290, DROPDOWN_HEIGHT);
  cboDevice->setBaseColor(gui_baseCol);
  cboDevice->setId("cboDevice");
  cboDevice->addActionListener(inputActionListener);

	chkDevEnabled = new gcn::UaeCheckBox("Device enabled", true);
	chkDevEnabled->setId("InputDevEnabled");
  chkDevEnabled->addActionListener(inputActionListener);

  lblColSource = new gcn::Label("Input source");
  lblColSource->setSize(180, LABEL_HEIGHT);
  lblColSource->setAlignment(gcn::Graphics::LEFT);
  lblColTarget = new gcn::Label("Input target");
  lblColTarget->setSize(180, LABEL_HEIGHT);
  lblColTarget->setAlignment(gcn::Graphics::LEFT);
  lblColAF = new gcn::Label("Autofire");
  lblColAF->setSize(80, LABEL_HEIGHT);
  lblColAF->setAlignment(gcn::Graphics::LEFT);

  const int listViewWidth = category.panel->getWidth() - 2 * DISTANCE_BORDER - 2;
  const int listViewHeight = 282;
  viewList = new gcn::Container();
  viewList->setSize(listViewWidth - 20, listViewHeight);

  widgetList = new WidgetListModel(0);
  actionList = new WidgetListModel(1);
  afList = new WidgetListModel(2);

  lstWidget = new gcn::UaeListBox(widgetList);
  lstWidget->setSize(listViewWidth - 20, listViewHeight);
  lstWidget->setBaseColor(gui_baseCol);
  lstWidget->setWrappingEnabled(true);
  lstWidget->addActionListener(inputActionListener);
  lstWidget->AddColumn(200, actionList);
  lstWidget->AddColumn(500, afList);
  lstWidget->setId("cboWidget");
  lstWidget->setFont(gui_fontsmall);
  
  scrAreaList = new gcn::ScrollArea(lstWidget);
  scrAreaList->setFrameSize(1);
  scrAreaList->setSize(listViewWidth, listViewHeight);
#ifdef ANDROID
  scrAreaList->setScrollbarWidth(30);
#else
  scrAreaList->setScrollbarWidth(20);
#endif
  scrAreaList->setBaseColor(gui_baseCol + 0x202020);
  scrAreaList->setFont(gui_fontsmall);
  
  ctrlAmigaList = new gcn::GenericListModel(amigalist, 1);
  lblAmiga = new gcn::Label("Target:");
  lblAmiga->setSize(60, LABEL_HEIGHT);
  lblAmiga->setAlignment(gcn::Graphics::RIGHT);
	cboAmiga = new gcn::UaeDropDown(ctrlAmigaList);
  cboAmiga->setSize(300, DROPDOWN_HEIGHT);
  cboAmiga->setBaseColor(gui_baseCol);
  cboAmiga->setId("cboAmigaAction");
  cboAmiga->addActionListener(inputActionListener);

	chkAF = new gcn::UaeCheckBox("Autofire", true);
	chkAF->setId("InputAutofire");
  chkAF->addActionListener(inputActionListener);

  int posY = DISTANCE_BORDER;
  category.panel->add(cboConfig, DISTANCE_BORDER, posY);
  category.panel->add(cboDevice, DISTANCE_BORDER + cboConfig->getWidth() + 8, posY);
  category.panel->add(chkDevEnabled, cboDevice->getX() + cboDevice->getWidth() + 8, posY + 1);
  posY += cboDevice->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(lblColSource, DISTANCE_BORDER, posY);
  category.panel->add(lblColTarget, DISTANCE_BORDER + 200, posY);
  category.panel->add(lblColAF, DISTANCE_BORDER + 500, posY);
  posY += lblColSource->getHeight();

  category.panel->add(scrAreaList, DISTANCE_BORDER, posY);
  posY += scrAreaList->getHeight() + DISTANCE_NEXT_Y;
  
  category.panel->add(lblAmiga, DISTANCE_BORDER, posY);
  category.panel->add(cboAmiga, DISTANCE_BORDER + lblAmiga->getWidth() + 8, posY);
  category.panel->add(chkAF, DISTANCE_BORDER + cboAmiga->getX() + cboAmiga->getWidth() + 8, posY);
  posY += cboAmiga->getHeight() + DISTANCE_NEXT_Y;

	init_inputdlg_2 ();
	values_to_inputdlg ();
}


void ExitPanelInput(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete cboConfig;
  delete cboDevice;
  delete ctrlDeviceList;

  delete lblColSource;
  delete lblColTarget;
  delete lblColAF;
  
  delete scrAreaList;
  delete lstWidget;
  
  delete lblAmiga;
  delete cboAmiga;
  delete ctrlAmigaList;
  delete chkAF;
  
  delete widgetList;
  delete actionList;
  delete afList;
  
  delete inputActionListener;
}


bool HelpPanelInput(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("Select the device first, for which you want to change the default mapping. Then select the line of the available");
  helptext.push_back("button/axis in the list you want to remap.");
  helptext.push_back(" ");
  helptext.push_back("Choose the new target action in the dropdown control \"Target\" for the current selected entry.");
  return true;
}
