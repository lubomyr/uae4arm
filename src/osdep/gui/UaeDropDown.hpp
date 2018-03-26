#ifndef GCN_UAEDROPDOWN_HPP
#define GCN_UAEDROPDOWN_HPP

#include <string>

#ifdef USE_SDL2
#include <guisan/keylistener.hpp>
#include <guisan/platform.hpp>
#include <guisan/widget.hpp>
#include <guisan/widgets/dropdown.hpp>
#else
#include "guichan/keylistener.hpp"
#include "guichan/platform.hpp"
#include "guichan/widget.hpp"
#include "guichan/widgets/dropdown.hpp"
#endif


namespace gcn
{
  class GCN_CORE_DECLSPEC UaeDropDown : public DropDown
  {
    public:
      UaeDropDown(ListModel *listModel = NULL,
                  ScrollArea *scrollArea = NULL,
                  ListBox *listBox = NULL);

      virtual ~UaeDropDown();
      
      virtual void keyPressed(KeyEvent& keyEvent);

      virtual void setEnabled(bool enabled);
      
      void clearSelected(void);

      bool isDroppedDown(void);
    
    protected:
      Color mBackgroundColorBackup;
      
  };
}


#endif // end GCN_UAEDROPDOWN_HPP
