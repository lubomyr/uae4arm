#ifndef GCN_UAELISTBOX_HPP
#define GCN_UAELISTBOX_HPP

#include <list>
#include <vector>

#ifdef USE_SDL2
#include <guisan/listmodel.hpp>
#include <guisan/platform.hpp>
#include <guisan/widget.hpp>
#include <guisan/widgets/listbox.hpp>
#else
#include "guichan/listmodel.hpp"
#include "guichan/platform.hpp"
#include "guichan/widget.hpp"
#include "guichan/widgets/listbox.hpp"
#endif


namespace gcn
{
  class GCN_CORE_DECLSPEC UaeListBox : public ListBox
  {
    public:
      UaeListBox();

		  explicit UaeListBox(ListModel* listModel);

      virtual ~UaeListBox();

      virtual void draw(Graphics* graphics);
      
      void AddColumn(int position, ListModel* listModel);
      
    protected:
      std::vector<ListModel *> subColumnList;
      std::vector<int> subColumnPos;

  };
}


#endif // end GCN_UAELISTBOX_HPP
