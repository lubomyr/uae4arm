#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#include "SelectorEntry.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "uae.h"
#include "gui.h"
#include "target.h"
#include "gui_handling.h"


extern SDL_Surface *prSDLScreen;


static int msg_done = 0;
class DoneActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      msg_done = 1;
    }
};
static DoneActionListener* doneActionListener;


void InGameMessage(const char *msg)
{
  gcn::Gui* msg_gui;
  gcn::SDLGraphics* msg_graphics;
  gcn::SDLInput* msg_input;
  gcn::contrib::SDLTrueTypeFont* msg_font;
  gcn::Color msg_baseCol;

  gcn::Container* msg_top;
  gcn::Window *wndMsg;
  gcn::Button* cmdDone;
  gcn::TextBox* txtMsg;
    
  int msgWidth = 260;
  int msgHeight = 100;
  
  msg_graphics = new gcn::SDLGraphics();
  msg_graphics->setTarget(prSDLScreen);
  msg_input = new gcn::SDLInput();
  msg_gui = new gcn::Gui();
  msg_gui->setGraphics(msg_graphics);
  msg_gui->setInput(msg_input);
  
  msg_baseCol.r = 192;
  msg_baseCol.g = 192;
  msg_baseCol.b = 208;

  msg_top = new gcn::Container();
  msg_top->setDimension(gcn::Rectangle((prSDLScreen->w - msgWidth) / 2, (prSDLScreen->h - msgHeight) / 2, msgWidth, msgHeight));
  msg_top->setBaseColor(msg_baseCol);
  msg_gui->setTop(msg_top);

  TTF_Init();
  msg_font = new gcn::contrib::SDLTrueTypeFont("data/FreeSans.ttf", 10);
  gcn::Widget::setGlobalFont(msg_font);

  doneActionListener = new DoneActionListener();
  
	wndMsg = new gcn::Window("Load");
	wndMsg->setSize(msgWidth, msgHeight);
  wndMsg->setPosition(0, 0);
  wndMsg->setBaseColor(msg_baseCol + 0x202020);
  wndMsg->setCaption("Information");
  wndMsg->setTitleBarHeight(12);

	cmdDone = new gcn::Button("Ok");
	cmdDone->setSize(40, 20);
  cmdDone->setBaseColor(msg_baseCol + 0x202020);
  cmdDone->addActionListener(doneActionListener);
  
  txtMsg = new gcn::TextBox(msg);
  txtMsg->setEnabled(false);
  txtMsg->setPosition(6, 6);
  txtMsg->setSize(wndMsg->getWidth() - 16, 46);
  txtMsg->setOpaque(false);
  
  wndMsg->add(txtMsg, 6, 6);
  wndMsg->add(cmdDone, (wndMsg->getWidth() - cmdDone->getWidth()) / 2, wndMsg->getHeight() - 38);

  msg_top->add(wndMsg);
  cmdDone->requestFocus();
  
  msg_done = 0;
  while(!msg_done)
  {
    //-------------------------------------------------
    // Check user input
    //-------------------------------------------------
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
      if (event.type == SDL_KEYDOWN)
      {
        switch(event.key.keysym.sym)
        {
          case SDLK_PAGEDOWN:
          case SDLK_HOME:
          case SDLK_RETURN:
            msg_done = 1;
            break;
        }
      }

      //-------------------------------------------------
      // Send event to guichan-controls
      //-------------------------------------------------
#ifdef ANDROIDSDL
            /*
             * Now that we are done polling and using SDL events we pass
             * the leftovers to the SDLInput object to later be handled by
             * the Gui. (This example doesn't require us to do this 'cause a
             * label doesn't use input. But will do it anyway to show how to
             * set up an SDL application with Guichan.)
             */
            if (event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP) {
                // Filter emulated mouse events for Guichan, we wand absolute input
            } else {
                // Convert multitouch event to SDL mouse event
                static int x = 0, y = 0, buttons = 0, wx=0, wy=0, pr=0;
                SDL_Event event2;
                memcpy(&event2, &event, sizeof(event));
                if (event.type == SDL_JOYBALLMOTION &&
                    event.jball.which == 0 &&
                    event.jball.ball == 0) {
                    event2.type = SDL_MOUSEMOTION;
                    event2.motion.which = 0;
                    event2.motion.state = buttons;
                    event2.motion.xrel = event.jball.xrel - x;
                    event2.motion.yrel = event.jball.yrel - y;
                    if (event.jball.xrel!=0) {
                        x = event.jball.xrel;
                        y = event.jball.yrel;
                    }
                    event2.motion.x = x;
                    event2.motion.y = y;
                    //__android_log_print(ANDROID_LOG_INFO, "GUICHAN","Mouse motion %d %d btns %d", x, y, buttons);
                    if (buttons == 0) {
                        // Push mouse motion event first, then button down event
                        msg_input->pushInput(event2);
                        buttons = SDL_BUTTON_LMASK;
                        event2.type = SDL_MOUSEBUTTONDOWN;
                        event2.button.which = 0;
                        event2.button.button = SDL_BUTTON_LEFT;
                        event2.button.state =  SDL_PRESSED;
                        event2.button.x = x;
                        event2.button.y = y;
                        //__android_log_print(ANDROID_LOG_INFO, "GUICHAN","Mouse button %d coords %d %d", buttons, x, y);
                    }
                }
                if (event.type == SDL_JOYBUTTONUP &&
                    event.jbutton.which == 0 &&
                    event.jbutton.button == 0) {
                    // Do not push button down event here, because we need mouse motion event first
                    buttons = 0;
                    event2.type = SDL_MOUSEBUTTONUP;
                    event2.button.which = 0;
                    event2.button.button = SDL_BUTTON_LEFT;
                    event2.button.state = SDL_RELEASED;
                    event2.button.x = x;
                    event2.button.y = y;
                    //__android_log_print(ANDROID_LOG_INFO, "GUICHAN","Mouse button %d coords %d %d", buttons, x, y);
                }
                msg_input->pushInput(event2);
            }
#else
            msg_input->pushInput(event);
#endif
    }

    // Now we let the Gui object perform its logic.
    msg_gui->logic();
    // Now we let the Gui object draw itself.
    msg_gui->draw();
    // Finally we update the screen.
    SDL_Flip(prSDLScreen);
  }

  msg_top->remove(wndMsg);

  delete txtMsg;
  delete cmdDone;
  delete doneActionListener;
  delete wndMsg;
  
  delete msg_font;
  delete msg_top;
  
  delete msg_gui;
  delete msg_input;
  delete msg_graphics;
}
