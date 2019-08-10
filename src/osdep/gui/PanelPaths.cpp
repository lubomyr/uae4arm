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

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "gui.h"
#include "gui_handling.h"

static gcn::Label *lblSystemROMs;
static gcn::TextField *txtSystemROMs;
static gcn::Button *cmdSystemROMs;
static gcn::Label *lblConfigPath;
static gcn::TextField *txtConfigPath;
static gcn::Button *cmdConfigPath;
static gcn::Button *cmdRescanROMs;


static void RefreshPanelPaths(void)
{
  char tmp[MAX_PATH];
  
  fetch_rompath(tmp, MAX_PATH);
  txtSystemROMs->setText(tmp);
  
  fetch_configurationpath(tmp, MAX_PATH);
  txtConfigPath->setText(tmp);
}


class PathsActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      char tmp[MAX_PATH];
      
      if (actionEvent.getSource() == cmdSystemROMs) {
        fetch_rompath(tmp, MAX_PATH);
        if(SelectFolder("Folder for System ROMs", tmp))
        {
          set_rompath(tmp);
          RefreshPanelPaths();
        }
        cmdSystemROMs->requestFocus();

      } else if(actionEvent.getSource() == cmdConfigPath) {
        fetch_configurationpath(tmp, MAX_PATH);
        if(SelectFolder("Folder for configuration files", tmp))
        {
          set_configurationpath(tmp);
          RefreshPanelPaths();
        }
        cmdConfigPath->requestFocus();

      } else if(actionEvent.getSource() == cmdRescanROMs) {
        RescanROMs();

      }
    }
};
static PathsActionListener* pathsActionListener;


void InitPanelPaths(const struct _ConfigCategory& category)
{
  int textFieldWidth = category.panel->getWidth() - 2 * DISTANCE_BORDER - SMALL_BUTTON_WIDTH - DISTANCE_NEXT_X;
  int yPos = DISTANCE_BORDER;
  pathsActionListener = new PathsActionListener();
  
  lblSystemROMs = new gcn::Label("System ROMs:");
  lblSystemROMs->setSize(120, LABEL_HEIGHT);
  txtSystemROMs = new gcn::TextField();
  txtSystemROMs->setSize(textFieldWidth, TEXTFIELD_HEIGHT);
  txtSystemROMs->setEnabled(false);
  cmdSystemROMs = new gcn::Button("...");
  cmdSystemROMs->setId("SystemROMs");
  cmdSystemROMs->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
  cmdSystemROMs->setBaseColor(gui_baseCol);
  cmdSystemROMs->addActionListener(pathsActionListener);

  lblConfigPath = new gcn::Label("Configuration files:");
  lblConfigPath->setSize(120, LABEL_HEIGHT);
  txtConfigPath = new gcn::TextField();
  txtConfigPath->setSize(textFieldWidth, TEXTFIELD_HEIGHT);
  txtConfigPath->setEnabled(false);
  cmdConfigPath = new gcn::Button("...");
  cmdConfigPath->setId("ConfigPath");
  cmdConfigPath->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
  cmdConfigPath->setBaseColor(gui_baseCol);
  cmdConfigPath->addActionListener(pathsActionListener);
  
  category.panel->add(lblSystemROMs, DISTANCE_BORDER, yPos);
  yPos += lblSystemROMs->getHeight();
  category.panel->add(txtSystemROMs, DISTANCE_BORDER, yPos);
  category.panel->add(cmdSystemROMs, DISTANCE_BORDER + textFieldWidth + DISTANCE_NEXT_X, yPos);
  yPos += txtSystemROMs->getHeight() + DISTANCE_NEXT_Y;
  
  category.panel->add(lblConfigPath, DISTANCE_BORDER, yPos);
  yPos += lblConfigPath->getHeight();
  category.panel->add(txtConfigPath, DISTANCE_BORDER, yPos);
  category.panel->add(cmdConfigPath, DISTANCE_BORDER + textFieldWidth + DISTANCE_NEXT_X, yPos);
  yPos += txtConfigPath->getHeight() + DISTANCE_NEXT_Y;

  cmdRescanROMs = new gcn::Button("Rescan ROMs");
  cmdRescanROMs->setSize(120, BUTTON_HEIGHT);
  cmdRescanROMs->setBaseColor(gui_baseCol);
  cmdRescanROMs->setId("RescanROMs");
  cmdRescanROMs->addActionListener(pathsActionListener);
  
  category.panel->add(cmdRescanROMs, DISTANCE_BORDER, category.panel->getHeight() - BUTTON_HEIGHT - DISTANCE_BORDER);

  RefreshPanelPaths();
}


void ExitPanelPaths(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete lblSystemROMs;
  delete txtSystemROMs;
  delete cmdSystemROMs;

  delete lblConfigPath;
  delete txtConfigPath;
  delete cmdConfigPath;
  
  delete cmdRescanROMs;

  delete pathsActionListener;
}


bool HelpPanelPaths(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("Specify the location of your kickstart roms and the folder where the configuration files should be stored.");
  helptext.push_back("Use the \"...\" button to open a dialog to choose the folder.");
  helptext.push_back(" ");
  helptext.push_back("After changing the location of the kickstart roms, click on \"Rescan ROMS\" to refresh the list of the available");
  helptext.push_back("ROMs.");
  return true;
}
