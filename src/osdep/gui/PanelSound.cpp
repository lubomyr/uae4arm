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
#include "sounddep/sound.h"
#include "GenericListModel.h"


static gcn::Window *grpSound;
static gcn::UaeRadioButton* optSoundDisabled;
static gcn::UaeRadioButton* optSoundDisabledEmu;
static gcn::UaeRadioButton* optSoundEmulated;
static gcn::UaeRadioButton* optSoundEmulatedBest;
static gcn::Window *grpMode;
static gcn::UaeRadioButton* optMono;
static gcn::UaeRadioButton* optStereo;
static gcn::Label* lblFrequency;
static gcn::UaeDropDown* cboFrequency;
static gcn::Label* lblInterpolation;
static gcn::UaeDropDown* cboInterpolation;
static gcn::Label* lblFilter;
static gcn::UaeDropDown* cboFilter;
static gcn::Label* lblSeparation;
static gcn::Label* lblSeparationInfo;
static gcn::Slider* sldSeparation;
static gcn::Label* lblStereoDelay;
static gcn::Label* lblStereoDelayInfo;
static gcn::Slider* sldStereoDelay;
static gcn::Label* lblPaulaVol;
static gcn::Label* lblPaulaVolInfo;
static gcn::Slider* sldPaulaVol;

static int curr_separation_idx;
static int curr_stereodelay_idx;


static const TCHAR* freqList[] = { _T("11025"), _T("22050"), _T("32000"), _T("44100") };
static gcn::GenericListModel frequencyTypeList(freqList, 4);


static const TCHAR* interpolList[] = { _T("Disabled"), _T("Anti"), _T("Sinc"), _T("RH"), _T("Crux") };
static gcn::GenericListModel interpolationTypeList(interpolList, 5);


static const TCHAR* filterList[] = { _T("Always off"), _T("Emulated (A500)"), _T("Emulated (A1200)"), _T("Always on (A500)"), _T("Always on (A1200)") };
static gcn::GenericListModel filterTypeList(filterList, 5);


static void RefreshPanelSound(void)
{
  char tmp[10];
  int i;

  switch(workprefs.produce_sound)
  {
    case 0:
      optSoundDisabled->setSelected(true);
      break;
    case 1:
      optSoundDisabledEmu->setSelected(true);
      break;
    case 2:
      optSoundEmulated->setSelected(true);
      break;
    case 3:
      optSoundEmulatedBest->setSelected(true);
      break;
  }

	if (workprefs.sound_stereo == 0)
    optMono->setSelected(true);
  else if (workprefs.sound_stereo == 1)
    optStereo->setSelected(true);

  switch(workprefs.sound_freq)
  {
    case 11025:
      cboFrequency->setSelected(0);
      break;
    case 22050:
      cboFrequency->setSelected(1);
      break;
    case 32000:
      cboFrequency->setSelected(2);
      break;
    default:
      cboFrequency->setSelected(3);
      break;
  }

  cboInterpolation->setSelected(workprefs.sound_interpol);

  i = 0;
  switch (workprefs.sound_filter)
  {
    case 0:
      i = 0;
      break;
    case 1:
      i = workprefs.sound_filter_type ? 2 : 1;
      break;
    case 2:
      i = workprefs.sound_filter_type ? 4 : 3;
      break;
  }
  cboFilter->setSelected(i);  
  
  if(workprefs.sound_stereo == 0) {
    curr_separation_idx = 0;
    curr_stereodelay_idx = 0;
  } else {
    curr_separation_idx = 10 - workprefs.sound_stereo_separation;
    curr_stereodelay_idx = workprefs.sound_mixed_stereo_delay > 0 ? workprefs.sound_mixed_stereo_delay : 0;
  }

  sldSeparation->setValue(curr_separation_idx);
  sldSeparation->setEnabled(workprefs.sound_stereo >= 1);
  snprintf(tmp, sizeof (tmp) - 1, "%d%%", 100 - 10 * curr_separation_idx);
  lblSeparationInfo->setCaption(tmp);

  sldStereoDelay->setValue(curr_stereodelay_idx);
  sldStereoDelay->setEnabled(workprefs.sound_stereo >= 1);
  if(curr_stereodelay_idx <= 0)
    lblStereoDelayInfo->setCaption("-");
  else {
    snprintf(tmp, sizeof (tmp) - 1, "%d", curr_stereodelay_idx);
    lblStereoDelayInfo->setCaption(tmp);
  }

  sldPaulaVol->setValue(100 - workprefs.sound_volume_paula);
  snprintf(tmp, sizeof (tmp) - 1, "%d %%", 100 - workprefs.sound_volume_paula);
  lblPaulaVolInfo->setCaption(tmp);
}


class SoundActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    if (actionEvent.getSource() == optSoundDisabled)
    		workprefs.produce_sound = 0;

	    else if (actionEvent.getSource() == optSoundDisabledEmu)
    		workprefs.produce_sound = 1;

	    else if (actionEvent.getSource() == optSoundEmulated)
    		workprefs.produce_sound = 2;

	    else if (actionEvent.getSource() == optSoundEmulatedBest)
    		workprefs.produce_sound = 3;

	    else if (actionEvent.getSource() == optMono)
    		workprefs.sound_stereo = 0;

	    else if (actionEvent.getSource() == optStereo)
    		workprefs.sound_stereo = 1;

	    else if (actionEvent.getSource() == cboFrequency) {
        switch(cboFrequency->getSelected())
        {
          case 0:
    		    workprefs.sound_freq = 11025;
            break;
          case 1:
    		    workprefs.sound_freq = 22050;
            break;
          case 2:
    		    workprefs.sound_freq = 32000;
            break;
          case 3:
    		    workprefs.sound_freq = 44100;
            break;
        }
      }

	    else if (actionEvent.getSource() == cboInterpolation)
        workprefs.sound_interpol = cboInterpolation->getSelected();

	    else if (actionEvent.getSource() == cboFilter) {
        switch (cboFilter->getSelected())
        {
        	case 0:
          	workprefs.sound_filter = FILTER_SOUND_OFF;
          	break;
        	case 1:
          	workprefs.sound_filter = FILTER_SOUND_EMUL;
          	workprefs.sound_filter_type = 0;
          	break;
        	case 2:
          	workprefs.sound_filter = FILTER_SOUND_EMUL;
          	workprefs.sound_filter_type = 1;
          	break;
        	case 3:
          	workprefs.sound_filter = FILTER_SOUND_ON;
          	workprefs.sound_filter_type = 0;
          	break;
        	case 4:
          	workprefs.sound_filter = FILTER_SOUND_ON;
          	workprefs.sound_filter_type = 1;
          	break;
        }

      } else if (actionEvent.getSource() == sldSeparation) {
        if(curr_separation_idx != (int)(sldSeparation->getValue())
        && workprefs.sound_stereo > 0)
        {
      		curr_separation_idx = (int)(sldSeparation->getValue());
      		workprefs.sound_stereo_separation = 10 - curr_separation_idx;
    	  }

      } else if (actionEvent.getSource() == sldStereoDelay) {
        if(curr_stereodelay_idx != (int)(sldStereoDelay->getValue())
        && workprefs.sound_stereo > 0)
        {
      		curr_stereodelay_idx = (int)(sldStereoDelay->getValue());
      		if(curr_stereodelay_idx > 0)
      		  workprefs.sound_mixed_stereo_delay = curr_stereodelay_idx;
      		else
      		  workprefs.sound_mixed_stereo_delay = -1;
    	  }

      } else if (actionEvent.getSource() == sldPaulaVol) {
        int newvol = 100 - (int) sldPaulaVol->getValue();
        if(workprefs.sound_volume_paula != newvol)
          workprefs.sound_volume_paula = newvol;
			}

      RefreshPanelSound();
    }
};
static SoundActionListener* soundActionListener;


