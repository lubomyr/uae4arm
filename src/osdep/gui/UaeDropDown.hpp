#ifndef GCN_UAEDROPDOWN_HPP
#define GCN_UAEDROPDOWN_HPP

#include <string>

#ifdef USE_SDL2
#include <guisan/keylistener.hpp>
#include <guisan/platform.hpp>
#include <guisan/widget.hpp>
#include <guisan/widgets/dropdown.hpp>
#else
#include "guichan/actionlistener.hpp"
#include "guichan/basiccontainer.hpp"
#include "guichan/deathlistener.hpp"
#include "guichan/focushandler.hpp"
#include "guichan/focuslistener.hpp"
#include "guichan/keylistener.hpp"
#include "guichan/listmodel.hpp"
#include "guichan/mouselistener.hpp"
#include "guichan/platform.hpp"
#include "guichan/selectionlistener.hpp"
#include "guichan/widgets/listbox.hpp"
#include "guichan/widgets/scrollarea.hpp"
#endif


namespace gcn
{
  class ListBox;
  class ListModel;
  class ScrollArea;

  class GCN_CORE_DECLSPEC UaeDropDown : 
    public ActionListener,
    public BasicContainer,
    public KeyListener,
    public MouseListener,
    public FocusListener,
    public SelectionListener
  {
    public:
      UaeDropDown(ListModel *listModel = NULL, ScrollArea *scrollArea = NULL, ListBox *listBox = NULL);

      virtual ~UaeDropDown();
      
      int getSelected() const;
      void setSelected(int selected);

      void setListModel(ListModel *listModel);
      ListModel *getListModel() const;

      void adjustHeight();

      void addSelectionListener(SelectionListener* selectionListener);
      void removeSelectionListener(SelectionListener* selectionListener);

      virtual void draw(Graphics* graphics);
      void setBaseColor(const Color& color);
      void setBackgroundColor(const Color& color);
      void setForegroundColor(const Color& color);
      void setFont(Font *font);
      void setSelectionColor(const Color& color);

      virtual Rectangle getChildrenArea();
      virtual void focusLost(const Event& event);
      virtual void action(const ActionEvent& actionEvent);
      virtual void death(const Event& event);
      virtual void keyPressed(KeyEvent& keyEvent);
      virtual void mousePressed(MouseEvent& mouseEvent);
      virtual void mouseReleased(MouseEvent& mouseEvent);
      virtual void mouseWheelMovedUp(MouseEvent& mouseEvent);
      virtual void mouseWheelMovedDown(MouseEvent& mouseEvent);
      virtual void mouseDragged(MouseEvent& mouseEvent);
      virtual void valueChanged(const SelectionEvent& event);

      virtual void setEnabled(bool enabled);
      void clearSelected(void);
      bool isDroppedDown(void);
    
    
    protected:
      virtual void drawButton(Graphics *graphics);
      virtual void dropDown();
      virtual void foldUp();
      void distributeValueChangedEvent();

      bool mDroppedDown;
      bool mPushed;
      int mFoldedUpHeight;
      int mFoldedUpY;

      ScrollArea* mScrollArea;
      ListBox* mListBox;
      FocusHandler mInternalFocusHandler;
      bool mInternalScrollArea;
      bool mInternalListBox;
      bool mIsDragged;

      typedef std::list<SelectionListener*> SelectionListenerList;
      SelectionListenerList mSelectionListeners;
      typedef SelectionListenerList::iterator SelectionListenerIterator;

      Color mBackgroundColorBackup;
      bool mPreferUp;
     
  };
}


#endif // end GCN_UAEDROPDOWN_HPP
