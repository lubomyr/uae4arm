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

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "memory-uae.h"
#include "newcpu.h"
#include "uae.h"
#include "gui.h"
#include "gui_handling.h"


static const char *CacheSize_list[] = { "-", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB" };
static const int CacheSize_values[] = { 0, 1024, 2048, 4096, 8192, 16384 };

static gcn::Window *grpCPU;
static gcn::UaeRadioButton* optCPU68000;
static gcn::UaeRadioButton* optCPU68010;
static gcn::UaeRadioButton* optCPU68020;
static gcn::UaeRadioButton* optCPU68030;
static gcn::UaeRadioButton* optCPU68040;
static gcn::UaeCheckBox* chk24Bit;
static gcn::UaeCheckBox* chkCPUCompatible;
static gcn::UaeCheckBox* chkJIT;
static gcn::Window *grpFPU;
static gcn::UaeRadioButton* optFPUnone;
static gcn::UaeRadioButton* optFPU68881;
static gcn::UaeRadioButton* optFPU68882;
static gcn::UaeRadioButton* optFPUinternal;
static gcn::UaeCheckBox* chkFPUstrict;
static gcn::UaeCheckBox* chkFPUJIT;
static gcn::Window *grpCPUSpeed;
static gcn::UaeRadioButton* opt7Mhz;
static gcn::UaeRadioButton* opt14Mhz;
static gcn::UaeRadioButton* opt28Mhz;
static gcn::UaeRadioButton* optFastest;
static gcn::Label* lblCachemem;
static gcn::Label* lblCachesize;
static gcn::Slider* sldCachemem;


static void RefreshPanelCPU(void)
{
  if(workprefs.cpu_model == 68000)
    optCPU68000->setSelected(true);
  else if(workprefs.cpu_model == 68010)
    optCPU68010->setSelected(true);
  else if(workprefs.cpu_model == 68020)
    optCPU68020->setSelected(true);
  else if(workprefs.cpu_model == 68030)
    optCPU68030->setSelected(true);
  else if(workprefs.cpu_model == 68040)
    optCPU68040->setSelected(true);

  chk24Bit->setSelected(workprefs.address_space_24);
  chk24Bit->setEnabled(workprefs.cpu_model == 68020);
  chkCPUCompatible->setSelected(workprefs.cpu_compatible > 0);
  chkCPUCompatible->setEnabled(workprefs.cpu_model <= 68010);
  chkJIT->setSelected(workprefs.cachesize > 0);

  switch(workprefs.fpu_model)
  {
    case 68881:
      optFPU68881->setSelected(true);
      break;
    case 68882:
      optFPU68882->setSelected(true);
      break;
    case 68040:
      optFPUinternal->setSelected(true);
      break;
    default:
      optFPUnone->setSelected(true);
      break;
  }
  optFPU68881->setEnabled(workprefs.cpu_model >= 68020 && workprefs.cpu_model < 68040);
  optFPU68882->setEnabled(workprefs.cpu_model >= 68020 && workprefs.cpu_model < 68040);
  optFPUinternal->setEnabled(workprefs.cpu_model == 68040);
  
  chkFPUstrict->setSelected(workprefs.fpu_strict);
#ifdef USE_JIT_FPU
  chkFPUJIT->setSelected(workprefs.compfpu);
  chkFPUJIT->setEnabled(workprefs.cachesize > 0);
#else
  chkFPUJIT->setSelected(false);
  chkFPUJIT->setEnabled(false);
#endif
   
	if (workprefs.m68k_speed == M68K_SPEED_7MHZ_CYCLES)
    opt7Mhz->setSelected(true);
  else if (workprefs.m68k_speed == M68K_SPEED_14MHZ_CYCLES)
    opt14Mhz->setSelected(true);
  else if (workprefs.m68k_speed == M68K_SPEED_25MHZ_CYCLES)
    opt28Mhz->setSelected(true);
  else if (workprefs.m68k_speed == -1)
    optFastest->setSelected(true);
    
  for(int i = 0; i < 6; ++i) {
    if(workprefs.cachesize == CacheSize_values[i]) {
      sldCachemem->setValue(i);
      lblCachesize->setCaption(CacheSize_list[i]);
      break;
    }
  }
  sldCachemem->setEnabled(workprefs.cachesize > 0);
}


class CPUActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    if (actionEvent.getSource() == optCPU68000) {
  		  workprefs.cpu_model = 68000;
  		  workprefs.fpu_model = 0;
  		  workprefs.address_space_24 = true;
  		  workprefs.z3fastmem[0].size = 0;
  		  workprefs.rtgboards[0].rtgmem_size = 0;

      } else if (actionEvent.getSource() == optCPU68010) {
  		  workprefs.cpu_model = 68010;
  		  workprefs.fpu_model = 0;
  		  workprefs.address_space_24 = true;
  		  workprefs.z3fastmem[0].size = 0;
  		  workprefs.rtgboards[0].rtgmem_size = 0;

      } else if (actionEvent.getSource() == optCPU68020) {
  		  workprefs.cpu_model = 68020;
  		  if(workprefs.fpu_model == 68040)
  		    workprefs.fpu_model = 68881;
  		  workprefs.cpu_compatible = 0;

      } else if (actionEvent.getSource() == optCPU68030) {
  		  workprefs.cpu_model = 68030;
  		  if(workprefs.fpu_model == 68040)
  		    workprefs.fpu_model = 68881;
  		  workprefs.address_space_24 = false;
  		  workprefs.cpu_compatible = 0;

      } else if (actionEvent.getSource() == optCPU68040) {
  		  workprefs.cpu_model = 68040;
  		  workprefs.fpu_model = 68040;
  		  workprefs.address_space_24 = false;
  		  workprefs.cpu_compatible = 0;

      } else if (actionEvent.getSource() == optFPUnone) {
  		  workprefs.fpu_model = 0;

  		} else if(actionEvent.getSource() == optFPU68881) {
  		  workprefs.fpu_model = 68881;

  		} else if(actionEvent.getSource() == optFPU68882)	{
  		  workprefs.fpu_model = 68882;

  		} else if(actionEvent.getSource() == optFPUinternal) {
  		  workprefs.fpu_model = 68040;

  		} else if (actionEvent.getSource() == opt7Mhz) {
      	workprefs.m68k_speed = M68K_SPEED_7MHZ_CYCLES;

      } else if (actionEvent.getSource() == opt14Mhz) {
	      workprefs.m68k_speed = M68K_SPEED_14MHZ_CYCLES;

      } else if (actionEvent.getSource() == opt28Mhz) {
	      workprefs.m68k_speed = M68K_SPEED_25MHZ_CYCLES;

      } else if (actionEvent.getSource() == optFastest) {
	      workprefs.m68k_speed = -1;

      } else if (actionEvent.getSource() == chk24Bit) {
  		  workprefs.address_space_24 = chk24Bit->isSelected();

      } else if (actionEvent.getSource() == chkCPUCompatible) {
  	    if (chkCPUCompatible->isSelected()) {
  	      workprefs.cpu_compatible = 1;
  	      workprefs.cachesize = 0;
        } else {
  	      workprefs.cpu_compatible = 0;
        }

      } else if (actionEvent.getSource() == chkJIT) {
		    if (chkJIT->isSelected()) {
		      workprefs.cpu_compatible = 0;
		      workprefs.cachesize = MAX_JIT_CACHE;
		      workprefs.compfpu = true;
	      } else {
		      workprefs.cachesize = 0;
		      workprefs.compfpu = false;
	      }
	    } else if (actionEvent.getSource() == chkFPUJIT) {
	      workprefs.compfpu = chkFPUJIT->isSelected();

	    } else if (actionEvent.getSource() == chkFPUstrict) {
        workprefs.fpu_strict = chkFPUstrict->isSelected();

	    } else if (actionEvent.getSource() == sldCachemem) {
    		workprefs.cachesize = CacheSize_values[(int)(sldCachemem->getValue())];

      }
      
	    RefreshPanelCPU();
    }
};
static CPUActionListener* cpuActionListener;


