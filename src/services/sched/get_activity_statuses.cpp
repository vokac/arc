#include <arc/message/SOAPEnvelope.h>
#include <arc/ws-addressing/WSA.h>
#include "job.h"

namespace Arc {

Arc::MCC_Status GridSchedulerService::GetActivityStatuses(Arc::XMLNode &, Arc::XMLNode &out)
{
    return Arc::MCC_Status();
}

}
