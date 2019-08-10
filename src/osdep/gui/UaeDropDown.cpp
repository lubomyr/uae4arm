#include "UaeDropDown.hpp"
#ifdef USE_SDL2
#include <guisan/widgets/dropdown.hpp>
#include <guisan/key.hpp>
#else
#include "guichan/exception.hpp"
#include "guichan/font.hpp"
#include "guichan/graphics.hpp"
#include "guichan/key.hpp"
#include "guichan/listmodel.hpp"
#include "guichan/mouseinput.hpp"
#include "guichan/widgets/listbox.hpp"
#include "guichan/widgets/scrollarea.hpp"
#endif


namespace gcn
{
  UaeDropDown::UaeDropDown(ListModel *listModel, ScrollArea *scrollArea, ListBox *listBox)
  {
    setWidth(100);
    setFocusable(true);
    mDroppedDown = false;
    mPushed = false;
    mIsDragged = false;

    setInternalFocusHandler(&mInternalFocusHandler);

    mInternalScrollArea = (scrollArea == NULL);
    mInternalListBox = (listBox == NULL);

    if (mInternalScrollArea) {
      mScrollArea = new ScrollArea();
    } else {
      mScrollArea = scrollArea;
    }

    if (mInternalListBox) {
      mListBox = new ListBox();
    } else {
      mListBox = listBox;
    }

    mScrollArea->setContent(mListBox);
    add(mScrollArea);

    mListBox->addActionListener(this);
    mListBox->addSelectionListener(this);

    setListModel(listModel);

    if (mListBox->getSelected() < 0) {
      mListBox->setSelected(0);
    }

    addMouseListener(this);
    addKeyListener(this);
    addFocusListener(this);

    mPreferUp = false;
    mFoldedUpY = -1;
    adjustHeight();
#ifdef ANDROID
    mScrollArea->setScrollbarWidth(30);
#else
    mScrollArea->setScrollbarWidth(20);
#endif
  }

  UaeDropDown::~UaeDropDown()
  {
    if (widgetExists(mListBox)) {
      mListBox->removeActionListener(this);
      mListBox->removeSelectionListener(this);
    }

    if (mInternalScrollArea) {
      delete mScrollArea;
    }

    if (mInternalListBox) {
      delete mListBox;
    }

    setInternalFocusHandler(NULL);
  }
    
    
  void UaeDropDown::draw(Graphics* graphics)
  {
    int h;
    int y;
    
    if (mDroppedDown) {
      h = mFoldedUpHeight;
      if(getY() == mFoldedUpY)
        y = 0;
      else
        y = getHeight() - mFoldedUpHeight;
    } else {
      h = getHeight();
      y = 0;
    }

    Color faceColor = getBaseColor();
    Color highlightColor, shadowColor;
    int alpha = getBaseColor().a;
    highlightColor = faceColor + 0x303030;
    highlightColor.a = alpha;
    shadowColor = faceColor - 0x303030;
    shadowColor.a = alpha;

    // Draw a border.
    graphics->setColor(shadowColor);
    graphics->drawLine(0, y, getWidth() - 1, y);
    graphics->drawLine(0, y + 1, 0, y + h - 2);
    graphics->setColor(highlightColor);
    graphics->drawLine(getWidth() - 1, y + 1, getWidth() - 1, y + h - 1);
    graphics->drawLine(0, y + h - 1, getWidth() - 1, y + h - 1);

    // Push a clip area so the other drawings don't need to worry
    // about the border.
    graphics->pushClipArea(Rectangle(1, y + 1, getWidth() - 2, y + h - 2));
    const Rectangle currentClipArea = graphics->getCurrentClipArea();

    graphics->setColor(getBackgroundColor());
    graphics->fillRectangle(Rectangle(0, 0, currentClipArea.width, currentClipArea.height));
     
    if (isFocused()) {
      graphics->setColor(getSelectionColor());
      graphics->fillRectangle(Rectangle(0, 0, currentClipArea.width - currentClipArea.height, currentClipArea.height));
      graphics->setColor(getForegroundColor());
    }

    if (mListBox->getListModel() && mListBox->getSelected() >= 0) {
      graphics->setColor(getForegroundColor());
      graphics->setFont(getFont());

      graphics->drawText(mListBox->getListModel()->getElementAt(mListBox->getSelected()), 1, 0);
    }
      
    // Push a clip area before drawing the button.
    graphics->pushClipArea(Rectangle(currentClipArea.width - currentClipArea.height, 0, currentClipArea.height, currentClipArea.height));
    drawButton(graphics);
    graphics->popClipArea();
    graphics->popClipArea();

    if (mDroppedDown) {
      // Draw a border around the children.
      graphics->setColor(shadowColor);
      if(getY() == mFoldedUpY)
        graphics->drawRectangle(Rectangle(0, mFoldedUpHeight, getWidth(), getHeight() - mFoldedUpHeight));
      else
        graphics->drawRectangle(Rectangle(0, 0, getWidth(), getHeight() - mFoldedUpHeight));
      drawChildren(graphics);
    }
  }