void InitPanelCPU(const struct _ConfigCategory& category)
{
  cpuActionListener = new CPUActionListener();
  
	optCPU68000 = new gcn::UaeRadioButton("68000", "radiocpugroup");
	optCPU68000->addActionListener(cpuActionListener);
	optCPU68010 = new gcn::UaeRadioButton("68010", "radiocpugroup");
	optCPU68010->addActionListener(cpuActionListener);
	optCPU68020 = new gcn::UaeRadioButton("68020", "radiocpugroup");
	optCPU68020->addActionListener(cpuActionListener);
	optCPU68030 = new gcn::UaeRadioButton("68030", "radiocpugroup");
	optCPU68030->addActionListener(cpuActionListener);
	optCPU68040 = new gcn::UaeRadioButton("68040", "radiocpugroup");
	optCPU68040->addActionListener(cpuActionListener);
	
	chk24Bit = new gcn::UaeCheckBox("24-bit addressing", true);
	chk24Bit->setId("CPU24Bit");
  chk24Bit->addActionListener(cpuActionListener);

	chkCPUCompatible = new gcn::UaeCheckBox("More compatible", true);
	chkCPUCompatible->setId("CPUComp");
  chkCPUCompatible->addActionListener(cpuActionListener);

	chkJIT = new gcn::UaeCheckBox("JIT", true);
	chkJIT->setId("JIT");
  chkJIT->addActionListener(cpuActionListener);

	lblCachemem = new gcn::Label("Cache:");
  sldCachemem = new gcn::Slider(0, 5);
  sldCachemem->setSize(100, SLIDER_HEIGHT);
  sldCachemem->setBaseColor(gui_baseCol);
	sldCachemem->setMarkerLength(20);
	sldCachemem->setStepLength(1);
	sldCachemem->setId("Cachemem");
  sldCachemem->addActionListener(cpuActionListener);
  lblCachesize = new gcn::Label("None  ");
	
	grpCPU = new gcn::Window("CPU");
	grpCPU->setPosition(DISTANCE_BORDER, DISTANCE_BORDER);
	grpCPU->add(optCPU68000, 5, 10);
	grpCPU->add(optCPU68010, 5, 40);
	grpCPU->add(optCPU68020, 5, 70);
	grpCPU->add(optCPU68030, 5, 100);
	grpCPU->add(optCPU68040, 5, 130);
	grpCPU->add(chk24Bit, 5, 170);
	grpCPU->add(chkCPUCompatible, 5, 200);
	grpCPU->add(chkJIT, 5, 230);
	grpCPU->add(lblCachemem, 5, 260);
	grpCPU->add(sldCachemem, 6, 290);
	grpCPU->add(lblCachesize, 110, 290);
	grpCPU->setMovable(false);
#ifdef ANDROID
	grpCPU->setSize(165, 335);
#else
	grpCPU->setSize(160, 335);
#endif
  grpCPU->setBaseColor(gui_baseCol);
  
  category.panel->add(grpCPU);

	optFPUnone = new gcn::UaeRadioButton("None", "radiofpugroup");
	optFPUnone->setId("FPUnone");
	optFPUnone->addActionListener(cpuActionListener);

	optFPU68881 = new gcn::UaeRadioButton("68881", "radiofpugroup");
	optFPU68881->addActionListener(cpuActionListener);
  
	optFPU68882 = new gcn::UaeRadioButton("68882", "radiofpugroup");
	optFPU68882->addActionListener(cpuActionListener);

	optFPUinternal = new gcn::UaeRadioButton("CPU internal", "radiofpugroup");
	optFPUinternal->addActionListener(cpuActionListener);

	chkFPUstrict = new gcn::UaeCheckBox("More compatible", true);
	chkFPUstrict->setId("FPUstrict");
  chkFPUstrict->addActionListener(cpuActionListener);

	chkFPUJIT = new gcn::UaeCheckBox("FPU JIT", true);
	chkFPUJIT->setId("FPUJIT");
  chkFPUJIT->addActionListener(cpuActionListener);

	grpFPU = new gcn::Window("FPU");
	grpFPU->setPosition(DISTANCE_BORDER + grpCPU->getWidth() + DISTANCE_NEXT_X, DISTANCE_BORDER);
	grpFPU->add(optFPUnone,  5, 10);
	grpFPU->add(optFPU68881, 5, 40);
	grpFPU->add(optFPU68882, 5, 70);
	grpFPU->add(optFPUinternal, 5, 100);
	grpFPU->add(chkFPUstrict, 5, 140);
	grpFPU->add(chkFPUJIT, 5, 170);
	grpFPU->setMovable(false);
	grpFPU->setSize(180, 215);
  grpFPU->setBaseColor(gui_baseCol);
  
  category.panel->add(grpFPU);

	opt7Mhz = new gcn::UaeRadioButton("7 Mhz", "radiocpuspeedgroup");
	opt7Mhz->addActionListener(cpuActionListener);

	opt14Mhz = new gcn::UaeRadioButton("14 Mhz", "radiocpuspeedgroup");
	opt14Mhz->addActionListener(cpuActionListener);

	opt28Mhz = new gcn::UaeRadioButton("25 Mhz", "radiocpuspeedgroup");
	opt28Mhz->addActionListener(cpuActionListener);

	optFastest = new gcn::UaeRadioButton("Fastest", "radiocpuspeedgroup");
	optFastest->addActionListener(cpuActionListener);

	grpCPUSpeed = new gcn::Window("CPU Speed");
	grpCPUSpeed->setPosition(grpFPU->getX() + grpFPU->getWidth() + DISTANCE_NEXT_X, DISTANCE_BORDER);
	grpCPUSpeed->add(opt7Mhz,  5, 10);
	grpCPUSpeed->add(opt14Mhz, 5, 40);
	grpCPUSpeed->add(opt28Mhz, 5, 70);
	grpCPUSpeed->add(optFastest, 5, 100);
	grpCPUSpeed->setMovable(false);
#ifdef ANDROID
	grpCPUSpeed->setSize(100, 145);
#else
	grpCPUSpeed->setSize(95, 145);
#endif
  grpCPUSpeed->setBaseColor(gui_baseCol);

  category.panel->add(grpCPUSpeed);

  RefreshPanelCPU();
}


