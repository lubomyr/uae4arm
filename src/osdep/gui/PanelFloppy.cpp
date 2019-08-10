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
#include "UaeCheckBox.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "include/memory-uae.h"
#include "disk.h"
#include "uae.h"
#include "gui.h"
#include "gui_handling.h"
#include "GenericListModel.h"


static gcn::UaeCheckBox* chkDFx[4];
static gcn::UaeDropDown* cboDFxType[4];
static gcn::UaeCheckBox* chkDFxWriteProtect[4];
static gcn::Button* cmdDFxInfo[4];
static gcn::Button* cmdDFxEject[4];
static gcn::Button* cmdDFxSelect[4];
static gcn::UaeDropDown* cboDFxFile[4];
static gcn::Label* lblDriveSpeed;
static gcn::Label* lblDriveSpeedInfo;
static gcn::Slider* sldDriveSpeed;
static gcn::UaeCheckBox* chkLoadConfig;
static gcn::Button *cmdSaveForDisk;
static gcn::Button *cmdCreateDDDisk;
static gcn::Button *cmdCreateHDDisk;

static const char *diskfile_filter[] = { ".adf", ".adz", ".ipf", ".fdi", ".zip", ".dms", ".gz", ".xz", "\0" };
static const char *drivespeedlist[] = { "100% (compatible)", "200%", "400%", "800%" };
static const int drivespeedvalues[] = { 100, 200, 400, 800 };

static void AdjustDropDownControls(void);
static bool bLoadConfigForDisk = false;
static bool bIgnoreListChange = false;


static gcn::GenericListModel driveTypeList;
static gcn::GenericListModel diskfileList;


static bool LoadConfigByName(const char *name)
{
  ConfigFileInfo* config = SearchConfigInList(name);
  if(config != NULL) {
    if(emulating) {
		  uae_restart(-1, config->FullPath);
    } else {
      target_cfgfile_load(&workprefs, config->FullPath, 0, 0);
      strncpy(last_active_config, config->Name, MAX_PATH - 1);
      DisableResume();
    }
  }

  return false;
}


static void AdjustDropDownControls(void)
{
  int i, j;
  
  bIgnoreListChange = true;
  
  for(i = 0; i < 4; ++i) {
    cboDFxFile[i]->clearSelected();

    if((workprefs.floppyslots[i].dfxtype != DRV_NONE) && strlen(workprefs.floppyslots[i].df) > 0) {
      for(j=0; j<lstMRUDiskList.size(); ++j) {
        if(!lstMRUDiskList[j].compare(workprefs.floppyslots[i].df)) {
          cboDFxFile[i]->setSelected(j);
          break;
        }
      }
    }
  }
       
  bIgnoreListChange = false;
}


static void RefreshPanelFloppy(void)
{
  int i;
  bool prevAvailable = true;
  
  AdjustDropDownControls();

  workprefs.nr_floppies = 0;
  for(i = 0; i < 4; ++i) {
    bool driveEnabled = workprefs.floppyslots[i].dfxtype != DRV_NONE;
    chkDFx[i]->setSelected(driveEnabled);
    cboDFxType[i]->setSelected(workprefs.floppyslots[i].dfxtype + 1);
    chkDFxWriteProtect[i]->setSelected(disk_getwriteprotect(&workprefs, workprefs.floppyslots[i].df));
    chkDFx[i]->setEnabled(prevAvailable);
    cboDFxType[i]->setEnabled(prevAvailable);
    
	  chkDFxWriteProtect[i]->setEnabled(driveEnabled && !workprefs.floppy_read_only);
    cmdDFxInfo[i]->setEnabled(driveEnabled);
    cmdDFxEject[i]->setEnabled(driveEnabled);
    cmdDFxSelect[i]->setEnabled(driveEnabled);
    cboDFxFile[i]->setEnabled(driveEnabled);
    
    prevAvailable = driveEnabled;
    if(driveEnabled)
      workprefs.nr_floppies = i + 1;
  }

  chkLoadConfig->setSelected(bLoadConfigForDisk);
  
  for(i = 0; i < 4; ++i) {
    if(workprefs.floppy_speed == drivespeedvalues[i]) {
      sldDriveSpeed->setValue(i);
      lblDriveSpeedInfo->setCaption(drivespeedlist[i]);
      break;
    }
  }
}


