#ifndef __ARC_REQUEST_H__
#define __ARC_REQUEST_H__

#include <list>
#include <fstream>
#include "../../../../libs/common/XMLNode.h"
#include "../../../../libs/common/Logger.h"

/** Basic class for Request*/

namespace Arc {

typedef std::list<RequestItem*> ReqItemList;

/**A request can has a few <subjects, actions, objects> tuples */
//**There can be different types of subclass which inherit Request, such like XACMLRequest, ArcRequest, GACLRequest */
class Request {
protect:
  ReqItemList rlist;
public:
  virtual ReqItemList getRequestItems () const {};
  virtual void setRequestItems (const ReqItemList* sl) {};

  //**Parse request information from a input stream, such as one file*/
  Request (const std::ifstream& input) {};

  //**Parse request information from a xml stucture in memory*/
  Request (const Arc::XMLNode& node) {};
  virtual ~Request();
};

} // namespace Arc

#endif /* __ARC_REQUEST_H__ */