  void UaeDropDown::drawButton(Graphics *graphics)
  {
    Color faceColor, highlightColor, shadowColor;
    int offset;
    int alpha = getBaseColor().a;

    if (mPushed) {
      faceColor = getBaseColor() - 0x303030;
      faceColor.a = alpha;
      highlightColor = faceColor - 0x303030;
      highlightColor.a = alpha;
      shadowColor = faceColor + 0x303030;
      shadowColor.a = alpha;
      offset = 1;
    } else {
      faceColor = getBaseColor();
      faceColor.a = alpha;
      highlightColor = faceColor + 0x303030;
      highlightColor.a = alpha;
      shadowColor = faceColor - 0x303030;
      shadowColor.a = alpha;
      offset = 0;
    }

    const Rectangle currentClipArea = graphics->getCurrentClipArea();
    graphics->setColor(highlightColor);
    graphics->drawLine(0, 0, currentClipArea.width - 1, 0);
    graphics->drawLine(0, 1, 0, currentClipArea.height - 1);
    graphics->setColor(shadowColor);
    graphics->drawLine(currentClipArea.width - 1, 1, currentClipArea.width - 1, currentClipArea.height - 1);
    graphics->drawLine(1, currentClipArea.height - 1, currentClipArea.width - 2, currentClipArea.height - 1);

    graphics->setColor(faceColor);
    graphics->fillRectangle(Rectangle(1, 1, currentClipArea.width - 2, currentClipArea.height - 2));

    graphics->setColor(getForegroundColor());

    int i;
    int n = currentClipArea.height / 3;
    int dx = currentClipArea.height / 2;
    int dy = (currentClipArea.height * 2) / 3;
    for (i = 0; i < n; i++) {
      graphics->drawLine(dx - i + offset, dy - i + offset, dx + i + offset, dy - i + offset);
    }
  }

  int UaeDropDown::getSelected() const
  {
    return mListBox->getSelected();
  }

  void UaeDropDown::setSelected(int selected)
  {
    if (selected >= 0) {
      mListBox->setSelected(selected);
    }
  }

  void UaeDropDown::keyPressed(KeyEvent& keyEvent)
  {
    if (keyEvent.isConsumed())
      return;
      
    Key key = keyEvent.getKey();

    if ((key.getValue() == Key::ENTER || key.getValue() == Key::SPACE) && !mDroppedDown) {
      dropDown();
      keyEvent.consume();
    } else if (key.getValue() == Key::UP) {
      setSelected(getSelected() - 1);
      keyEvent.consume();
#ifdef USE_SDL2
      distributeValueChangedEvent();
#else
      distributeActionEvent();
#endif
    } else if (key.getValue() == Key::DOWN) {
      setSelected(getSelected() + 1);
      keyEvent.consume();
#ifdef USE_SDL2
      distributeValueChangedEvent();
#else
      distributeActionEvent();
#endif
    }
  }

  void UaeDropDown::mousePressed(MouseEvent& mouseEvent)
  {        
    // If we have a mouse press on the widget.
    if (0 <= mouseEvent.getY() && mouseEvent.getY() < getHeight()
    && mouseEvent.getX() >= 0 && mouseEvent.getX() < getWidth()
    && mouseEvent.getButton() == MouseEvent::LEFT
    && !mDroppedDown && mouseEvent.getSource() == this) {
      mPushed = true;
      dropDown();
      requestModalMouseInputFocus();
    }
    // Fold up the listbox if the upper part is clicked after fold down
    else if (mouseEvent.getX() >= 0 && mouseEvent.getX() < getWidth()
    && mouseEvent.getButton() == MouseEvent::LEFT
    && mDroppedDown && mouseEvent.getSource() == this) {
      int y;      
      if(getY() == mFoldedUpY)
        y = 0;
      else
        y = getHeight() - mFoldedUpHeight;
      if(y <= mouseEvent.getY() && mouseEvent.getY() < y + mFoldedUpHeight) {
        mPushed = false;
        foldUp();
        releaseModalMouseInputFocus();
      }
    }
    // If we have a mouse press outside the widget
    else if (0 > mouseEvent.getY() || mouseEvent.getY() >= getHeight()
    || mouseEvent.getX() < 0 || mouseEvent.getX() >= getWidth()) {
      mPushed = false;
      foldUp();
    }
  }

