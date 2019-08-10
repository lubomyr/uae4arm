#ifndef GCN_UAERADIOBUTTON_HPP
#define GCN_UAERADIOBUTTON_HPP

#include <map>
#include <string>

#ifdef USE_SDL2
#include <guisan/platform.hpp>
#include <guisan/widget.hpp>
#include <guisan/widgets/radiobutton.hpp>
#else
#include "guichan/platform.hpp"
#include "guichan/widget.hpp"
#include "guichan/widgets/radiobutton.hpp"
#endif


namespace gcn
{
  class GCN_CORE_DECLSPEC UaeRadioButton : public RadioButton
  {
    public:
      UaeRadioButton();

      UaeRadioButton(const std::string &caption,
                  const std::string &group,
                  bool selected = false);

      virtual ~UaeRadioButton();

      virtual void draw(Graphics* graphics);

  };
}


#endif // end GCN_UAERADIOBUTTON_HPP
