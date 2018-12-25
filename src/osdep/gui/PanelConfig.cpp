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
#include "UaeListBox.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "blkdev.h"
#include "inputdevice.h"
#include "gui.h"
#include "gui_handling.h"
#include "GenericListModel.h"


static int ensureVisible = -1;

static gcn::Button *cmdLoad;
static gcn::Button *cmdSave;
static gcn::Button *cmdDelete;
static gcn::Label *lblName;
static gcn::TextField *txtName;
static gcn::Label *lblDesc;
static gcn::TextField *txtDesc;
static gcn::UaeListBox* lstConfigs;
static gcn::ScrollArea* scrAreaConfigs;


static gcn::GenericListModel configsList;


static void InitConfigsList(void)
{
  configsList.clear();
  for(int i=0; i<ConfigFilesList.size(); ++i) {
    char tmp[MAX_DPATH];
    strncpy(tmp, ConfigFilesList[i]->Name, MAX_DPATH - 1);
    if(strlen(ConfigFilesList[i]->Description) > 0) {
      strncat(tmp, " (", MAX_DPATH - 1);
      strncat(tmp, ConfigFilesList[i]->Description, MAX_DPATH - 1);
      strncat(tmp, ")", MAX_DPATH - 1);
    }
    configsList.add(tmp);
  }
}


static void MakeCurrentVisible(void)
{
  if(ensureVisible >= 0) {
    scrAreaConfigs->setVerticalScrollAmount(ensureVisible * 19);
    ensureVisible = -1;
  }
}


static void RefreshPanelConfig(void)
{
  ReadConfigFileList();
  InitConfigsList();

  txtName->setText(last_active_config);
  txtDesc->setText("");

  // Search current entry
  if(!txtName->getText().empty()) {
    for(int i=0; i<ConfigFilesList.size(); ++i) {
      if(!_tcscmp(ConfigFilesList[i]->Name, last_active_config)) {
        // Select current entry
        txtDesc->setText(ConfigFilesList[i]->Description);
        lstConfigs->setSelected(i);
        ensureVisible = i;
        RegisterRefreshFunc(MakeCurrentVisible);
        break;
      }
    }
  }
}


class ConfigActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      int i;
      if (actionEvent.getSource() == cmdLoad) {
        //-----------------------------------------------
        // Load selected configuration
        //-----------------------------------------------
        i = lstConfigs->getSelected();
        if(emulating) {
  			  uae_restart(-1, ConfigFilesList[i]->FullPath);
        } else {
          target_cfgfile_load(&workprefs, ConfigFilesList[i]->FullPath, 0, 0);
          strncpy(last_active_config, ConfigFilesList[i]->Name, MAX_PATH - 1);
          DisableResume();
  				inputdevice_updateconfig (NULL, &workprefs);
          RefreshPanelConfig();
        }

      } else if(actionEvent.getSource() == cmdSave) {
        //-----------------------------------------------
        // Save current configuration
        //-----------------------------------------------
        char filename[MAX_DPATH];
        if(!txtName->getText().empty())
        {
          fetch_configurationpath(filename, MAX_DPATH);
          strncat(filename, txtName->getText().c_str(), MAX_DPATH - 1);
          strncat(filename, ".uae", MAX_DPATH - 1);
          strncpy(workprefs.description, txtDesc->getText().c_str(), 255);
          if(cfgfile_save(&workprefs, filename, 0)) {
            strncpy(last_active_config, txtName->getText().c_str(), MAX_PATH - 1);
            RefreshPanelConfig();
          }
        }

      } else if(actionEvent.getSource() == cmdDelete) {
        //-----------------------------------------------
        // Delete selected config
        //-----------------------------------------------
        char msg[256];
        i = lstConfigs->getSelected();
        if(i >= 0 && strcmp(ConfigFilesList[i]->Name, OPTIONSFILENAME)) {
          snprintf(msg, 255, "Do you want to delete '%s' ?", ConfigFilesList[i]->Name);
          if(ShowMessage("Delete Configuration", msg, "", "Yes", "No")) {
            remove(ConfigFilesList[i]->FullPath);
            if(!strcmp(last_active_config, ConfigFilesList[i]->Name)) {
              txtName->setText("");
              txtDesc->setText("");
              last_active_config[0] = '\0';
            }
            ConfigFilesList.erase(ConfigFilesList.begin() + i);
            RefreshPanelConfig();
          }
          cmdDelete->requestFocus();
        }

      } else if(actionEvent.getSource() == lstConfigs) {
        int selected_item;
        selected_item = lstConfigs->getSelected();
        if(!txtName->getText().compare(ConfigFilesList[selected_item]->Name))
        {
          //-----------------------------------------------
          // Selected same config again -> load and start it
          //-----------------------------------------------
    			if(emulating) {
    			  uae_restart(0, ConfigFilesList[selected_item]->FullPath);
    			} else {
            target_cfgfile_load(&workprefs, ConfigFilesList[selected_item]->FullPath, 0, 0);
            strncpy(last_active_config, ConfigFilesList[selected_item]->Name, MAX_PATH - 1);
            DisableResume();
    				inputdevice_updateconfig (NULL, &workprefs);
            RefreshPanelConfig();
    			  uae_reset(0, 1);
    			}
    			gui_running = false;
        } else {
          txtName->setText(ConfigFilesList[selected_item]->Name);
          txtDesc->setText(ConfigFilesList[selected_item]->Description);
        }

      }
    }
};
static ConfigActionListener* configActionListener;


