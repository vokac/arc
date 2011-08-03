#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DataDeliveryComm.h"
#include "DataDeliveryRemoteComm.h"
#include "DataDeliveryLocalComm.h"

namespace DataStaging {

  DataDeliveryComm* DataDeliveryComm::CreateInstance(const DTR& dtr, const TransferParameters& params) {
    if (dtr.get_remote_delivery_endpoint())
      return new DataDeliveryRemoteComm(dtr, params);
    return new DataDeliveryLocalComm(dtr, params);
  }

  DataDeliveryComm::DataDeliveryComm(const DTR& dtr, const TransferParameters& params)
    : handler_(NULL),dtr_id(dtr.get_short_id()),transfer_params(params)
  {}

  DataDeliveryCommHandler::DataDeliveryCommHandler(void) {
    Glib::Mutex::Lock lock(lock_);
    Arc::CreateThreadFunction(&func,this);
  }

  void DataDeliveryCommHandler::Add(DataDeliveryComm* item) {
    Glib::Mutex::Lock lock(lock_);
    items_.push_back(item);
  }

  void DataDeliveryCommHandler::Remove(DataDeliveryComm* item) {
    Glib::Mutex::Lock lock(lock_);
    for(std::list<DataDeliveryComm*>::iterator i = items_.begin();
                        i!=items_.end();) {
      if(*i == item) {
        i=items_.erase(i);
      } else {
        ++i;
      }
    }
  }

  DataDeliveryCommHandler* DataDeliveryCommHandler::comm_handler = NULL;

  DataDeliveryCommHandler* DataDeliveryCommHandler::getInstance() {
    if(comm_handler) return comm_handler;
    return (comm_handler = new DataDeliveryCommHandler);
  }

  // This is a dedicated thread which periodically checks for
  // new state reported by comm instances and modifies states accordingly
  void DataDeliveryCommHandler::func(void* arg) {
    if(!arg) return;
    // We do not need extremely low latency, so this
    // thread simply polls for data 2 times per second.
    DataDeliveryCommHandler& it = *(DataDeliveryCommHandler*)arg;
    for(;;) {
      {
        Glib::Mutex::Lock lock(it.lock_);
        for(std::list<DataDeliveryComm*>::iterator i = it.items_.begin();
                  i != it.items_.end();++i) {
          DataDeliveryComm* comm = *i;
          if(comm)
            comm->PullStatus();
        }
      }
      Glib::usleep(500000);
    }
  }

} // namespace DataStaging