void ExitPanelCPU(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete optCPU68000;
  delete optCPU68010;
  delete optCPU68020;
  delete optCPU68030;
  delete optCPU68040;
  delete chk24Bit;
  delete chkCPUCompatible;
  delete chkJIT;
  delete lblCachemem;
  delete sldCachemem;
  delete lblCachesize;
  delete grpCPU;
  
  delete optFPUnone;
  delete optFPU68881;
  delete optFPU68882;
  delete optFPUinternal;
  delete chkFPUstrict;
  delete chkFPUJIT;
  delete grpFPU;
  
  delete opt7Mhz;
  delete opt14Mhz;
  delete opt28Mhz;
  delete optFastest;
  delete grpCPUSpeed;

  delete cpuActionListener;
}


bool HelpPanelCPU(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("Select the required Amiga CPU (68000 - 68040).");
  helptext.push_back("If you select 68020, you can choose between 24-bit addressing (68EC020) or 32-bit addressing (68020).");
  helptext.push_back("The option \"More compatible\" is only available if 68000 or 68010 is selected and emulates simple prefetch of");
  helptext.push_back("the 68000. This may improve compatibility in few situations but is not required for most games and demos.");
  helptext.push_back(" ");
  helptext.push_back("JIT enables the Just-in-time compiler. This may break compatibility in some games.");
  helptext.push_back(" ");
  helptext.push_back("The available FPU models depending on the selected CPU.");
  helptext.push_back("The option \"More compatible\" activates more accurate rounding and compare of two floats.");
  helptext.push_back(" ");
  helptext.push_back("With \"CPU Speed\" you can choose the clock rate of the Amiga.");
  helptext.push_back(" ");
  helptext.push_back("In current version, you will not see a difference in the performance for 68020, 68030 and 68040 CPUs. The cpu");
  helptext.push_back("cycles for the opcodes are based on 68020. The different cycles for 68030 and 68040 may come in a later");
  helptext.push_back("version.");
  return true;
}
  
