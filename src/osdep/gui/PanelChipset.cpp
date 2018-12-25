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
#include "UaeCheckBox.hpp"
#include "UaeDropDown.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "gui.h"
#include "include/memory-uae.h"
#include "newcpu.h"
#include "custom.h"
#include "gui_handling.h"
#include "GenericListModel.h"


static gcn::Window *grpChipset;
static gcn::UaeRadioButton* optOCS;
static gcn::UaeRadioButton* optECSAgnus;
static gcn::UaeRadioButton* optECS;
static gcn::UaeRadioButton* optAGA;
static gcn::UaeCheckBox* chkNTSC;
static gcn::Label *lblChipset;
static gcn::UaeDropDown* cboChipset;
static gcn::Window *grpBlitter;
static gcn::UaeRadioButton* optBlitNormal;
static gcn::UaeRadioButton* optBlitImmed;
static gcn::UaeRadioButton* optBlitWait;
static gcn::Window *grpCopper;
static gcn::UaeCheckBox* chkFastCopper;
static gcn::Window *grpCollisionLevel;
static gcn::UaeRadioButton* optCollNone;
static gcn::UaeRadioButton* optCollSprites;
static gcn::UaeRadioButton* optCollPlayfield;
static gcn::UaeRadioButton* optCollFull;


struct chipset {
	int compatible;
	char name[32];
};
static struct chipset chipsets [] = {
  { CP_GENERIC, "Generic" },
  { CP_CD32,    "CD32" },
  { CP_A500,    "A500" },
  { CP_A500P,   "A500+" },
  { CP_A600,    "A600" },
  { CP_A1200,   "A1200" },
  { CP_A2000,   "A2000" },
  { CP_A4000,   "A4000" },
  { -1, "" }
};
static const int numChipsets = 8;


static gcn::GenericListModel chipsetList;
static bool bIgnoreListChange = true;


static void RefreshPanelChipset(void)
{
	int i, idx;
	
	bIgnoreListChange = true;
	idx = 0;
	for(i = 0; i<numChipsets; ++i) {
	  if(chipsets[i].compatible == workprefs.cs_compatible) {
	    idx = i;
	    break;
	  }
	}
	cboChipset->setSelected(idx);
	bIgnoreListChange = false;
	
	if (workprefs.chipset_mask == 0)
    optOCS->setSelected(true);
	else if (workprefs.chipset_mask == CSMASK_ECS_AGNUS)
    optECSAgnus->setSelected(true);
  else if (workprefs.chipset_mask == (CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE))
    optECS->setSelected(true);
  else if (workprefs.chipset_mask == (CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA))
    optAGA->setSelected(true);
  
  chkNTSC->setSelected(workprefs.ntscmode);
  
  if(workprefs.immediate_blits)
    optBlitImmed->setSelected(true);
  else if(workprefs.waiting_blits)
    optBlitWait->setSelected(true);
  else
    optBlitNormal->setSelected(true);
  
  chkFastCopper->setSelected(workprefs.fast_copper);
  
  if(workprefs.collision_level == 0)
    optCollNone->setSelected(true);
  else if(workprefs.collision_level == 1)
    optCollSprites->setSelected(true);
  else if(workprefs.collision_level == 2)
    optCollPlayfield->setSelected(true);
  else
    optCollFull->setSelected(true);
}


class ChipsetActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cboChipset) {
        if (!bIgnoreListChange) {
    	    //---------------------------------------
          // Chipset selected
    	    //---------------------------------------
    	    int cs = chipsets[cboChipset->getSelected()].compatible;
    	    if(workprefs.cs_compatible != cs) {
      	    workprefs.cs_compatible = cs;
      	    built_in_chipset_prefs (&workprefs);
      	    RefreshPanelChipset();
      	  }
        }

      } else if (actionEvent.getSource() == optOCS) {
    		workprefs.chipset_mask = 0;

	    } else if (actionEvent.getSource() == optECSAgnus) {
    		workprefs.chipset_mask = CSMASK_ECS_AGNUS;

	    } else if (actionEvent.getSource() == optECS) {
    		workprefs.chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE;

	    } else if (actionEvent.getSource() == optAGA) {
    		workprefs.chipset_mask = CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA;

      }	else if (actionEvent.getSource() == chkNTSC) {
        if (chkNTSC->isSelected()) {
  	      workprefs.ntscmode = true;
  	      workprefs.chipset_refreshrate = 60;
        } else {
  	      workprefs.ntscmode = false;
  	      workprefs.chipset_refreshrate = 50;
        }

      } else if (actionEvent.getSource() == chkFastCopper) {
  	    workprefs.fast_copper = chkFastCopper->isSelected();

  	  } else if (actionEvent.getSource() == optBlitNormal) {
        workprefs.immediate_blits = false;
        workprefs.waiting_blits = false;

  	  } else if (actionEvent.getSource() == optBlitImmed) {
        workprefs.immediate_blits = optBlitImmed->isSelected();
        workprefs.waiting_blits = false;
  	    
  	  } else if (actionEvent.getSource() == optBlitWait) {
        workprefs.immediate_blits = false;
        workprefs.waiting_blits = optBlitWait->isSelected();

      } else if (actionEvent.getSource() == optCollNone) {
    		workprefs.collision_level = 0;

	    } else if (actionEvent.getSource() == optCollSprites) {
    		workprefs.collision_level = 1;

	    } else if (actionEvent.getSource() == optCollPlayfield) {
    		workprefs.collision_level = 2;

	    } else if (actionEvent.getSource() == optCollFull) {
    		workprefs.collision_level = 3;
      }

    	cfgfile_compatibility_romtype(&workprefs);
      RefreshPanelChipset();
    }
};
static ChipsetActionListener* chipsetActionListener;