  void UaeDropDown::mouseReleased(MouseEvent& mouseEvent)
  {
    if (mIsDragged) {
      mPushed = false;
    }

    // Released outside of widget. Can happen when we have modal input focus.
    if ((0 > mouseEvent.getY() || mouseEvent.getY() >= getHeight()
    || mouseEvent.getX() < 0 || mouseEvent.getX() >= getWidth())
    && mouseEvent.getButton() == MouseEvent::LEFT && isModalMouseInputFocused()) {
      releaseModalMouseInputFocus();
      if (mIsDragged) {
        foldUp();
      }
    } else if (mouseEvent.getButton() == MouseEvent::LEFT) {
      mPushed = false;
    }

    mIsDragged = false;
  }

  void UaeDropDown::mouseDragged(MouseEvent& mouseEvent)
  {
    mIsDragged = true;

    mouseEvent.consume();
  }

  void UaeDropDown::setListModel(ListModel *listModel)
  {
    mListBox->setListModel(listModel);

    if (mListBox->getSelected() < 0) {
      mListBox->setSelected(0);
    }

    adjustHeight();
  }

  ListModel *UaeDropDown::getListModel() const
  {
    return mListBox->getListModel();
  }

  void UaeDropDown::adjustHeight()
  {
    if (mScrollArea == NULL) {
      throw GCN_EXCEPTION("Scroll area has been deleted.");
    }

    if (mListBox == NULL) {
      throw GCN_EXCEPTION("List box has been deleted.");
    }

    int listBoxHeight = mListBox->getHeight();
    
    // We add 2 for the border
    int h2 = getFont()->getHeight() + 2;

    setHeight(h2);

    // The addition/subtraction of 2 compensates for the seperation lines
    // seperating the selected element view and the scroll area.
    if (mDroppedDown && getParent()) {
      int parentHeight = getParent()->getChildrenArea().height;

      if(parentHeight - mFoldedUpY < getFont()->getHeight() * 4
      && mFoldedUpY > getFont()->getHeight() * 4){
        // More space in up direction
        int h = mFoldedUpY + mFoldedUpHeight;
        
        if (listBoxHeight > h - h2 - 2) {
          mScrollArea->setHeight(h - h2 - 2);
          setY(0);
          setHeight(h);
        } else {
          setY(mFoldedUpY - listBoxHeight - 2);
          setHeight(listBoxHeight + h2 + 2);
          mScrollArea->setHeight(listBoxHeight);
        }
        
      } else {
        int h = parentHeight - getY();
  
        if (listBoxHeight > h - h2 - 2) {
          mScrollArea->setHeight(h - h2 - 2);
          setHeight(h);
        } else {
          setHeight(listBoxHeight + h2 + 2);
          mScrollArea->setHeight(listBoxHeight);
        }
      }
    }
    
    if(!mDroppedDown) {
      if(mFoldedUpY >= 0)
        setY(mFoldedUpY);
      mFoldedUpY = -1;
    }

    mScrollArea->setWidth(getWidth());
    // Resize the ListBox to exactly fit the ScrollArea.
    mListBox->setWidth(mScrollArea->getChildrenArea().width);
    mScrollArea->setPosition(0, 0);
  }

  void UaeDropDown::dropDown()
  {
    if (!mDroppedDown) {
      mDroppedDown = true;
      mFoldedUpHeight = getHeight();
      mFoldedUpY = getY();
      adjustHeight();

      if (getParent()) {
        getParent()->moveToTop(this);
      }
    }

    mListBox->requestFocus();
  }

  void UaeDropDown::foldUp()
  {
    if (mDroppedDown) {
      mDroppedDown = false;
      adjustHeight();
      mInternalFocusHandler.focusNone();
    }
  }

