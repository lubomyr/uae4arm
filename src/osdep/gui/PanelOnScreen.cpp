#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#include "SelectorEntry.hpp"
#include "UaeRadioButton.hpp"
#include "UaeDropDown.hpp"
#include "UaeCheckBox.hpp"
#include "GenericListModel.h"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "gui_handling.h"
#include "onscreen_layout.h"

extern int onscreen_keeps_aspect();
extern void onscreen_screen_size(int *w, int *h);
extern void onscreen_default_positions(struct uae_prefs *p);

static gcn::UaeCheckBox* checkBox_onscreen_control;
static gcn::UaeCheckBox* checkBox_onscreen_textinput;
static gcn::UaeCheckBox* checkBox_onscreen_dpad;
static gcn::UaeCheckBox* checkBox_onscreen_button1;
static gcn::UaeCheckBox* checkBox_onscreen_button2;
static gcn::UaeCheckBox* checkBox_onscreen_button3;
static gcn::UaeCheckBox* checkBox_onscreen_button4;
static gcn::UaeCheckBox* checkBox_onscreen_button5;
static gcn::UaeCheckBox* checkBox_onscreen_button6;
static gcn::UaeCheckBox* checkBox_onscreen_custompos;
static gcn::UaeCheckBox* checkBox_floatingJoystick;
static gcn::UaeCheckBox* checkBox_disableMenuVKeyb;
static gcn::Label* label_onscreen_size;
static gcn::UaeDropDown* dropdown_onscreen_size;
static gcn::Label* label_onscreen_theme;
static gcn::UaeDropDown* dropdown_onscreen_theme;
static gcn::Label* label_onscreen_ctrlsize;
static gcn::UaeDropDown* dropdown_onscreen_ctrlsize;
static gcn::Label* label_onscreen_drawsize;
static gcn::UaeDropDown* dropdown_onscreen_drawsize;
static gcn::Label* label_onscreen_transparency;
static gcn::UaeDropDown* dropdown_onscreen_transparency;
static gcn::Label* label_onscreen_dpadmode;
static gcn::UaeDropDown* dropdown_onscreen_dpadmode;

// Розмір екранних кнопок у відсотках від типового
static const int onScreenSizeValues[] = { 50, 75, 100, 125, 150, 200 };
static const TCHAR* onScreenSizeLabels[] = { _T("50%"), _T("75%"), _T("100%"), _T("125%"), _T("150%"), _T("200%") };
static gcn::GenericListModel onScreenSizeList(onScreenSizeLabels, 6);

// Вигляд екранного керування. Ці списки мусять збігатися з тими, що в обгортці
// SDL (SettingsMenuKeyboard.java) - значення передаються туди як індекси.
static const TCHAR* onScreenThemeLabels[] = { _T("Ultimate Droid"), _T("Simple"), _T("Sun"), _T("Keen"),
                                              _T("Retro"), _T("Gba"), _T("Psx"), _T("Snes"),
                                              _T("DualShock"), _T("N64") };
static gcn::GenericListModel onScreenThemeList(onScreenThemeLabels, 10);
static const TCHAR* onScreenCtrlSizeLabels[] = { _T("Large"), _T("Medium"), _T("Small"), _T("Tiny"), _T("Custom") };
static gcn::GenericListModel onScreenCtrlSizeList(onScreenCtrlSizeLabels, 5);
static const TCHAR* onScreenDrawSizeLabels[] = { _T("Large"), _T("Medium"), _T("Small"), _T("Tiny") };
static gcn::GenericListModel onScreenDrawSizeList(onScreenDrawSizeLabels, 4);
static const TCHAR* onScreenTransparencyLabels[] = { _T("Invisible"), _T("Almost invisible"), _T("Transparent"),
                                                     _T("Semi-transparent"), _T("Opaque") };
static gcn::GenericListModel onScreenTransparencyList(onScreenTransparencyLabels, 5);
static const TCHAR* onScreenDpadModeLabels[] = { _T("Joystick"), _T("Cursor keys") };
static gcn::GenericListModel onScreenDpadModeList(onScreenDpadModeLabels, 2);
static gcn::Button* button_onscreen_pos;
static gcn::Button* button_onscreen_ok;
static gcn::Button* button_onscreen_reset;
static gcn::Window *window_setup_position;
static gcn::Window *window_pos_textinput;
static gcn::Window *window_pos_dpad;
static gcn::Window *window_pos_button1;
static gcn::Window *window_pos_button2;
static gcn::Window *window_pos_button3;
static gcn::Window *window_pos_button4;
static gcn::Window *window_pos_button5;
static gcn::Window *window_pos_button6;
static gcn::Label* label_setup_onscreen;
static gcn::TextField *textInput;