void InitPanelSound(const struct _ConfigCategory& category)
{
  soundActionListener = new SoundActionListener();

	optSoundDisabled = new gcn::UaeRadioButton("Disabled", "radiosoundgroup");
	optSoundDisabled->setId("sndDisable");
	optSoundDisabled->addActionListener(soundActionListener);

	optSoundDisabledEmu = new gcn::UaeRadioButton("Disabled, but emulated", "radiosoundgroup");
	optSoundDisabledEmu->setId("sndDisEmu");
	optSoundDisabledEmu->addActionListener(soundActionListener);
  
	optSoundEmulated = new gcn::UaeRadioButton("Enabled", "radiosoundgroup");
	optSoundEmulated->setId("sndEmulate");
	optSoundEmulated->addActionListener(soundActionListener);

	optSoundEmulatedBest = new gcn::UaeRadioButton("Enabled, most accurate", "radiosoundgroup");
	optSoundEmulatedBest->setId("sndEmuBest");
	optSoundEmulatedBest->addActionListener(soundActionListener);

	grpSound = new gcn::Window("Sound Emulation");
	grpSound->add(optSoundDisabled, 5, 10);
	grpSound->add(optSoundDisabledEmu, 5, 40);
	grpSound->add(optSoundEmulated, 5, 70);
	grpSound->add(optSoundEmulatedBest, 5, 100);
	grpSound->setMovable(false);
#ifdef ANDROID
	grpSound->setSize(210, 150);
#else
	grpSound->setSize(200, 150);
#endif
  grpSound->setBaseColor(gui_baseCol);

    int labelWidth;
#ifdef ANDROID
	labelWidth = 135;
#else
	labelWidth = 130;
#endif

	lblFrequency = new gcn::Label("Frequency:");
	lblFrequency->setSize(labelWidth, LABEL_HEIGHT);
  lblFrequency->setAlignment(gcn::Graphics::RIGHT);
  cboFrequency = new gcn::UaeDropDown(&frequencyTypeList);
  cboFrequency->setSize(160, DROPDOWN_HEIGHT);
  cboFrequency->setBaseColor(gui_baseCol);
  cboFrequency->setId("cboFrequency");
  cboFrequency->addActionListener(soundActionListener);

	optMono = new gcn::UaeRadioButton("Mono", "radiosoundmodegroup");
	optMono->addActionListener(soundActionListener);

	optStereo = new gcn::UaeRadioButton("Stereo", "radiosoundmodegroup");
	optStereo->addActionListener(soundActionListener);

	grpMode = new gcn::Window("Mode");
	grpMode->add(optMono, 5, 10);
	grpMode->add(optStereo, 5, 40);
	grpMode->setMovable(false);
#ifdef ANDROID
	grpMode->setSize(95, 90);
#else
	grpMode->setSize(90, 90);
#endif
  grpMode->setBaseColor(gui_baseCol);

	lblInterpolation = new gcn::Label("Interpolation:");
	lblInterpolation->setSize(labelWidth, LABEL_HEIGHT);
  lblInterpolation->setAlignment(gcn::Graphics::RIGHT);
  cboInterpolation = new gcn::UaeDropDown(&interpolationTypeList);
  cboInterpolation->setSize(160, DROPDOWN_HEIGHT);
  cboInterpolation->setBaseColor(gui_baseCol);
  cboInterpolation->setId("cboInterpol");
  cboInterpolation->addActionListener(soundActionListener);

	lblFilter = new gcn::Label("Filter:");
	lblFilter->setSize(labelWidth, LABEL_HEIGHT);
  lblFilter->setAlignment(gcn::Graphics::RIGHT);
  cboFilter = new gcn::UaeDropDown(&filterTypeList);
  cboFilter->setSize(160, DROPDOWN_HEIGHT);
  cboFilter->setBaseColor(gui_baseCol);
  cboFilter->setId("cboFilter");
  cboFilter->addActionListener(soundActionListener);

	lblSeparation = new gcn::Label("Stereo separation:");
	lblSeparation->setSize(labelWidth, LABEL_HEIGHT);
  lblSeparation->setAlignment(gcn::Graphics::RIGHT);
  sldSeparation = new gcn::Slider(0, 10);
  sldSeparation->setSize(160, SLIDER_HEIGHT);
  sldSeparation->setBaseColor(gui_baseCol);
	sldSeparation->setMarkerLength(20);
	sldSeparation->setStepLength(1);
	sldSeparation->setId("sldSeparation");
  sldSeparation->addActionListener(soundActionListener);
  lblSeparationInfo = new gcn::Label("100%");

	lblStereoDelay = new gcn::Label("Stereo delay:");
	lblStereoDelay->setSize(labelWidth, LABEL_HEIGHT);
  lblStereoDelay->setAlignment(gcn::Graphics::RIGHT);
  sldStereoDelay = new gcn::Slider(0, 10);
  sldStereoDelay->setSize(160, SLIDER_HEIGHT);
  sldStereoDelay->setBaseColor(gui_baseCol);
	sldStereoDelay->setMarkerLength(20);
	sldStereoDelay->setStepLength(1);
	sldStereoDelay->setId("sldStereoDelay");
  sldStereoDelay->addActionListener(soundActionListener);
  lblStereoDelayInfo = new gcn::Label("10");
  
	lblPaulaVol = new gcn::Label("Paula Volume:");
  lblPaulaVol->setSize(labelWidth, LABEL_HEIGHT);
  lblPaulaVol->setAlignment(gcn::Graphics::RIGHT);
  sldPaulaVol = new gcn::Slider(0, 100);
  sldPaulaVol->setSize(160, SLIDER_HEIGHT);
  sldPaulaVol->setBaseColor(gui_baseCol);
	sldPaulaVol->setMarkerLength(20);
	sldPaulaVol->setStepLength(10);
	sldPaulaVol->setId("PaulaVol");
  sldPaulaVol->addActionListener(soundActionListener);
  lblPaulaVolInfo = new gcn::Label("80 %");
  lblPaulaVolInfo->setSize(100, LABEL_HEIGHT);

  int posY = DISTANCE_BORDER;
  category.panel->add(grpSound, DISTANCE_BORDER, posY);
  category.panel->add(grpMode, grpSound->getX() + grpSound->getWidth() + DISTANCE_NEXT_X, posY);
  posY += grpSound->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblFrequency, DISTANCE_BORDER, posY);
  category.panel->add(cboFrequency, lblFrequency->getX() + lblFrequency->getWidth() + 12, posY);
  posY += cboFrequency->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblInterpolation, DISTANCE_BORDER, posY);
  category.panel->add(cboInterpolation, lblInterpolation->getX() + lblInterpolation->getWidth() + 12, posY);
  posY += cboInterpolation->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblFilter, DISTANCE_BORDER, posY);
  category.panel->add(cboFilter, lblFilter->getX() + lblFilter->getWidth() + 12, posY);
  posY += cboFilter->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblSeparation, DISTANCE_BORDER, posY);
  category.panel->add(sldSeparation, lblSeparation->getX() + lblSeparation->getWidth() + 12, posY);
  category.panel->add(lblSeparationInfo, sldSeparation->getX() + sldSeparation->getWidth() + 12, posY);
  posY += SLIDER_HEIGHT + DISTANCE_NEXT_Y;
  category.panel->add(lblStereoDelay, DISTANCE_BORDER, posY);
  category.panel->add(sldStereoDelay, lblStereoDelay->getX() + lblStereoDelay->getWidth() + 12, posY);
  category.panel->add(lblStereoDelayInfo, sldStereoDelay->getX() + sldStereoDelay->getWidth() + 12, posY);
  posY += SLIDER_HEIGHT + DISTANCE_NEXT_Y;
  category.panel->add(lblPaulaVol, DISTANCE_BORDER, posY);
  category.panel->add(sldPaulaVol, lblPaulaVol->getX() + lblPaulaVol->getWidth() + 12, posY);
  category.panel->add(lblPaulaVolInfo, sldPaulaVol->getX() + sldPaulaVol->getWidth() + 12, posY);
  posY += SLIDER_HEIGHT + DISTANCE_NEXT_Y;
  
  RefreshPanelSound();
}


