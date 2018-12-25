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
#include "include/memory-uae.h"
#include "uae.h"
#include "autoconf.h"
#include "gfxboard.h"
#include "gui.h"
#include "gui_handling.h"


static const char *ChipMem_list[] = { "512 K", "1 MB", "2 MB", "4 MB", "8 MB" };
static const int ChipMem_values[] = { 0x080000, 0x100000, 0x200000, 0x400000, 0x800000 };
static const char *SlowMem_list[] = { "None", "512 K", "1 MB", "1.5 MB", "1.8 MB" };
static const int SlowMem_values[] = { 0x000000, 0x080000, 0x100000, 0x180000, 0x1c0000 };
static const char *FastMem_list[] = { "None", "1 MB", "2 MB", "4 MB", "8 MB", "16 MB", "32 MB", "64 MB", "128 MB" };
static const int FastMem_values[] = { 0x000000, 0x100000, 0x200000, 0x400000, 0x800000, 0x1000000, 0x2000000, 0x4000000, 0x8000000 };
static const char *A3000LowMem_list[] = { "None", "8 MB", "16 MB", "32 MB", "64 MB" };
static const int A3000LowMem_values[] = { 0x080000, 0x800000, 0x1000000, 0x2000000, 0x4000000 };
static const char *A3000HighMem_list[] = { "None", "8 MB", "16 MB", "32 MB", "64 MB", "128 MB" };
static const int A3000HighMem_values[] = { 0x080000, 0x800000, 0x1000000, 0x2000000, 0x4000000, 0x8000000 };

static gcn::Window *grpRAM;
static gcn::Label* lblChipmem;
static gcn::Label* lblChipsize;
static gcn::Slider* sldChipmem;
static gcn::Label* lblSlowmem;
static gcn::Label* lblSlowsize;
static gcn::Slider* sldSlowmem;
static gcn::Label* lblFastmem;
static gcn::Label* lblFastsize;
static gcn::Slider* sldFastmem;  
static gcn::Label* lblZ3mem;
static gcn::Label* lblZ3size;
static gcn::Slider* sldZ3mem;  
static gcn::Label* lblGfxmem;
static gcn::Label* lblGfxsize;
static gcn::Slider* sldGfxmem;  
static gcn::Label* lblA3000Lowmem;
static gcn::Label* lblA3000Lowsize;
static gcn::Slider* sldA3000Lowmem;  
static gcn::Label* lblA3000Highmem;
static gcn::Label* lblA3000Highsize;
static gcn::Slider* sldA3000Highmem;  


static void RefreshPanelRAM(void)
{
  int i;
  
  for(i = 0; i < 5; ++i) {
    if(workprefs.chipmem_size == ChipMem_values[i]) {
      sldChipmem->setValue(i);
      lblChipsize->setCaption(ChipMem_list[i]);
      break;
    }
  }

  for(i = 0; i < 5; ++i) {
    if(workprefs.bogomem_size == SlowMem_values[i]) {
      sldSlowmem->setValue(i);
      lblSlowsize->setCaption(SlowMem_list[i]);
      break;
    }
  }

  for(i = 0; i < 5; ++i) {
    if(workprefs.fastmem[0].size == FastMem_values[i]) {
      sldFastmem->setValue(i);
      lblFastsize->setCaption(FastMem_list[i]);
      break;
    }
  }

  for(i = 0; i < 9; ++i) {
    if(workprefs.z3fastmem[0].size == FastMem_values[i]) {
      sldZ3mem->setValue(i);
      lblZ3size->setCaption(FastMem_list[i]);
      break;
    }
  }
  sldZ3mem->setEnabled(!workprefs.address_space_24);

  for(i = 0; i < 6; ++i) {
    if(workprefs.rtgboards[0].rtgmem_size == FastMem_values[i]) {
      sldGfxmem->setValue(i);
      lblGfxsize->setCaption(FastMem_list[i]);
      break;
    }
  }
  sldGfxmem->setEnabled(!workprefs.address_space_24);

  for(i = 0; i < 5; ++i) {
    if(workprefs.mbresmem_low_size == A3000LowMem_values[i]) {
      sldA3000Lowmem->setValue(i);
      lblA3000Lowsize->setCaption(A3000LowMem_list[i]);
      break;
    }
  }

  for(i = 0; i < 6; ++i) {
    if(workprefs.mbresmem_high_size == A3000HighMem_values[i]) {
      sldA3000Highmem->setValue(i);
      lblA3000Highsize->setCaption(A3000HighMem_list[i]);
      break;
    }
  }

  expansion_generate_autoconfig_info(&workprefs);
}


class MemorySliderActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
 	    if (actionEvent.getSource() == sldChipmem) {
    		workprefs.chipmem_size = ChipMem_values[(int)(sldChipmem->getValue())];
      	if ((workprefs.chipmem_size > 0x200000) && (workprefs.fastmem[0].size > 0))
      		workprefs.fastmem[0].size = 0;
			}
			
 	    if (actionEvent.getSource() == sldSlowmem) {
      	workprefs.bogomem_size = SlowMem_values[(int)(sldSlowmem->getValue())];
      }
      
	    if (actionEvent.getSource() == sldFastmem) {
     		workprefs.fastmem[0].size = FastMem_values[(int)(sldFastmem->getValue())];
	      if (workprefs.fastmem[0].size > 0 && workprefs.chipmem_size > 0x200000)
	        workprefs.chipmem_size = 0x200000;
  		}	

	    if (actionEvent.getSource() == sldZ3mem) {
     		workprefs.z3fastmem[0].size = FastMem_values[(int)(sldZ3mem->getValue())];
	      if (workprefs.z3fastmem[0].size > max_z3fastmem)
	        workprefs.z3fastmem[0].size = max_z3fastmem;
  		}	

	    if (actionEvent.getSource() == sldGfxmem) {
     		workprefs.rtgboards[0].rtgmem_size = FastMem_values[(int)(sldGfxmem->getValue())];
     		workprefs.rtgboards[0].rtgmem_type = GFXBOARD_UAE_Z3;
  		}	

 	    if (actionEvent.getSource() == sldA3000Lowmem) {
      	workprefs.mbresmem_low_size = A3000LowMem_values[(int)(sldA3000Lowmem->getValue())];
      	if(currprefs.mbresmem_low_size != workprefs.mbresmem_low_size)
      	  DisableResume();
      }

 	    if (actionEvent.getSource() == sldA3000Highmem) {
      	workprefs.mbresmem_high_size = A3000HighMem_values[(int)(sldA3000Highmem->getValue())];
      	if(currprefs.mbresmem_high_size != workprefs.mbresmem_high_size)
      	  DisableResume();
      }

  		RefreshPanelRAM();
    }
};
static MemorySliderActionListener* memorySliderActionListener;


