#ifndef GCN_UAELISTBOX_HPP
#define GCN_UAELISTBOX_HPP

#include <list>

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
  };
}


#endif // end GCN_UAELISTBOX_HPP
