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


static int total_devices;
static int current_device_idx = -1;
static int current_widget_idx = -1;

static gcn::Label *lblDevice;
static gcn::UaeDropDown* cboDevice;
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


static const char *device_list[12] = { NULL };
static gcn::GenericListModel *ctrlDeviceList;

static const char *amigalist[1] = { "<none>" };
static gcn::GenericListModel *ctrlAmigaList;


static void init_amiga_list()
{
  int af = 0;
  int index = 0;
  ctrlAmigaList->clear();
  ctrlAmigaList->add("<none>");
  
  if(current_device_idx >= 0 && current_widget_idx >= 0) {
    int port;
    TCHAR name1[256], name2[256];
    int aftmp;
    
		inputdevice_get_mapping (current_device_idx, current_widget_idx, NULL, &port, name1, 0);
		while(inputdevice_iterate (current_device_idx, current_widget_idx, name2, &aftmp)) {
			ctrlAmigaList->add(name2);
			if (name1 && !_tcscmp (name1, name2)) {
				index = ctrlAmigaList->getNumberOfElements() - 1;
				af = aftmp;
			}
		}
  }
  cboAmiga->setSelected(index);
  chkAF->setSelected(af != 0);
  chkAF->setEnabled(index > 0);
}

static void init_control_list(void)
{
  int i;
  
  numWidgets = inputdevice_get_widget_num (current_device_idx);
  for (i = 0; i < numWidgets; i++) {
  	uae_u64 flags;
  	int port;
 		
		inputdevice_get_widget_type (current_device_idx, i, widgets[i].widget, true);
  	inputdevice_get_mapping (current_device_idx, i, &flags, &port, widgets[i].action, 0);
  	if (flags & IDEV_MAPPED_AUTOFIRE_SET) {
  		if (flags & IDEV_MAPPED_INVERTTOGGLE)
  			_tcscpy(widgets[i].af, _T("On"));
  		else 
  			_tcscpy(widgets[i].af, _T("Yes"));
  	} else if (flags & IDEV_MAPPED_AUTOFIRE_POSSIBLE) {
 			_tcscpy(widgets[i].af, _T("No"));
  	} else {
  		_tcscpy (widgets[i].af, _T("-"));
  	}
  	if (port > 0) {
  		TCHAR tmp[256];
  		_tcscpy (tmp, widgets[i].action);
  		_stprintf (widgets[i].action, _T("[PORT%d] %s"), port, tmp);
  	}
  }
}


class InputActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cboDevice) {
        current_device_idx = cboDevice->getSelected();
        init_control_list();
      
      } else if(actionEvent.getSource() == lstWidget) {
        current_widget_idx = lstWidget->getSelected();
        init_amiga_list();
        
      } else if(actionEvent.getSource() == cboAmiga) {
        if(current_device_idx >= 0 && current_widget_idx >= 0) {
          int idx = cboAmiga->getSelected();
    			uae_u64 flags;
          inputdevice_get_mapping (current_device_idx, current_widget_idx, &flags, NULL, 0, 0);
          if(idx == 0 || ctrlAmigaList->getElementAt(idx).c_str()[0] == '[') {
            inputdevice_set_mapping (current_device_idx, current_widget_idx, NULL, flags, -1, 0);
            chkAF->setEnabled(false);
  			  } else {
            inputdevice_set_mapping (current_device_idx, current_widget_idx,
  			      ctrlAmigaList->getElementAt(idx).c_str(), flags, -1, 0);
            chkAF->setEnabled(true);
			    }
			    inputdevice_updateconfig(NULL, &workprefs);
			    inputdevice_config_change ();
			    init_control_list();
        }
        
      } else if(actionEvent.getSource() == chkAF) {
        int idx = cboAmiga->getSelected();
  			uae_u64 flags;
        inputdevice_get_mapping (current_device_idx, current_widget_idx, &flags, NULL, 0, 0);
        if(chkAF->isSelected())
          flags |= IDEV_MAPPED_AUTOFIRE_SET;
        else
          flags &= ~(IDEV_MAPPED_AUTOFIRE_SET);
        inputdevice_set_mapping (current_device_idx, current_widget_idx,
		      ctrlAmigaList->getElementAt(idx).c_str(), flags, -1, 0);
		    inputdevice_updateconfig(NULL, &workprefs);
		    inputdevice_config_change ();
		    init_control_list();

      }
    }
};
static InputActionListener* inputActionListener;