void InitPanelChipset(const struct _ConfigCategory& category)
{
  chipsetList.clear();
  for(int i=0; chipsets[i].compatible >= 0; ++i)
    chipsetList.add(chipsets[i].name);
    
  chipsetActionListener = new ChipsetActionListener();
  
	optOCS = new gcn::UaeRadioButton("OCS", "radiochipsetgroup");
	optOCS->addActionListener(chipsetActionListener);

	optECSAgnus = new gcn::UaeRadioButton("ECS Agnus", "radiochipsetgroup");
	optECSAgnus->addActionListener(chipsetActionListener);

	optECS = new gcn::UaeRadioButton("Full ECS", "radiochipsetgroup");
	optECS->addActionListener(chipsetActionListener);

	optAGA = new gcn::UaeRadioButton("AGA", "radiochipsetgroup");
	optAGA->addActionListener(chipsetActionListener);

	chkNTSC = new gcn::UaeCheckBox("NTSC");
  chkNTSC->addActionListener(chipsetActionListener);

  lblChipset = new gcn::Label("Extra:");
#ifdef ANDROID
  lblChipset->setSize(42, LABEL_HEIGHT);
#else
  lblChipset->setSize(40, LABEL_HEIGHT);
#endif
  lblChipset->setAlignment(gcn::Graphics::RIGHT);
	cboChipset = new gcn::UaeDropDown(&chipsetList);
#ifdef ANDROID
  cboChipset->setSize(78, DROPDOWN_HEIGHT);
#else
  cboChipset->setSize(75, DROPDOWN_HEIGHT);
#endif
  cboChipset->setBaseColor(gui_baseCol);
  cboChipset->setId("ChipsetExtra");
  cboChipset->addActionListener(chipsetActionListener);
  
	grpChipset = new gcn::Window("Chipset");
	grpChipset->setPosition(DISTANCE_BORDER, DISTANCE_BORDER);
	grpChipset->add(optOCS, 5, 10);
	grpChipset->add(optECSAgnus, 5, 40);
	grpChipset->add(optECS, 5, 70);
	grpChipset->add(optAGA, 5, 100);
	grpChipset->add(chkNTSC, 5, 140);
	grpChipset->add(lblChipset, 115, 10);
	grpChipset->add(cboChipset, 115 + lblChipset->getWidth() + 8, 10);

	grpChipset->setMovable(false);
#ifdef ANDROID
	grpChipset->setSize(260, 185);
#else
	grpChipset->setSize(255, 185);
#endif
  grpChipset->setBaseColor(gui_baseCol);
  
  category.panel->add(grpChipset);

	optBlitNormal = new gcn::UaeRadioButton("Normal", "radiocblittergroup");
	optBlitNormal->setId("BlitNormal");
	optBlitNormal->addActionListener(chipsetActionListener);

	optBlitImmed = new gcn::UaeRadioButton("Immediate", "radiocblittergroup");
	optBlitImmed->addActionListener(chipsetActionListener);

	optBlitWait = new gcn::UaeRadioButton("Wait for blit.", "radiocblittergroup");
	optBlitWait->setId("BlitWait");
	optBlitWait->addActionListener(chipsetActionListener);

	grpBlitter = new gcn::Window("Blitter");
	grpBlitter->setPosition(DISTANCE_BORDER + grpChipset->getWidth() + DISTANCE_NEXT_X, DISTANCE_BORDER);
	grpBlitter->add(optBlitNormal, 5, 10);
	grpBlitter->add(optBlitImmed, 5, 40);
	grpBlitter->add(optBlitWait, 5, 70);
	grpBlitter->setMovable(false);
#ifdef ANDROID
	grpBlitter->setSize(125, 115);
#else
	grpBlitter->setSize(120, 115);
#endif
  grpBlitter->setBaseColor(gui_baseCol);
  
  category.panel->add(grpBlitter);
  
	chkFastCopper = new gcn::UaeCheckBox("Fast copper");
  chkFastCopper->addActionListener(chipsetActionListener);

	grpCopper = new gcn::Window("Copper");
	grpCopper->setPosition(DISTANCE_BORDER + grpChipset->getWidth() + DISTANCE_NEXT_X, grpBlitter->getY() + grpBlitter->getHeight() + DISTANCE_NEXT_Y);
	grpCopper->add(chkFastCopper, 5, 10);
	grpCopper->setMovable(false);
#ifdef ANDROID
	grpCopper->setSize(130, 55);
#else
	grpCopper->setSize(120, 55);
#endif
  grpCopper->setBaseColor(gui_baseCol);

  category.panel->add(grpCopper);

	optCollNone = new gcn::UaeRadioButton("None", "radioccollisiongroup");
	optCollNone->setId("CollNone");
	optCollNone->addActionListener(chipsetActionListener);

	optCollSprites = new gcn::UaeRadioButton("Sprites only", "radioccollisiongroup");
	optCollSprites->addActionListener(chipsetActionListener);

	optCollPlayfield = new gcn::UaeRadioButton("Sprites and Sprites vs. Playfield", "radioccollisiongroup");
	optCollPlayfield->setId("CollPlay");
	optCollPlayfield->addActionListener(chipsetActionListener);

	optCollFull = new gcn::UaeRadioButton("Full (rarely needed)", "radioccollisiongroup");
	optCollFull->setId("CollFull");
	optCollFull->addActionListener(chipsetActionListener);

	grpCollisionLevel = new gcn::Window("Collision Level");
	grpCollisionLevel->setPosition(DISTANCE_BORDER, DISTANCE_BORDER + grpChipset->getHeight() + DISTANCE_NEXT_Y);
	grpCollisionLevel->add(optCollNone, 5, 10);
	grpCollisionLevel->add(optCollSprites, 5, 40);
	grpCollisionLevel->add(optCollPlayfield, 5, 70);
	grpCollisionLevel->add(optCollFull, 5, 100);
	grpCollisionLevel->setMovable(false);
#ifdef ANDROID
	grpCollisionLevel->setSize(270, 145);
#else
	grpCollisionLevel->setSize(250, 145);
#endif
  grpCollisionLevel->setBaseColor(gui_baseCol);
  
  category.panel->add(grpCollisionLevel);

  RefreshPanelChipset();
}


