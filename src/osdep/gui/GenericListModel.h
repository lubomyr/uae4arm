#ifndef GCN_GENLISTMODEL_H
#define GCN_GENLISTMODEL_H

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

  class GCN_CORE_DECLSPEC GenericListModel : public gcn::ListModel
  {
  private:
    std::vector<std::string> values;
    
  public:
    GenericListModel();
    
    GenericListModel(const char *entries[], int count);
  
    GenericListModel(std::vector<std::string> list);
  
    int getNumberOfElements(void);
  
    std::string getElementAt(int i);
  
    void clear(void);
    
    void add(const char * newvalue);
  
    void add(std::string newvalue);
    
  };
}

#endif // GCN_GENLISTMODEL_H