/* The editor's inner area stands for the whole screen, drawn so that on the
   device it has the screen's shape, with each control as big as it will be
   there - so what is placed is what appears. Positions are still stored in
   the ONSCREEN_SETUP_WIDTH x ONSCREEN_SETUP_HEIGHT space pandora_gfx.cpp
   scales from, so saved layouts stay valid on any device. */
static int setup_w = ONSCREEN_SETUP_WIDTH;   /* the editor's inner area */
static int setup_h = ONSCREEN_SETUP_HEIGHT;
static float screen_aspect = (float)ONSCREEN_SETUP_WIDTH / ONSCREEN_SETUP_HEIGHT;

/* Sizes as fractions of the screen height, the same as pandora_gfx.cpp. */
#define PROXY_TEXT   (1.0f / 10.0f)
#define PROXY_DPAD   (1.0f / 2.5f)
#define PROXY_BUTTON (1.0f / 5.0f)

/* The controls are square on the screen. The menu's pixels are not - it is
   stretched to the screen, or to 4:3 - so work out each side as its share of
   the screen instead of in menu pixels.
   The size shown is the drawn one: Draw size makes the wrapper draw a control
   smaller than the area it answers to, by 2 / (Draw size + 3) around the same
   centre (shrinkButtonRect() in SDL_touchscreenkeyboard.c), Large drawing it
   whole. */
static void proxy_geometry(float frac, int *base_w, int *base_h, int *size_w, int *size_h)
{
    float scale = workprefs.onScreen_size > 0 ? workprefs.onScreen_size / 100.0f : 1.0f;
    if (workprefs.onScreen_drawsize > 0)
        scale = scale * 2.0f / (workprefs.onScreen_drawsize + 3);
    *base_h = (int)(setup_h * frac);
    *base_w = (int)(setup_w * frac / screen_aspect);
    *size_h = (int)(*base_h * scale);
    *size_w = (int)(*base_w * scale);
}

/* A stored position is the corner of the control at 100%; other sizes grow
   around its centre, as in pandora_gfx.cpp. */
static void place_proxy(gcn::Window *w, int px, int py, float frac)
{
    int base_w, base_h, size_w, size_h;
    proxy_geometry(frac, &base_w, &base_h, &size_w, &size_h);
    w->setSize(size_w, size_h);
    w->setPosition((int)(px * setup_w / (float)ONSCREEN_SETUP_WIDTH) - (size_w - base_w) / 2,
                   (int)(py * setup_h / (float)ONSCREEN_SETUP_HEIGHT) - (size_h - base_h) / 2);
}

static void read_proxy(gcn::Window *w, int *px, int *py, float frac)
{
    int base_w, base_h, size_w, size_h;
    proxy_geometry(frac, &base_w, &base_h, &size_w, &size_h);
    *px = (int)((w->getX() + (size_w - base_w) / 2) * ONSCREEN_SETUP_WIDTH / (float)setup_w + 0.5f);
    *py = (int)((w->getY() + (size_h - base_h) / 2) * ONSCREEN_SETUP_HEIGHT / (float)setup_h + 0.5f);
}