  void UaeDropDown::focusLost(const Event& event)
  {
    foldUp();
    mInternalFocusHandler.focusNone();
  }


  void UaeDropDown::death(const Event& event)
  {        
    if (event.getSource() == mScrollArea) {
      mScrollArea = NULL;
    }

    BasicContainer::death(event);
  }

  void UaeDropDown::action(const ActionEvent& actionEvent)
  {
    foldUp();
    releaseModalMouseInputFocus();
    distributeActionEvent();
  }

  Rectangle UaeDropDown::getChildrenArea()
  {
    if (mDroppedDown) {
      // Calculate the children area (with the one pixel border in mind)
      if(getY() == mFoldedUpY) {
        return Rectangle(1, mFoldedUpHeight + 1, getWidth() - 2, getHeight() - mFoldedUpHeight - 2);
      } else {
        return Rectangle(1, 1, getWidth() - 2, getHeight() - mFoldedUpHeight - 2);
      }
    }

    return Rectangle();
  }

  void UaeDropDown::setBaseColor(const Color& color)
  {
    if (mInternalScrollArea) {
      mScrollArea->setBaseColor(color);
    }

    if (mInternalListBox) {
      mListBox->setBaseColor(color);
    }

    Widget::setBaseColor(color);
  }

  void UaeDropDown::setBackgroundColor(const Color& color)
  {
    if (mInternalScrollArea) {
      mScrollArea->setBackgroundColor(color);
    }

    if (mInternalListBox) {
      mListBox->setBackgroundColor(color);
    }

    Widget::setBackgroundColor(color);
  }

  void UaeDropDown::setForegroundColor(const Color& color)
  {
    if (mInternalScrollArea) {
      mScrollArea->setForegroundColor(color);
    }

    if (mInternalListBox) {
      mListBox->setForegroundColor(color);
    }

    Widget::setForegroundColor(color);
  }

	void UaeDropDown::setFont(Font *font)
	{
		if (mInternalScrollArea) {
      mScrollArea->setFont(font);
    }

    if (mInternalListBox) {
      mListBox->setFont(font);
    }

    Widget::setFont(font);
	}

	void UaeDropDown::mouseWheelMovedUp(MouseEvent& mouseEvent)
	{
    if (isFocused() && mouseEvent.getSource() == this) {                   
      mouseEvent.consume();

      if (mListBox->getSelected() > 0) {
        mListBox->setSelected(mListBox->getSelected() - 1);
      }
    }
  }

  void UaeDropDown::mouseWheelMovedDown(MouseEvent& mouseEvent)
  {
    if (isFocused() && mouseEvent.getSource() == this) {            
      mouseEvent.consume();

      mListBox->setSelected(mListBox->getSelected() + 1);
    }
  }

  void UaeDropDown::setSelectionColor(const Color& color)
  {
    Widget::setSelectionColor(color);
    
    if (mInternalListBox) {
      mListBox->setSelectionColor(color);
    }       
  }

  void UaeDropDown::valueChanged(const SelectionEvent& event)
  {
    distributeValueChangedEvent();
  }

  void UaeDropDown::addSelectionListener(SelectionListener* selectionListener)
  {
    mSelectionListeners.push_back(selectionListener);
  }
 
  void UaeDropDown::removeSelectionListener(SelectionListener* selectionListener)
  {
    mSelectionListeners.remove(selectionListener);
  }

  void UaeDropDown::distributeValueChangedEvent()
  {
    SelectionListenerIterator iter;

    for (iter = mSelectionListeners.begin(); iter != mSelectionListeners.end(); ++iter) {
      SelectionEvent event(this);
      (*iter)->valueChanged(event);
    }
  }

  void UaeDropDown::clearSelected(void)
  {
    mListBox->setSelected(-1);
  }

  bool UaeDropDown::isDroppedDown(void)
  {
    return mDroppedDown;
  }
    
  void UaeDropDown::setEnabled(bool enabled)
  {
    if(mEnabled != enabled) {
      mEnabled = enabled;
      if(mEnabled)
        mBackgroundColor = mBackgroundColorBackup;
      else {
        mBackgroundColorBackup = mBackgroundColor;
        mBackgroundColor = mBackgroundColor - 0x303030;
      }
    }
  }
}