static void RefreshDiskListModel(void)
{
  diskfileList.clear();
  for(int i = 0; i < lstMRUDiskList.size(); ++i)
    diskfileList.add(lstMRUDiskList[i]);
}


class DFxActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    if(actionEvent.getSource() == chkLoadConfig) {
	      bLoadConfigForDisk = chkLoadConfig->isSelected();

	    } else if(actionEvent.getSource() == sldDriveSpeed) {
    		workprefs.floppy_speed = drivespeedvalues[(int)sldDriveSpeed->getValue()];

      } else if(actionEvent.getSource() == cmdSaveForDisk) {
        //---------------------------------------
        // Save configuration for current disk
        //---------------------------------------
        if(strlen(workprefs.floppyslots[0].df) > 0) {
          char filename[MAX_DPATH];
          char diskname[MAX_PATH];
          
          extractFileName(workprefs.floppyslots[0].df, diskname);
          removeFileExtension(diskname);
          
          fetch_configurationpath(filename, MAX_DPATH);
          strncat(filename, diskname, MAX_DPATH - 1);
          strncat(filename, ".uae", MAX_DPATH - 1);
          
  			  snprintf(workprefs.description, sizeof (workprefs.description) - 1, "Configuration for disk '%s'", diskname);
          if(cfgfile_save(&workprefs, filename, 0))
            ReadConfigFileList();
        }

      } else if(actionEvent.getSource() == cmdCreateDDDisk) {
        // Create 3.5'' DD Disk
        char tmp[MAX_PATH];
        char diskname[MAX_PATH];
        strncpy(tmp, currentDir, MAX_PATH - 1);
        if(SelectFile("Create 3.5'' DD disk file", tmp, diskfile_filter, true))
        {
          extractFileName(tmp, diskname);
          removeFileExtension(diskname);
          diskname[31] = '\0';
          disk_creatediskfile(&workprefs, tmp, 0, DRV_35_DD, -1, diskname, false, false, NULL);
    	    AddFileToDiskList(tmp, 1);
          RefreshDiskListModel();
    	    extractPath(tmp, currentDir);
        }
        cmdCreateDDDisk->requestFocus();

      } else if(actionEvent.getSource() == cmdCreateHDDisk) {
        // Create 3.5'' HD Disk
        char tmp[MAX_PATH];
        char diskname[MAX_PATH];
        strncpy(tmp, currentDir, MAX_PATH - 1);
        if(SelectFile("Create 3.5'' HD disk file", tmp, diskfile_filter, true))
        {
          extractFileName(tmp, diskname);
          removeFileExtension(diskname);
          diskname[31] = '\0';
          disk_creatediskfile(&workprefs, tmp, 0, DRV_35_HD, -1, diskname, false, false, NULL);
    	    AddFileToDiskList(tmp, 1);
          RefreshDiskListModel();
    	    extractPath(tmp, currentDir);
        }
        cmdCreateHDDisk->requestFocus();

	    } else {
  	    for(int i = 0; i < 4; ++i) {

  	      if (actionEvent.getSource() == cboDFxType[i]) {
            workprefs.floppyslots[i].dfxtype = cboDFxType[i]->getSelected() - 1;

  	      } else if (actionEvent.getSource() == chkDFx[i]) {
      	    //---------------------------------------
            // Drive enabled/disabled
      	    //---------------------------------------
            if(chkDFx[i]->isSelected())
              workprefs.floppyslots[i].dfxtype = DRV_35_DD;
            else
              workprefs.floppyslots[i].dfxtype = DRV_NONE;

          } else if(actionEvent.getSource() == chkDFxWriteProtect[i]) {
      	    //---------------------------------------
            // Write-protect changed
      	    //---------------------------------------
            disk_setwriteprotect (&workprefs, i, workprefs.floppyslots[i].df, chkDFxWriteProtect[i]->isSelected());
            if(disk_getwriteprotect(&workprefs, workprefs.floppyslots[i].df) != chkDFxWriteProtect[i]->isSelected()) {
              // Failed to change write protection -> maybe filesystem doesn't support this
              chkDFxWriteProtect[i]->setSelected(!chkDFxWriteProtect[i]->isSelected());
              ShowMessage("Set/Clear write protect", "Failed to change write permission.", "Maybe underlying filesystem doesn't support this.", "Ok", "");
            }
            DISK_reinsert(i);

          } else if (actionEvent.getSource() == cmdDFxInfo[i]) {
      	    //---------------------------------------
            // Show info about current disk-image
      	    //---------------------------------------
            if(workprefs.floppyslots[i].dfxtype != DRV_NONE && strlen(workprefs.floppyslots[i].df) > 0)
            ; // ToDo: Show info dialog

          } else if (actionEvent.getSource() == cmdDFxEject[i]) {
      	    //---------------------------------------
            // Eject disk from drive
      	    //---------------------------------------
            disk_eject(i);
            strncpy(workprefs.floppyslots[i].df, "", MAX_DPATH - 1);
            AdjustDropDownControls();
  
          } else if (actionEvent.getSource() == cmdDFxSelect[i]) {
      	    //---------------------------------------
            // Select disk for drive
      	    //---------------------------------------
      	    char tmp[MAX_PATH];
  
      	    if(strlen(workprefs.floppyslots[i].df) > 0)
      	      strncpy(tmp, workprefs.floppyslots[i].df, MAX_PATH - 1);
      	    else
      	      strncpy(tmp, currentDir, MAX_PATH - 1);
  
      	    if(SelectFile("Select disk image file", tmp, diskfile_filter)) {
        	    if(strncmp(workprefs.floppyslots[i].df, tmp, MAX_PATH)) {
          	    strncpy(workprefs.floppyslots[i].df, tmp, sizeof(workprefs.floppyslots[i].df) - 1);
          	    disk_insert(i, tmp);
          	    AddFileToDiskList(tmp, 1);
                RefreshDiskListModel();
          	    extractPath(tmp, currentDir);
  
          	    if(i == 0 && chkLoadConfig->isSelected()) {
        	        // Search for config of disk
        	        extractFileName(workprefs.floppyslots[i].df, tmp);
        	        removeFileExtension(tmp);
        	        LoadConfigByName(tmp);
        	      }
                AdjustDropDownControls();
        	    }
    	      }
    	      cmdDFxSelect[i]->requestFocus();

          } else if (actionEvent.getSource() == cboDFxFile[i]) {
      	    //---------------------------------------
            // Diskimage from list selected
      	    //---------------------------------------
      	    if(!bIgnoreListChange) {
        	    int idx = cboDFxFile[i]->getSelected();
  
        	    if(idx < 0) {
                disk_eject(i);
                strncpy(workprefs.floppyslots[i].df, "", MAX_DPATH - 1);
                AdjustDropDownControls();
      	      } else {
          	    if(diskfileList.getElementAt(idx).compare(workprefs.floppyslots[i].df)) {
            	    strncpy(workprefs.floppyslots[i].df, diskfileList.getElementAt(idx).c_str(), sizeof(workprefs.floppyslots[i].df) - 1);
            	    disk_insert(i, workprefs.floppyslots[i].df);
            	    lstMRUDiskList.erase(lstMRUDiskList.begin() + idx);
            	    lstMRUDiskList.insert(lstMRUDiskList.begin(), workprefs.floppyslots[i].df);
                  RefreshDiskListModel();
                  bIgnoreListChange = true;
                  cboDFxFile[i]->setSelected(0);
                  bIgnoreListChange = false;
  
            	    if(i == 0 && chkLoadConfig->isSelected()) {
          	        // Search for config of disk
          	        char tmp[MAX_PATH];
          	        
          	        extractFileName(workprefs.floppyslots[i].df, tmp);
          	        removeFileExtension(tmp);
          	        LoadConfigByName(tmp);
                  }
                }
        	    }
        	  }
          }
        }
      }
      RefreshPanelFloppy();
    }
};
static DFxActionListener* dfxActionListener;