static void RefreshPanelOnScreen(void)
{
    int sizeIdx = 2; // 100%
    for (int i = 0; i < 6; ++i) {
        if (onScreenSizeValues[i] == workprefs.onScreen_size)
            sizeIdx = i;
    }
    dropdown_onscreen_size->setSelected(sizeIdx);
    dropdown_onscreen_size->setEnabled(workprefs.custom_position != 0);

    dropdown_onscreen_theme->setSelected(workprefs.onScreen_theme);
    dropdown_onscreen_ctrlsize->setSelected(workprefs.onScreen_controlsize);
    dropdown_onscreen_drawsize->setSelected(workprefs.onScreen_drawsize);
    dropdown_onscreen_transparency->setSelected(workprefs.onScreen_transparency);
    dropdown_onscreen_dpadmode->setSelected(workprefs.onScreen_dpad_mode);

    if (workprefs.onScreen==0)
        checkBox_onscreen_control->setSelected(false);
    else if (workprefs.onScreen==1)
        checkBox_onscreen_control->setSelected(true);
    if (workprefs.onScreen_textinput==0)
        checkBox_onscreen_textinput->setSelected(false);
    else if (workprefs.onScreen_textinput==1)
        checkBox_onscreen_textinput->setSelected(true);
    if (workprefs.onScreen_dpad==0)
        checkBox_onscreen_dpad->setSelected(false);
    else if (workprefs.onScreen_dpad==1)
        checkBox_onscreen_dpad->setSelected(true);
    if (workprefs.onScreen_button1==0)
        checkBox_onscreen_button1->setSelected(false);
    else if (workprefs.onScreen_button1==1)
        checkBox_onscreen_button1->setSelected(true);
    if (workprefs.onScreen_button2==0)
        checkBox_onscreen_button2->setSelected(false);
    else if (workprefs.onScreen_button2==1)
        checkBox_onscreen_button2->setSelected(true);
    if (workprefs.onScreen_button3==0)
        checkBox_onscreen_button3->setSelected(false);
    else if (workprefs.onScreen_button3==1)
        checkBox_onscreen_button3->setSelected(true);
    if (workprefs.onScreen_button4==0)
        checkBox_onscreen_button4->setSelected(false);
    else if (workprefs.onScreen_button4==1)
        checkBox_onscreen_button4->setSelected(true);
    if (workprefs.onScreen_button5==0)
        checkBox_onscreen_button5->setSelected(false);
    else if (workprefs.onScreen_button5==1)
        checkBox_onscreen_button5->setSelected(true);
    if (workprefs.onScreen_button6==0)
        checkBox_onscreen_button6->setSelected(false);
    else if (workprefs.onScreen_button6==1)
        checkBox_onscreen_button6->setSelected(true);
    if (workprefs.custom_position==0)
        checkBox_onscreen_custompos->setSelected(false);
    else if (workprefs.custom_position==1)
        checkBox_onscreen_custompos->setSelected(true);
    if (workprefs.floatingJoystick)
        checkBox_floatingJoystick->setSelected(true);
    else
        checkBox_floatingJoystick->setSelected(false);
    if (workprefs.disableMenuVKeyb)
        checkBox_disableMenuVKeyb->setSelected(true);
    else
        checkBox_disableMenuVKeyb->setSelected(false);
    
    textInput->disableVirtualKeyboard(workprefs.disableMenuVKeyb);
    
    place_proxy(window_pos_textinput, workprefs.pos_x_textinput, workprefs.pos_y_textinput, PROXY_TEXT);
    window_pos_textinput->setVisible(workprefs.onScreen_textinput);
    place_proxy(window_pos_dpad, workprefs.pos_x_dpad, workprefs.pos_y_dpad, PROXY_DPAD);
    window_pos_dpad->setVisible(workprefs.onScreen_dpad);
    place_proxy(window_pos_button1, workprefs.pos_x_button1, workprefs.pos_y_button1, PROXY_BUTTON);
    window_pos_button1->setVisible(workprefs.onScreen_button1);
    place_proxy(window_pos_button2, workprefs.pos_x_button2, workprefs.pos_y_button2, PROXY_BUTTON);
    window_pos_button2->setVisible(workprefs.onScreen_button2);
    place_proxy(window_pos_button3, workprefs.pos_x_button3, workprefs.pos_y_button3, PROXY_BUTTON);
    window_pos_button3->setVisible(workprefs.onScreen_button3);
    place_proxy(window_pos_button4, workprefs.pos_x_button4, workprefs.pos_y_button4, PROXY_BUTTON);
    window_pos_button4->setVisible(workprefs.onScreen_button4);
    place_proxy(window_pos_button5, workprefs.pos_x_button5, workprefs.pos_y_button5, PROXY_BUTTON);
    window_pos_button5->setVisible(workprefs.onScreen_button5);
    place_proxy(window_pos_button6, workprefs.pos_x_button6, workprefs.pos_y_button6, PROXY_BUTTON);
    window_pos_button6->setVisible(workprefs.onScreen_button6);
    button_onscreen_pos->setVisible(workprefs.custom_position);
}

class OnScreenActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
        if (actionEvent.getSource() == checkBox_onscreen_control) {
            if (checkBox_onscreen_control->isSelected())
                workprefs.onScreen=1;
            else
                workprefs.onScreen=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_textinput) {
            if (checkBox_onscreen_textinput->isSelected())
                workprefs.onScreen_textinput=1;
            else
                workprefs.onScreen_textinput=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_dpad) {
            if (checkBox_onscreen_dpad->isSelected())
                workprefs.onScreen_dpad=1;
            else
                workprefs.onScreen_dpad=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_button1) {
            if (checkBox_onscreen_button1->isSelected())
                workprefs.onScreen_button1=1;
            else
                workprefs.onScreen_button1=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_button2) {
            if (checkBox_onscreen_button2->isSelected())
                workprefs.onScreen_button2=1;
            else
                workprefs.onScreen_button2=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_button3) {
            if (checkBox_onscreen_button3->isSelected())
                workprefs.onScreen_button3=1;
            else
                workprefs.onScreen_button3=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_button4) {
            if (checkBox_onscreen_button4->isSelected())
                workprefs.onScreen_button4=1;
            else
                workprefs.onScreen_button4=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_button5) {
            if (checkBox_onscreen_button5->isSelected())
                workprefs.onScreen_button5=1;
            else
                workprefs.onScreen_button5=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_button6) {
            if (checkBox_onscreen_button6->isSelected())
                workprefs.onScreen_button6=1;
            else
                workprefs.onScreen_button6=0;
        }
        if (actionEvent.getSource() == checkBox_onscreen_custompos) {
            if (checkBox_onscreen_custompos->isSelected())
                workprefs.custom_position=1;
            else
                workprefs.custom_position=0;
        }
        if (actionEvent.getSource() == checkBox_floatingJoystick)
            if (checkBox_floatingJoystick->isSelected())
                workprefs.floatingJoystick=1;
            else
                workprefs.floatingJoystick=0;
        if (actionEvent.getSource() == checkBox_disableMenuVKeyb)
            if (checkBox_disableMenuVKeyb->isSelected())
                workprefs.disableMenuVKeyb=1;
            else
                workprefs.disableMenuVKeyb=0;
        if (actionEvent.getSource() == dropdown_onscreen_size) {
            int idx = dropdown_onscreen_size->getSelected();
            if (idx >= 0 && idx < 6)
                workprefs.onScreen_size = onScreenSizeValues[idx];
        }
        if (actionEvent.getSource() == dropdown_onscreen_theme)
            workprefs.onScreen_theme = dropdown_onscreen_theme->getSelected();
        if (actionEvent.getSource() == dropdown_onscreen_ctrlsize)
            workprefs.onScreen_controlsize = dropdown_onscreen_ctrlsize->getSelected();
        if (actionEvent.getSource() == dropdown_onscreen_drawsize)
            workprefs.onScreen_drawsize = dropdown_onscreen_drawsize->getSelected();
        if (actionEvent.getSource() == dropdown_onscreen_transparency)
            workprefs.onScreen_transparency = dropdown_onscreen_transparency->getSelected();
        if (actionEvent.getSource() == dropdown_onscreen_dpadmode)
            workprefs.onScreen_dpad_mode = dropdown_onscreen_dpadmode->getSelected();
        RefreshPanelOnScreen();
    }
};
static OnScreenActionListener* onScreenActionListener;

class SetupPosButtonActionListener : public gcn::ActionListener
{
public:
    void action(const gcn::ActionEvent& actionEvent) {
        if (actionEvent.getSource() == button_onscreen_pos)
            window_setup_position->setVisible(true);
        RefreshPanelOnScreen();
    }
};
static SetupPosButtonActionListener* setupPosButtonActionListener;