void InitPanelConfig(const struct _ConfigCategory& category)
{
  configActionListener = new ConfigActionListener();

  cmdLoad = new gcn::Button("Load");
  cmdLoad->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
  cmdLoad->setBaseColor(gui_baseCol);
  cmdLoad->setId("ConfigLoad");
  cmdLoad->addActionListener(configActionListener);

  cmdSave = new gcn::Button("Save");
  cmdSave->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
  cmdSave->setBaseColor(gui_baseCol);
  cmdSave->setId("ConfigSave");
  cmdSave->addActionListener(configActionListener);

  cmdDelete = new gcn::Button("Delete");
  cmdDelete->setSize(BUTTON_WIDTH, BUTTON_HEIGHT);
  cmdDelete->setBaseColor(gui_baseCol);
  cmdDelete->setId("CfgDelete");
  cmdDelete->addActionListener(configActionListener);

  int buttonX = DISTANCE_BORDER;
  int buttonY = category.panel->getHeight() - DISTANCE_BORDER - BUTTON_HEIGHT;
  category.panel->add(cmdLoad, buttonX, buttonY);
  buttonX += BUTTON_WIDTH + DISTANCE_NEXT_X;
  category.panel->add(cmdSave, buttonX, buttonY);
  buttonX += BUTTON_WIDTH + 3 * DISTANCE_NEXT_X;
  buttonX += BUTTON_WIDTH + DISTANCE_NEXT_X;
  buttonX = category.panel->getWidth() - DISTANCE_BORDER - BUTTON_WIDTH;
  category.panel->add(cmdDelete, buttonX, buttonY);

  lblName = new gcn::Label("Name:");
  lblName->setSize(90, LABEL_HEIGHT);
  lblName->setAlignment(gcn::Graphics::RIGHT);
  txtName = new gcn::TextField();
  txtName->setSize(300, TEXTFIELD_HEIGHT);
  txtName->setId("ConfigName");

  lblDesc = new gcn::Label("Description:");
  lblDesc->setSize(90, LABEL_HEIGHT);
  lblDesc->setAlignment(gcn::Graphics::RIGHT);
  txtDesc = new gcn::TextField();
  txtDesc->setSize(300, TEXTFIELD_HEIGHT);
  txtDesc->setId("ConfigDesc");
  
  lstConfigs = new gcn::UaeListBox(&configsList);
  lstConfigs->setSize(category.panel->getWidth() - 2 * DISTANCE_BORDER - 22, 232);
  lstConfigs->setBaseColor(gui_baseCol);
  lstConfigs->setWrappingEnabled(true);
  lstConfigs->setId("ConfigList");
  lstConfigs->addActionListener(configActionListener);
  
  scrAreaConfigs = new gcn::ScrollArea(lstConfigs);
#ifdef USE_SDL2
	scrAreaConfigs->setBorderSize(1);
#else
  scrAreaConfigs->setFrameSize(1);
#endif
  scrAreaConfigs->setPosition(DISTANCE_BORDER, DISTANCE_BORDER);
  scrAreaConfigs->setSize(category.panel->getWidth() - 2 * DISTANCE_BORDER - 2, 252);
  scrAreaConfigs->setScrollbarWidth(20);
  scrAreaConfigs->setBaseColor(gui_baseCol);
  category.panel->add(scrAreaConfigs);

  category.panel->add(lblName, DISTANCE_BORDER, 2 + buttonY - DISTANCE_NEXT_Y - 2 * TEXTFIELD_HEIGHT - 10);
  category.panel->add(txtName, DISTANCE_BORDER + lblName->getWidth() + 8, buttonY - DISTANCE_NEXT_Y - 2 * TEXTFIELD_HEIGHT - 10);
  category.panel->add(lblDesc, DISTANCE_BORDER, 2 + buttonY - DISTANCE_NEXT_Y - TEXTFIELD_HEIGHT);
  category.panel->add(txtDesc, DISTANCE_BORDER + lblName->getWidth() + 8, buttonY - DISTANCE_NEXT_Y - TEXTFIELD_HEIGHT);

  if(strlen(last_active_config) == 0)
    strncpy(last_active_config, OPTIONSFILENAME, MAX_PATH - 1);
  txtName->setText(last_active_config);
  txtDesc->setText(workprefs.description);
  ensureVisible = -1;
  
  RefreshPanelConfig();
}


void ExitPanelConfig(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete lstConfigs;
  delete scrAreaConfigs;
  
  delete cmdLoad;
  delete cmdSave;
  delete cmdDelete;

  delete lblName;
  delete txtName;
  delete lblDesc;
  delete txtDesc;

  delete configActionListener;
}


bool HelpPanelConfig(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("To load a configuration, select the entry in the list and then click on \"Load\". If you doubleclick on an entry");
  helptext.push_back("in the list, the emulation starts with this configuration.");
  helptext.push_back(" ");
  helptext.push_back("If you want to create a new configuration, setup all options, enter a new name in \"Name\", provide a short");
  helptext.push_back("description and then click on \"Save\".");
  helptext.push_back(" ");
  helptext.push_back("\"Delete\" will delete the selected configuration.");
  return true;
}