void ExitPanelChipset(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete optOCS;
  delete optECSAgnus;
  delete optECS;
  delete optAGA;
  delete chkNTSC;
  delete lblChipset;
  delete cboChipset;
  delete grpChipset;

  delete optBlitNormal;
  delete optBlitImmed;
  delete optBlitWait;
  delete grpBlitter;
  
  delete chkFastCopper;
  delete grpCopper;
  
  delete optCollNone;
  delete optCollSprites;
  delete optCollPlayfield;
  delete optCollFull;
  delete grpCollisionLevel;

  delete chipsetActionListener;
}


bool HelpPanelChipset(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("If you want to emulate an Amiga 1200, select AGA. For most Amiga 500 games, select \"Full ECS\". Some older");
  helptext.push_back("Amiga games requires \"OCS\" or \"ECS Agnus\". You have to play with these options if a game won't work as");
  helptext.push_back("expected. By selecting an entry in \"Extra\", all internal chipset settings will become the required values for the specified");
  helptext.push_back("Amiga model.");
  helptext.push_back("For some games, you have to activate \"NTSC\" (60 Hz instead of 50 Hz) for correct timing.");
  helptext.push_back(" ");
  helptext.push_back("When you see some graphic issues in a game, try \"Immediate\" or \"Wait for blit.\" for blitter and/or disable");
  helptext.push_back("\"Fast copper\".");
  helptext.push_back(" ");
  helptext.push_back("\"Fast copper\" uses a prediction algorithm instead of checking the copper state on a more regular basis. This may");
  helptext.push_back("cause issues but brings a big performance improvement. The option was removed in WinUAE in an early state,");
  helptext.push_back("but for most games, it works fine and the better performance is helpful for low powered devices.");
  helptext.push_back(" ");
  helptext.push_back("For \"Collision Level\", select \"Sprites and Sprites vs. Playfield\" which is fine for nearly all games.");
  return true;
}