void InitPanelFloppy(const struct _ConfigCategory& category)
{
	int posX;
	int posY = DISTANCE_BORDER;
	int i;
	
	driveTypeList.clear();
  driveTypeList.add(_T("Disabled"));
  driveTypeList.add(_T("3.5'' DD"));
  driveTypeList.add(_T("3.5'' HD"));
  driveTypeList.add(_T("5.25'' SD"));
  driveTypeList.add(_T("3.5'' ESCOM"));
  
  RefreshDiskListModel();

	dfxActionListener = new DFxActionListener();
	
	for(i = 0; i < 4; ++i) {
	  char tmp[21];
	  snprintf(tmp, 20, "DF%d:", i); 
	  chkDFx[i] = new gcn::UaeCheckBox(tmp);
	  chkDFx[i]->addActionListener(dfxActionListener);
	  
	  cboDFxType[i] = new gcn::UaeDropDown(&driveTypeList);
    cboDFxType[i]->setSize(106, DROPDOWN_HEIGHT);
    cboDFxType[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cboType%d", i);
	  cboDFxType[i]->setId(tmp);
    cboDFxType[i]->addActionListener(dfxActionListener);
	  
	  chkDFxWriteProtect[i] = new gcn::UaeCheckBox("Write-protected");
	  chkDFxWriteProtect[i]->addActionListener(dfxActionListener);
	  snprintf(tmp, 20, "chkWP%d", i);
	  chkDFxWriteProtect[i]->setId(tmp);
	  
    cmdDFxInfo[i] = new gcn::Button("?");
    cmdDFxInfo[i]->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
    cmdDFxInfo[i]->setBaseColor(gui_baseCol);
    cmdDFxInfo[i]->addActionListener(dfxActionListener);

    cmdDFxEject[i] = new gcn::Button("Eject");
    cmdDFxEject[i]->setSize(SMALL_BUTTON_WIDTH * 2, SMALL_BUTTON_HEIGHT);
    cmdDFxEject[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cmdEject%d", i);
	  cmdDFxEject[i]->setId(tmp);
    cmdDFxEject[i]->addActionListener(dfxActionListener);

    cmdDFxSelect[i] = new gcn::Button("...");
    cmdDFxSelect[i]->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
    cmdDFxSelect[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cmdSel%d", i);
	  cmdDFxSelect[i]->setId(tmp);
    cmdDFxSelect[i]->addActionListener(dfxActionListener);

	  cboDFxFile[i] = new gcn::UaeDropDown(&diskfileList);
    cboDFxFile[i]->setSize(category.panel->getWidth() - 2 * DISTANCE_BORDER, DROPDOWN_HEIGHT);
    cboDFxFile[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "cboDisk%d", i);
	  cboDFxFile[i]->setId(tmp);
    cboDFxFile[i]->addActionListener(dfxActionListener);

    if(i == 0) {
      chkLoadConfig = new gcn::UaeCheckBox("Load config with same name as disk");
      chkLoadConfig->setId("LoadDiskCfg");
      chkLoadConfig->addActionListener(dfxActionListener);
    }
	}

	lblDriveSpeed = new gcn::Label("Floppy Drive Emulation Speed:");
  sldDriveSpeed = new gcn::Slider(0, 3);
  sldDriveSpeed->setSize(110, SLIDER_HEIGHT);
  sldDriveSpeed->setBaseColor(gui_baseCol);
	sldDriveSpeed->setMarkerLength(20);
	sldDriveSpeed->setStepLength(1);
	sldDriveSpeed->setId("DriveSpeed");
  sldDriveSpeed->addActionListener(dfxActionListener);
  lblDriveSpeedInfo = new gcn::Label(drivespeedlist[0]);

  cmdSaveForDisk = new gcn::Button("Save config for disk");
  cmdSaveForDisk->setSize(160, BUTTON_HEIGHT);
  cmdSaveForDisk->setBaseColor(gui_baseCol);
  cmdSaveForDisk->setId("SaveForDisk");
  cmdSaveForDisk->addActionListener(dfxActionListener);

  cmdCreateDDDisk = new gcn::Button("Create 3.5'' DD disk");
  cmdCreateDDDisk->setSize(160, BUTTON_HEIGHT);
  cmdCreateDDDisk->setBaseColor(gui_baseCol);
  cmdCreateDDDisk->setId("CreateDD");
  cmdCreateDDDisk->addActionListener(dfxActionListener);

  cmdCreateHDDisk = new gcn::Button("Create 3.5'' HD disk");
  cmdCreateHDDisk->setSize(160, BUTTON_HEIGHT);
  cmdCreateHDDisk->setBaseColor(gui_baseCol);
  cmdCreateHDDisk->setId("CreateHD");
  cmdCreateHDDisk->addActionListener(dfxActionListener);
	
	for(i = 0; i < 4; ++i) {
	  posX = DISTANCE_BORDER;
	  category.panel->add(chkDFx[i], posX, posY);
#ifdef ANDROID
	  posX += 85;
#else
	  posX += 100;
#endif
	  category.panel->add(cboDFxType[i], posX, posY);
	  posX += cboDFxType[i]->getWidth() + 2 * DISTANCE_NEXT_X;
	  category.panel->add(chkDFxWriteProtect[i], posX, posY);
	  posX += chkDFxWriteProtect[i]->getWidth() + 4 * DISTANCE_NEXT_X;
//	  category.panel->add(cmdDFxInfo[i], posX, posY);
	  posX += cmdDFxInfo[i]->getWidth() + DISTANCE_NEXT_X;
	  category.panel->add(cmdDFxEject[i], posX, posY);
	  posX += cmdDFxEject[i]->getWidth() + DISTANCE_NEXT_X;
	  category.panel->add(cmdDFxSelect[i], posX, posY);
	  posY += chkDFx[i]->getHeight() + 8;

	  category.panel->add(cboDFxFile[i], DISTANCE_BORDER, posY);
	  if(i == 0) {
  	  posY += cboDFxFile[i]->getHeight() + 8;
      category.panel->add(chkLoadConfig, DISTANCE_BORDER, posY);
    }
	  posY += cboDFxFile[i]->getHeight() + DISTANCE_NEXT_Y + 4;
  }
  
  posX = DISTANCE_BORDER;
  category.panel->add(lblDriveSpeed, posX, posY);
  posX += lblDriveSpeed->getWidth() + 8;
  category.panel->add(sldDriveSpeed, posX, posY);
  posX += sldDriveSpeed->getWidth() + DISTANCE_NEXT_X;
  category.panel->add(lblDriveSpeedInfo, posX, posY);
  posY += sldDriveSpeed->getHeight() + DISTANCE_NEXT_Y;

  posY = category.panel->getHeight() - DISTANCE_BORDER - BUTTON_HEIGHT;
  category.panel->add(cmdSaveForDisk, DISTANCE_BORDER, posY);
  category.panel->add(cmdCreateDDDisk, cmdSaveForDisk->getX() + cmdSaveForDisk->getWidth() + DISTANCE_NEXT_X, posY);
  category.panel->add(cmdCreateHDDisk, cmdCreateDDDisk->getX() + cmdCreateDDDisk->getWidth() + DISTANCE_NEXT_X, posY);
  
  RefreshPanelFloppy();
}


void ExitPanelFloppy(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
	for(int i = 0; i < 4; ++i) {
	  delete chkDFx[i];
	  delete cboDFxType[i];
	  delete chkDFxWriteProtect[i];
	  delete cmdDFxInfo[i];
	  delete cmdDFxEject[i];
	  delete cmdDFxSelect[i];
	  delete cboDFxFile[i];
	}
  delete chkLoadConfig;
  delete lblDriveSpeed;
  delete sldDriveSpeed;
  delete lblDriveSpeedInfo;
  delete cmdSaveForDisk;
  delete cmdCreateDDDisk;
  delete cmdCreateHDDisk;
  
  delete dfxActionListener;
}


bool HelpPanelFloppy(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("You can enable/disable each drive by clicking the checkbox next to DFx or select the drive type in the dropdown");
  helptext.push_back("control. \"3.5'' DD\" is the right choise for nearly all ADF and ADZ files.");
  helptext.push_back("The option \"Write-protected\" indicates if the emulator can write to the ADF. Changing the write protection of the");
  helptext.push_back("disk file may fail because of missing rights on the host filesystem.");
  helptext.push_back("The button \"...\" opens a dialog to select the required disk file. With the dropdown control, you can select one of");
  helptext.push_back("the disks you recently used.");
  helptext.push_back(" ");
  helptext.push_back("You can reduce the loading time for lot of games by increasing the floppy drive emulation speed. A few games");
  helptext.push_back("will not load with higher drive speed and you have to select 100%.");
  helptext.push_back(" ");
  helptext.push_back("\"Save config for disk\" will create a new configuration file with the name of the disk in DF0. This configuration will");
  helptext.push_back("be loaded each time you select the disk and have the option \"Load config with same name as disk\" enabled.");
  helptext.push_back(" ");
  helptext.push_back("With the buttons \"Create 3.5'' DD disk\" and \"Create 3.5'' HD disk\" you can create a new and empty disk.");
  return true;
}
