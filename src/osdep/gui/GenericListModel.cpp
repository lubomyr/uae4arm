#include "UaeListBox.hpp"
#ifdef USE_SDL2
#include <guisan/widgets/listbox.hpp>
#include <guisan/basiccontainer.hpp>
#include <guisan/font.hpp>
#include <guisan/graphics.hpp>
#include <guisan/listmodel.hpp>
#else
#include "guichan/widgets/listbox.hpp"
#include "guichan/basiccontainer.hpp"
#include "guichan/font.hpp"
#include "guichan/graphics.hpp"
#include "guichan/listmodel.hpp"
#endif
#include "GenericListModel.h"


namespace gcn
{
  GenericListModel::GenericListModel()
    : ListModel()
  {
  }
  
  GenericListModel::GenericListModel(const char *entries[], int count)
    : ListModel()
  {
    for(int i=0; i<count; ++i)
      values.push_back(entries[i]);
  }

  GenericListModel::GenericListModel(std::vector<std::string> list)
    : ListModel()
  {
    values = list;
  }

  int GenericListModel::getNumberOfElements(void)
  {
    return values.size();
  }

  std::string GenericListModel::getElementAt(int i)
  {
    if(i < 0 || i >= values.size())
      return "";
    return values[i];
  }

  void GenericListModel::clear(void)
  {
    values.clear();
  }
  
  void GenericListModel::add(const char * newvalue)
  {
    values.push_back(newvalue);
  }

  void GenericListModel::add(std::string newvalue)
  {
    values.push_back(newvalue);
  }
  
};