void InitPanelRAM(const struct _ConfigCategory& category)
{
  memorySliderActionListener = new MemorySliderActionListener();
  int sldWidth;
  int markerLength;
#ifdef ANDROID
  sldWidth = 150;
  markerLength = 30;
#else
  sldWidth = 110;
  markerLength = 20;
#endif

	lblChipmem = new gcn::Label("Chip:");
  sldChipmem = new gcn::Slider(0, 4);
  sldChipmem->setSize(sldWidth, SLIDER_HEIGHT);
  sldChipmem->setBaseColor(gui_baseCol);
	sldChipmem->setMarkerLength(markerLength);
	sldChipmem->setStepLength(1);
	sldChipmem->setId("Chipmem");
  sldChipmem->addActionListener(memorySliderActionListener);
  lblChipsize = new gcn::Label("None   ");

	lblSlowmem = new gcn::Label("Slow:");
  sldSlowmem = new gcn::Slider(0, 4);
  sldSlowmem->setSize(sldWidth, SLIDER_HEIGHT);
  sldSlowmem->setBaseColor(gui_baseCol);
	sldSlowmem->setMarkerLength(markerLength);
	sldSlowmem->setStepLength(1);
	sldSlowmem->setId("Slowmem");
  sldSlowmem->addActionListener(memorySliderActionListener);
  lblSlowsize = new gcn::Label("None   ");

	lblFastmem = new gcn::Label("Z2 Fast:");
  sldFastmem = new gcn::Slider(0, 4);
  sldFastmem->setSize(sldWidth, SLIDER_HEIGHT);
  sldFastmem->setBaseColor(gui_baseCol);
	sldFastmem->setMarkerLength(markerLength);
	sldFastmem->setStepLength(1);
	sldFastmem->setId("Fastmem");
  sldFastmem->addActionListener(memorySliderActionListener);
  lblFastsize = new gcn::Label("None   ");

	lblZ3mem = new gcn::Label("Z3 fast:");
  sldZ3mem = new gcn::Slider(0, 8);
  sldZ3mem->setSize(sldWidth, SLIDER_HEIGHT);
  sldZ3mem->setBaseColor(gui_baseCol);
	sldZ3mem->setMarkerLength(markerLength);
	sldZ3mem->setStepLength(1);
	sldZ3mem->setId("Z3mem");
  sldZ3mem->addActionListener(memorySliderActionListener);
  lblZ3size = new gcn::Label("None    ");
  
	lblGfxmem = new gcn::Label("RTG board:");
  sldGfxmem = new gcn::Slider(0, 5);
  sldGfxmem->setSize(sldWidth, SLIDER_HEIGHT);
  sldGfxmem->setBaseColor(gui_baseCol);
	sldGfxmem->setMarkerLength(markerLength);
	sldGfxmem->setStepLength(1);
	sldGfxmem->setId("Gfxmem");
  sldGfxmem->addActionListener(memorySliderActionListener);
  lblGfxsize = new gcn::Label("None   ");

	lblA3000Lowmem = new gcn::Label("A4000 Motherb. slot:");
  sldA3000Lowmem = new gcn::Slider(0, 4);
  sldA3000Lowmem->setSize(sldWidth, SLIDER_HEIGHT);
  sldA3000Lowmem->setBaseColor(gui_baseCol);
	sldA3000Lowmem->setMarkerLength(markerLength);
	sldA3000Lowmem->setStepLength(1);
	sldA3000Lowmem->setId("A3000Low");
  sldA3000Lowmem->addActionListener(memorySliderActionListener);
  lblA3000Lowsize = new gcn::Label("None   ");

	lblA3000Highmem = new gcn::Label("A4000 Proc. board:");
  sldA3000Highmem = new gcn::Slider(0, 5);
  sldA3000Highmem->setSize(sldWidth, SLIDER_HEIGHT);
  sldA3000Highmem->setBaseColor(gui_baseCol);
	sldA3000Highmem->setMarkerLength(markerLength);
	sldA3000Highmem->setStepLength(1);
	sldA3000Highmem->setId("A3000High");
  sldA3000Highmem->addActionListener(memorySliderActionListener);
  lblA3000Highsize = new gcn::Label("None   ");

	grpRAM = new gcn::Window("Memory Settings");
	grpRAM->setPosition(DISTANCE_BORDER, DISTANCE_BORDER);

	int posY = 10;
	grpRAM->add(lblChipmem, 8, posY);
	grpRAM->add(sldChipmem, 160, posY);
	grpRAM->add(lblChipsize, 160 + sldChipmem->getWidth() + 12, posY);
	posY += sldChipmem->getHeight() + DISTANCE_NEXT_Y;

	grpRAM->add(lblSlowmem, 8, posY);
	grpRAM->add(sldSlowmem, 160, posY);
	grpRAM->add(lblSlowsize, 160 + sldSlowmem->getWidth() + 12, posY);
	posY += sldSlowmem->getHeight() + DISTANCE_NEXT_Y;
	 
	grpRAM->add(lblFastmem, 8, posY);
	grpRAM->add(sldFastmem, 160, posY);
	grpRAM->add(lblFastsize, 160 + sldFastmem->getWidth() + 12, posY);
	posY += sldFastmem->getHeight() + DISTANCE_NEXT_Y;

	grpRAM->add(lblZ3mem, 8, posY);
	grpRAM->add(sldZ3mem, 160, posY);
	grpRAM->add(lblZ3size, 160 + sldZ3mem->getWidth() + 12, posY);
	posY += sldZ3mem->getHeight() + DISTANCE_NEXT_Y;

#ifdef PICASSO96
	grpRAM->add(lblGfxmem, 8, posY);
	grpRAM->add(sldGfxmem, 160, posY);
	grpRAM->add(lblGfxsize, 160 + sldGfxmem->getWidth() + 12, posY);
	posY += sldGfxmem->getHeight() + DISTANCE_NEXT_Y;
#endif

	grpRAM->add(lblA3000Lowmem, 8, posY);
	grpRAM->add(sldA3000Lowmem, 160, posY);
	grpRAM->add(lblA3000Lowsize, 160 + sldA3000Lowmem->getWidth() + 12, posY);
	posY += sldA3000Lowmem->getHeight() + DISTANCE_NEXT_Y;

	grpRAM->add(lblA3000Highmem, 8, posY);
	grpRAM->add(sldA3000Highmem, 160, posY);
	grpRAM->add(lblA3000Highsize, 160 + sldA3000Highmem->getWidth() + 12, posY);
	posY += sldA3000Highmem->getHeight() + DISTANCE_NEXT_Y;

	grpRAM->setMovable(false);
#ifdef ANDROID
	grpRAM->setSize(400, posY + DISTANCE_BORDER);
#else
	grpRAM->setSize(350, posY + DISTANCE_BORDER);
#endif
  grpRAM->setBaseColor(gui_baseCol);
  
  category.panel->add(grpRAM);
  
  RefreshPanelRAM();
}


void ExitPanelRAM(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete lblChipmem;
  delete sldChipmem;
  delete lblChipsize;
  delete lblSlowmem;
  delete sldSlowmem;
  delete lblSlowsize;
  delete lblFastmem;
  delete sldFastmem;
  delete lblFastsize;
  delete lblZ3mem;
  delete sldZ3mem;
  delete lblZ3size;
  delete lblGfxmem;
  delete sldGfxmem;
  delete lblGfxsize;
  delete lblA3000Lowmem;
  delete sldA3000Lowmem;
  delete lblA3000Lowsize;
  delete lblA3000Highmem;
  delete sldA3000Highmem;
  delete lblA3000Highsize;
  delete grpRAM;
  delete memorySliderActionListener;
}


bool HelpPanelRAM(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("Select the amount of RAM for each type you want to emulate in your Amiga.");
  helptext.push_back("\"Slow\" is the simple memory extension of an Amiga 500.");
  helptext.push_back("\"Z2 Fast\" is real fast memory in 24 bit address space.");
  helptext.push_back("\"Z3 Fast\" is real fast memory in 32 bit address space and only available if a 32 bit CPU is selected.");
  helptext.push_back("\"RTG board\" is the graphics memory used by Picasso96 and only available if a 32 bit CPU is selected. If you");
  helptext.push_back("select some memory for this type, the Z3 RTG board will be activated.");
  helptext.push_back("A4000 motherboard and processor board memory is only detected by the Amiga if you choose the correct");
  helptext.push_back("Kickstart ROM (A4000).");
  return true;
}