void ExitPanelSound(const struct _ConfigCategory& category)
{
  category.panel->clear();
  
  delete optSoundDisabled;
  delete optSoundDisabledEmu;
  delete optSoundEmulated;
  delete optSoundEmulatedBest;
  delete grpSound;
  delete optMono;
  delete optStereo;
  delete grpMode;
  delete lblFrequency;
  delete cboFrequency;
  delete lblInterpolation;
  delete cboInterpolation;
  delete lblFilter;
  delete cboFilter;
  delete lblSeparation;
  delete sldSeparation;
  delete lblSeparationInfo;
  delete lblStereoDelay;
  delete sldStereoDelay;
  delete lblStereoDelayInfo;
  delete lblPaulaVol;
  delete lblPaulaVolInfo;
  delete sldPaulaVol;
  delete soundActionListener;
}


bool HelpPanelSound(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("You can turn on sound emulation with different levels of accuracy and choose between mono and stereo.");
  helptext.push_back(" ");
  helptext.push_back("The different types of interpolation have different impact on the performance. Play with the settings to find the");
  helptext.push_back("type you like most. You may need headphones to really hear the differences between the interpolations.");
  helptext.push_back(" ");
  helptext.push_back("With \"Filter\", you can select the type of the Amiga audio filter.");
  helptext.push_back(" ");
  helptext.push_back("With \"Stereo separation\" and \"Stereo delay\", you can adjust how the left and right audio channels of the Amiga");
  helptext.push_back("are mixed to the left and right channels of your device. A value of 70% for separation and no delay is a good");
  helptext.push_back("start.");
  return true;
}