class WindowPosButtonActionListener : public gcn::ActionListener
{
public:
    void action(const gcn::ActionEvent& actionEvent) {
        if (actionEvent.getSource() == button_onscreen_ok) {
            read_proxy(window_pos_textinput, &workprefs.pos_x_textinput, &workprefs.pos_y_textinput, PROXY_TEXT);
            read_proxy(window_pos_dpad, &workprefs.pos_x_dpad, &workprefs.pos_y_dpad, PROXY_DPAD);
            read_proxy(window_pos_button1, &workprefs.pos_x_button1, &workprefs.pos_y_button1, PROXY_BUTTON);
            read_proxy(window_pos_button2, &workprefs.pos_x_button2, &workprefs.pos_y_button2, PROXY_BUTTON);
            read_proxy(window_pos_button3, &workprefs.pos_x_button3, &workprefs.pos_y_button3, PROXY_BUTTON);
            read_proxy(window_pos_button4, &workprefs.pos_x_button4, &workprefs.pos_y_button4, PROXY_BUTTON);
            read_proxy(window_pos_button5, &workprefs.pos_x_button5, &workprefs.pos_y_button5, PROXY_BUTTON);
            read_proxy(window_pos_button6, &workprefs.pos_x_button6, &workprefs.pos_y_button6, PROXY_BUTTON);
            window_setup_position->setVisible(false);
        }
        if (actionEvent.getSource() == button_onscreen_reset) {
            onscreen_default_positions(&workprefs);
            window_setup_position->setVisible(false);
        }
    }
};
static WindowPosButtonActionListener* windowPosButtonActionListener;

