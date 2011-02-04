#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <list>
#include <string>
#include <iostream>

#include <cstring>

#include <arc/ArcLocation.h>

#include "DataDeliveryComm.h"

namespace DataStaging {

  
  // Singleton thread for handling communications
  class DataDeliveryCommHandler {
   private:
    Glib::Mutex lock_;
    static void func(void* arg);
    std::list<DataDeliveryComm*> items_;
   public:
    DataDeliveryCommHandler(void) {
      Glib::Mutex::Lock lock(lock_);
      Arc::CreateThreadFunction(&func,this);
    };
    ~DataDeliveryCommHandler(void) {
    };
    void Add(DataDeliveryComm* item) {
      Glib::Mutex::Lock lock(lock_);
      items_.push_back(item);
    };
    void Remove(DataDeliveryComm* item) {
      Glib::Mutex::Lock lock(lock_);
      for(std::list<DataDeliveryComm*>::iterator i = items_.begin();
                          i!=items_.end();) {
        if(*i == item) {
          i=items_.erase(i);
        } else {
          ++i;
        }
      }
    };
  };

  static DataDeliveryCommHandler* comm_handler = NULL;

  static DataDeliveryCommHandler* get_comm_handler(void) {
    if(comm_handler) return comm_handler;
    return (comm_handler = new DataDeliveryCommHandler);
  }

  DataDeliveryComm::DataDeliveryComm(const DTR& dtr):child_(NULL),handler_(NULL) {
    if(!dtr.get_source()) return;
    if(!dtr.get_destination()) return;
    {
      Glib::Mutex::Lock lock(lock_);
      // Initial empty status
      memset(&status_,0,sizeof(status_));
      status_.commstatus = CommInit;
      status_pos_ = 0;
      // Generate options for child
      std::list<std::string> args;
      // maybe just nordugrid_libexec_loc_?
      std::string execpath = Arc::ArcLocation::Get()+G_DIR_SEPARATOR_S+PKGLIBEXECSUBDIR+G_DIR_SEPARATOR_S+"DataStagingDelivery";
      args.push_back(execpath);
      // check for alternative source or destination eg cache, mapped URL, TURL
      std::string surl = dtr.get_source()->TransferLocations()[0].fullstr();
      bool caching = false;
      if (!dtr.get_mapped_source().empty())
        surl = dtr.get_mapped_source();
      std::string durl = dtr.get_destination()->TransferLocations()[0].fullstr();
      if ((dtr.get_cache_state() == CACHEABLE) && !dtr.get_cache_file().empty()) {
        durl = dtr.get_cache_file();
        caching = true;
      }
      args.push_back("--surl");
      args.push_back(surl);
      args.push_back("--durl");
      args.push_back(durl);
      if (!dtr.get_usercfg().ProxyPath().empty()) {
        args.push_back("--sopt");
        args.push_back("credential="+dtr.get_usercfg().ProxyPath());
        args.push_back("--dopt");
        args.push_back("credential="+dtr.get_usercfg().ProxyPath());
      }
      if (!dtr.get_usercfg().CACertificatesDirectory().empty()) {
        args.push_back("--sopt");
        args.push_back("ca="+dtr.get_usercfg().CACertificatesDirectory());
        args.push_back("--dopt");
        args.push_back("ca="+dtr.get_usercfg().CACertificatesDirectory());
      }
      child_ = new Arc::Run(args);
      // Set up pipes
      child_->KeepStdout(false);
      child_->KeepStderr(false);
      child_->KeepStdin(false);
      child_->AssignStderr(errstr_);
      if(!caching) {
        child_->AssignUserId(dtr.get_local_user().get_uid());
        child_->AssignGroupId(dtr.get_local_user().get_gid());
      }
      // Start child
      if(!child_->Start()) {
        delete child_;
        child_=NULL;
        return;
      }
    }
    handler_=get_comm_handler();
    handler_->Add(this);
  }

  DataDeliveryComm::~DataDeliveryComm(void) {
    {
      Glib::Mutex::Lock lock(lock_);
      if(child_) {
        child_->Kill(10); // Give it a chance
        delete child_; child_=NULL;  // And then kill for sure
      }
    }
    if(handler_) handler_->Remove(this);
  }

  DataDeliveryComm::Status DataDeliveryComm::GetStatus(void) const {
    Glib::Mutex::Lock lock(*(const_cast<Glib::Mutex*>(&lock_)));
    DataDeliveryComm::Status tmp = status_;
    return tmp;
  }


  // Check for new state from child and fill state accordingly.
  // Detects communication and delivery failures and delivery termination.
  void DataDeliveryComm::PullStatus(void) {
    Glib::Mutex::Lock lock(lock_);
    if(!child_) return;
    for(;;) {
      if(status_pos_ < sizeof(status_buf_)) {
        int l = child_->ReadStdout(0,((char*)&status_buf_)+status_pos_,sizeof(status_buf_)-status_pos_);
        if(l == -1) { // child error or closed comm
          if(child_->Running()) {
            status_.commstatus = CommClosed;
          } else {
            status_.commstatus = CommExited;
            if(child_->Result() != 0) status_.commstatus = CommFailed;
          }
          delete child_; child_=NULL; break;
        }
        if(l == 0) break;
        status_pos_+=l;
      }
      if(status_pos_ >= sizeof(status_buf_)) {
        status_buf_.error_desc[sizeof(status_buf_.error_desc)-1] = 0;
        status_=status_buf_;
        status_pos_-=sizeof(status_buf_);
      }
    }
  }


  // This is a dedicated thread which periodically checks for
  // new state reported by children and modifies states accordingly
  void DataDeliveryCommHandler::func(void* arg) {
    if(!arg) return;
    // Currently Run does not support waiting for events.
    // But we do not need extremely low latency. So this
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