static void RefreshPanelInput(void)
{
  if(current_device_idx >= 0) {
    cboDevice->setSelected(current_device_idx);
    init_control_list();
    init_amiga_list();   
  }
}


void InitPanelInput(const struct _ConfigCategory& category)
{
  int j;

  workprefs.input_selected_setting = 0;
  inputdevice_updateconfig (NULL, &workprefs);
  inputdevice_config_change ();

  total_devices = 0;
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_JOYSTICK) && total_devices < 12; j++, total_devices++)
	  device_list[total_devices] = inputdevice_get_device_name (IDTYPE_JOYSTICK, j);
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_MOUSE) && total_devices < 12; j++, total_devices++)
		device_list[total_devices] = inputdevice_get_device_name (IDTYPE_MOUSE, j);
	for (j = 0; j < inputdevice_get_device_total (IDTYPE_KEYBOARD) && total_devices < 12; j++, total_devices++)
		device_list[total_devices] = inputdevice_get_device_name (IDTYPE_KEYBOARD, j);

  ctrlDeviceList = new gcn::GenericListModel(device_list, total_devices);
  if((current_device_idx < 0 || current_device_idx >= total_devices) && total_devices > 0)
    current_device_idx = 0;

  inputActionListener = new InputActionListener();

  lblDevice = new gcn::Label("Device:");
  lblDevice->setSize(60, LABEL_HEIGHT);
  lblDevice->setAlignment(gcn::Graphics::RIGHT);
	cboDevice = new gcn::UaeDropDown(ctrlDeviceList);
  cboDevice->setSize(300, DROPDOWN_HEIGHT);
  cboDevice->setBaseColor(gui_baseCol);
  cboDevice->setId("cboDevice");
  cboDevice->addActionListener(inputActionListener);

  lblColSource = new gcn::Label("Input source");
  lblColSource->setSize(180, LABEL_HEIGHT);
  lblColSource->setAlignment(gcn::Graphics::LEFT);
  lblColTarget = new gcn::Label("Input target");
  lblColTarget->setSize(180, LABEL_HEIGHT);
  lblColTarget->setAlignment(gcn::Graphics::LEFT);
  lblColAF = new gcn::Label("AF");
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
  lstWidget->AddColumn(510, afList);
  lstWidget->setId("cboWidget");
  
  scrAreaList = new gcn::ScrollArea(lstWidget);
  scrAreaList->setFrameSize(1);
  scrAreaList->setSize(listViewWidth, listViewHeight);
#ifdef ANDROID
  scrAreaList->setScrollbarWidth(30);
#else
  scrAreaList->setScrollbarWidth(20);
#endif
  scrAreaList->setBaseColor(gui_baseCol + 0x202020);
  
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
  category.panel->add(lblDevice, DISTANCE_BORDER, posY);
  category.panel->add(cboDevice, DISTANCE_BORDER + lblDevice->getWidth() + 8, posY);
  posY += cboDevice->getHeight() + DISTANCE_NEXT_Y;

  category.panel->add(lblColSource, DISTANCE_BORDER, posY);
  category.panel->add(lblColTarget, DISTANCE_BORDER + 200, posY);
  category.panel->add(lblColAF, DISTANCE_BORDER + 510, posY);
  posY += lblColSource->getHeight();

  category.panel->add(scrAreaList, DISTANCE_BORDER, posY);
  posY += scrAreaList->getHeight() + DISTANCE_NEXT_Y;
  
  category.panel->add(lblAmiga, DISTANCE_BORDER, posY);
  category.panel->add(cboAmiga, DISTANCE_BORDER + lblAmiga->getWidth() + 8, posY);
  category.panel->add(chkAF, DISTANCE_BORDER + cboAmiga->getX() + cboAmiga->getWidth() + 8, posY);
  posY += cboAmiga->getHeight() + DISTANCE_NEXT_Y;
  
  RefreshPanelInput();
}


void ExitPanelInput(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete lblDevice;
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
  helptext.push_back("...");
  helptext.push_back(" ");
  return true;
}