void InitPanelOnScreen(const struct _ConfigCategory& category)
{
    onScreenActionListener = new OnScreenActionListener();
    checkBox_onscreen_control = new gcn::UaeCheckBox("On-screen control");
    checkBox_onscreen_control->setPosition(10,125);
    checkBox_onscreen_control->setId("OnScrCtrl");
    checkBox_onscreen_control->addActionListener(onScreenActionListener);
    checkBox_onscreen_textinput = new gcn::UaeCheckBox("TextInput button");
    checkBox_onscreen_textinput->setPosition(10,155);
    checkBox_onscreen_textinput->setId("OnScrTextInput");
    checkBox_onscreen_textinput->addActionListener(onScreenActionListener);
    checkBox_onscreen_dpad = new gcn::UaeCheckBox("D-pad");
    checkBox_onscreen_dpad->setPosition(10,185);
    checkBox_onscreen_dpad->setId("OnScrDpad");
    checkBox_onscreen_dpad->addActionListener(onScreenActionListener);
    checkBox_onscreen_button1 = new gcn::UaeCheckBox("Button 1 <A>");
    checkBox_onscreen_button1->setPosition(10,215);
    checkBox_onscreen_button1->setId("OnScrButton1");
    checkBox_onscreen_button1->addActionListener(onScreenActionListener);
    checkBox_onscreen_button2 = new gcn::UaeCheckBox("Button 2 <B>");
    checkBox_onscreen_button2->setPosition(10,245);
    checkBox_onscreen_button2->setId("OnScrButton2");
    checkBox_onscreen_button2->addActionListener(onScreenActionListener);
    checkBox_onscreen_button3 = new gcn::UaeCheckBox("Button 3 <X>");
    checkBox_onscreen_button3->setPosition(170,125);
    checkBox_onscreen_button3->setId("OnScrButton3");
    checkBox_onscreen_button3->addActionListener(onScreenActionListener);
    checkBox_onscreen_button4 = new gcn::UaeCheckBox("Button 4 <Y>");
    checkBox_onscreen_button4->setPosition(170,155);
    checkBox_onscreen_button4->setId("OnScrButton4");
    checkBox_onscreen_button4->addActionListener(onScreenActionListener);
    checkBox_onscreen_button5 = new gcn::UaeCheckBox("Button 5 <R>");
    checkBox_onscreen_button5->setPosition(170,185);
    checkBox_onscreen_button5->setId("OnScrButton5");
    checkBox_onscreen_button5->addActionListener(onScreenActionListener);
    checkBox_onscreen_button6 = new gcn::UaeCheckBox("Button 6 <L>");
    checkBox_onscreen_button6->setPosition(170,215);
    checkBox_onscreen_button6->setId("OnScrButton6");
    checkBox_onscreen_button6->addActionListener(onScreenActionListener);
    checkBox_onscreen_custompos = new gcn::UaeCheckBox("Custom position");
    checkBox_onscreen_custompos->setPosition(170,245);
    checkBox_onscreen_custompos->setId("CustomPos");
    checkBox_onscreen_custompos->addActionListener(onScreenActionListener);
    checkBox_floatingJoystick = new gcn::UaeCheckBox("Floating Joystick");
    checkBox_floatingJoystick->setPosition(10,285);
    checkBox_floatingJoystick->setId("FloatJoy");
    checkBox_floatingJoystick->addActionListener(onScreenActionListener);
    checkBox_disableMenuVKeyb = new gcn::UaeCheckBox("Disable virtual keyboard in menu");
    checkBox_disableMenuVKeyb->setPosition(10,315);
    checkBox_disableMenuVKeyb->setId("DisableMenuVKeyb");
    checkBox_disableMenuVKeyb->addActionListener(onScreenActionListener);

    label_onscreen_size = new gcn::Label("Custom control size");
    label_onscreen_size->setPosition(10, 93);

    dropdown_onscreen_size = new gcn::UaeDropDown(&onScreenSizeList);
    dropdown_onscreen_size->setSize(90, DROPDOWN_HEIGHT);
    dropdown_onscreen_size->setPosition(170, 90);
    dropdown_onscreen_size->setBaseColor(gui_baseCol);
    dropdown_onscreen_size->setId("OnScrSize");
    dropdown_onscreen_size->addActionListener(onScreenActionListener);

    // Вигляд екранного керування - два ряди по два списки. Усі випадні списки
    // тримаємо вгорі панелі: вони розкриваються вниз, і внизу їм бракує місця.
    label_onscreen_theme = new gcn::Label("Theme");
    label_onscreen_theme->setPosition(10, 23);
    dropdown_onscreen_theme = new gcn::UaeDropDown(&onScreenThemeList);
    dropdown_onscreen_theme->setSize(150, DROPDOWN_HEIGHT);
    dropdown_onscreen_theme->setPosition(110, 20);
    dropdown_onscreen_theme->setBaseColor(gui_baseCol);
    dropdown_onscreen_theme->setId("OnScrTheme");
    dropdown_onscreen_theme->addActionListener(onScreenActionListener);

    label_onscreen_ctrlsize = new gcn::Label("Size");
    label_onscreen_ctrlsize->setPosition(300, 23);
    dropdown_onscreen_ctrlsize = new gcn::UaeDropDown(&onScreenCtrlSizeList);
    dropdown_onscreen_ctrlsize->setSize(150, DROPDOWN_HEIGHT);
    dropdown_onscreen_ctrlsize->setPosition(400, 20);
    dropdown_onscreen_ctrlsize->setBaseColor(gui_baseCol);
    dropdown_onscreen_ctrlsize->setId("OnScrCtrlSize");
    dropdown_onscreen_ctrlsize->addActionListener(onScreenActionListener);

    label_onscreen_drawsize = new gcn::Label("Draw size");
    label_onscreen_drawsize->setPosition(10, 58);
    dropdown_onscreen_drawsize = new gcn::UaeDropDown(&onScreenDrawSizeList);
    dropdown_onscreen_drawsize->setSize(150, DROPDOWN_HEIGHT);
    dropdown_onscreen_drawsize->setPosition(110, 55);
    dropdown_onscreen_drawsize->setBaseColor(gui_baseCol);
    dropdown_onscreen_drawsize->setId("OnScrDrawSize");
    dropdown_onscreen_drawsize->addActionListener(onScreenActionListener);

    label_onscreen_dpadmode = new gcn::Label("D-Pad");
    label_onscreen_dpadmode->setPosition(300, 93);
    dropdown_onscreen_dpadmode = new gcn::UaeDropDown(&onScreenDpadModeList);
    dropdown_onscreen_dpadmode->setSize(150, DROPDOWN_HEIGHT);
    dropdown_onscreen_dpadmode->setPosition(400, 90);
    dropdown_onscreen_dpadmode->setBaseColor(gui_baseCol);
    dropdown_onscreen_dpadmode->setId("OnScrDpadMode");
    dropdown_onscreen_dpadmode->addActionListener(onScreenActionListener);

    label_onscreen_transparency = new gcn::Label("Transparency");
    label_onscreen_transparency->setPosition(300, 58);
    dropdown_onscreen_transparency = new gcn::UaeDropDown(&onScreenTransparencyList);
    dropdown_onscreen_transparency->setSize(150, DROPDOWN_HEIGHT);
    dropdown_onscreen_transparency->setPosition(400, 55);
    dropdown_onscreen_transparency->setBaseColor(gui_baseCol);
    dropdown_onscreen_transparency->setId("OnScrTransparency");
    dropdown_onscreen_transparency->addActionListener(onScreenActionListener);

    button_onscreen_pos = new gcn::Button("Position Setup");
    button_onscreen_pos->setPosition(170,285);
    button_onscreen_pos->setBaseColor(gui_baseCol);
    setupPosButtonActionListener = new SetupPosButtonActionListener();
    button_onscreen_pos->addActionListener(setupPosButtonActionListener);

    button_onscreen_ok = new gcn::Button(" Ok ");
    button_onscreen_ok->setPosition(220,175);
    button_onscreen_ok->setBaseColor(gui_baseCol);
    button_onscreen_reset = new gcn::Button(" Reset Position to default ");
    button_onscreen_reset->setPosition(150,105);
    button_onscreen_reset->setBaseColor(gui_baseCol);
    windowPosButtonActionListener = new WindowPosButtonActionListener();
    button_onscreen_ok->addActionListener(windowPosButtonActionListener);
    button_onscreen_reset->addActionListener(windowPosButtonActionListener);
    label_setup_onscreen = new gcn::Label("Try drag and drop window then press ok");
    label_setup_onscreen->setPosition(100,140);
	
	textInput = new gcn::TextField();
//    label_setup_onscreen->setFont(font14);

    window_pos_textinput = new gcn::Window("Ab");
    window_pos_textinput->setMovable(true);
    window_pos_textinput->setSize(25,30);
    window_pos_textinput->setBaseColor(gui_baseCol);
    window_pos_dpad = new gcn::Window("Dpad");
    window_pos_dpad->setMovable(true);
    window_pos_dpad->setSize(100,130);
    window_pos_dpad->setBaseColor(gui_baseCol);
    window_pos_button1 = new gcn::Window("1<A>");
    window_pos_button1->setMovable(true);
    window_pos_button1->setSize(50,65);
    window_pos_button1->setBaseColor(gui_baseCol);
    window_pos_button2 = new gcn::Window("2<B>");
    window_pos_button2->setMovable(true);
    window_pos_button2->setSize(50,65);
    window_pos_button2->setBaseColor(gui_baseCol);
    window_pos_button3 = new gcn::Window("3<X>");
    window_pos_button3->setMovable(true);
    window_pos_button3->setSize(50,65);
    window_pos_button3->setBaseColor(gui_baseCol);
    window_pos_button4 = new gcn::Window("4<Y>");
    window_pos_button4->setMovable(true);
    window_pos_button4->setSize(50,65);
    window_pos_button4->setBaseColor(gui_baseCol);
    window_pos_button5 = new gcn::Window("5<R>");
    window_pos_button5->setMovable(true);
    window_pos_button5->setSize(50,65);
    window_pos_button5->setBaseColor(gui_baseCol);
    window_pos_button6 = new gcn::Window("6<L>");
    window_pos_button6->setMovable(true);
    window_pos_button6->setSize(50,65);
    window_pos_button6->setBaseColor(gui_baseCol);

    window_setup_position = new gcn::Window("Setup position");
    window_setup_position->add(label_setup_onscreen);
    window_setup_position->add(button_onscreen_ok);
    window_setup_position->add(button_onscreen_reset);
    window_setup_position->add(window_pos_textinput);
    window_setup_position->add(window_pos_dpad);
    window_setup_position->add(window_pos_button1);
    window_setup_position->add(window_pos_button2);
    window_setup_position->add(window_pos_button3);
    window_setup_position->add(window_pos_button4);
    window_setup_position->add(window_pos_button5);
    window_setup_position->add(window_pos_button6);
    window_setup_position->setMovable(false);

    /* Fit a rectangle into the panel that shows on the device in the shape of
       its screen - the one the controls are placed on, from pandora_gfx.cpp.
       The GUI_WIDTH x GUI_HEIGHT menu is stretched over the whole screen, so
       its pixels are not square. With the 4:3 screen ratio the wrapper keeps
       the menu's own shape on a screen wider than it, and squeezes it to 4:3
       on a narrower one. */
    {
        int screen_w, screen_h;
        onscreen_screen_size(&screen_w, &screen_h);
        if (screen_w > 0 && screen_h > 0)
            screen_aspect = screen_w / (float)screen_h;
        float gui_aspect = (float)GUI_WIDTH / GUI_HEIGHT;
        float shown_aspect = screen_aspect;
        if (onscreen_keeps_aspect())
            shown_aspect = gui_aspect < screen_aspect ? gui_aspect : 4.0f / 3.0f;
        float aspect = screen_aspect * gui_aspect / shown_aspect;

        int pad = window_setup_position->getPadding();
        int title = window_setup_position->getTitleBarHeight();
        int maxw = category.panel->getWidth() - 2 * DISTANCE_BORDER - 2 * pad;
        int maxh = category.panel->getHeight() - 2 * DISTANCE_BORDER - title - pad;
        if (maxw > maxh * aspect) {
            setup_h = maxh;
            setup_w = (int)(maxh * aspect);
        } else {
            setup_w = maxw;
            setup_h = (int)(maxw / aspect);
        }
        int winw = setup_w + 2 * pad;
        int winh = setup_h + title + pad;
        window_setup_position->setSize(winw, winh);
        window_setup_position->setPosition((category.panel->getWidth() - winw) / 2,
                                           (category.panel->getHeight() - winh) / 2);

        /* The hint and the buttons where they were, relative to the height. */
        button_onscreen_reset->setPosition((setup_w - button_onscreen_reset->getWidth()) / 2, setup_h * 105 / 370);
        label_setup_onscreen->setPosition((setup_w - label_setup_onscreen->getWidth()) / 2, setup_h * 140 / 370);
        button_onscreen_ok->setPosition((setup_w - button_onscreen_ok->getWidth()) / 2, setup_h * 175 / 370);
    }
    window_setup_position->setVisible(false);
    
    category.panel->add(checkBox_onscreen_control);
    category.panel->add(checkBox_onscreen_textinput);
    category.panel->add(checkBox_onscreen_dpad);
    category.panel->add(checkBox_onscreen_button1);
    category.panel->add(checkBox_onscreen_button2);
    category.panel->add(checkBox_onscreen_button3);
    category.panel->add(checkBox_onscreen_button4);
    category.panel->add(checkBox_onscreen_button5);
    category.panel->add(checkBox_onscreen_button6);
    category.panel->add(checkBox_onscreen_custompos);
    category.panel->add(checkBox_floatingJoystick);
    category.panel->add(checkBox_disableMenuVKeyb);
    category.panel->add(label_onscreen_size);
    category.panel->add(dropdown_onscreen_size);
    category.panel->add(label_onscreen_theme);
    category.panel->add(dropdown_onscreen_theme);
    category.panel->add(label_onscreen_ctrlsize);
    category.panel->add(dropdown_onscreen_ctrlsize);
    category.panel->add(label_onscreen_drawsize);
    category.panel->add(dropdown_onscreen_drawsize);
    category.panel->add(label_onscreen_transparency);
    category.panel->add(dropdown_onscreen_transparency);
    category.panel->add(label_onscreen_dpadmode);
    category.panel->add(dropdown_onscreen_dpadmode);
    category.panel->add(button_onscreen_pos);
    category.panel->add(window_setup_position);
    
  RefreshPanelOnScreen();
}


void ExitPanelOnScreen(const struct _ConfigCategory& category)
{
	category.panel->clear();

    delete label_onscreen_size;
    delete dropdown_onscreen_size;
    delete label_onscreen_theme;
    delete dropdown_onscreen_theme;
    delete label_onscreen_ctrlsize;
    delete dropdown_onscreen_ctrlsize;
    delete label_onscreen_drawsize;
    delete dropdown_onscreen_drawsize;
    delete label_onscreen_transparency;
    delete dropdown_onscreen_transparency;
    delete label_onscreen_dpadmode;
    delete dropdown_onscreen_dpadmode;
    delete checkBox_onscreen_control;
    delete checkBox_onscreen_textinput;
    delete checkBox_onscreen_dpad;
    delete checkBox_onscreen_button1;
    delete checkBox_onscreen_button2;
    delete checkBox_onscreen_button3;
    delete checkBox_onscreen_button4;
    delete checkBox_onscreen_button5;
    delete checkBox_onscreen_button6;
    delete checkBox_onscreen_custompos;
    delete checkBox_floatingJoystick;
    delete checkBox_disableMenuVKeyb;
    delete button_onscreen_pos;
    delete button_onscreen_ok;
    delete button_onscreen_reset;
    delete window_setup_position;
    delete window_pos_textinput;
    delete window_pos_dpad;
    delete window_pos_button1;
    delete window_pos_button2;
    delete window_pos_button3;
    delete window_pos_button4;
    delete window_pos_button5;
    delete window_pos_button6;
    delete label_setup_onscreen;
	delete textInput;
  delete setupPosButtonActionListener;
  delete windowPosButtonActionListener;
  delete onScreenActionListener;
}

bool HelpPanelOnScreen(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("onScreen Help");
  return true;
}
