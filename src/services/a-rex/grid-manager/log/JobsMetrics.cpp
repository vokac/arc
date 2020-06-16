#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstring>
#include <map>

#include <arc/StringConv.h>
#include <arc/Thread.h>

#include "JobsMetrics.h"

namespace ARex {

static Arc::Logger& logger = Arc::Logger::getRootLogger();

  JobStateList::JobStateList(int limit):limit(limit){
    ratio = 0.;
    failures = 0;
    counter = 0;
  }
  JobStateList::~JobStateList(){}

  JobStateList::JobNode::JobNode(int failed):failed(failed){
  }
  JobStateList::JobNode::~JobNode(){}

  int JobStateList::JobNode::getFailure(){
      int f;
      if(next==NULL){ f = failed;}
      else { f = failed + next->getFailure(); }
      return f;
    }

  int JobStateList::JobNode::getLength(){
      int length = 0;
      if(next==NULL){ length = 1;}
      else { length = 1 + next->getLength(); }
      return length;
    }



  void JobStateList::setFailure(int failed){
    int counter;
    JobStateList::JobNode* node = new JobStateList::JobNode(failed);
  if(firstNode==NULL){
    firstNode = node;
    lastNode = node;
  } else {
    node->next = firstNode;
    firstNode->prev = node;
    firstNode = node;
    counter++;

    if(counter>=limit){
      JobStateList::JobNode* lastnode = lastNode;
      lastNode = lastnode->prev;
      lastNode->next = NULL;
      counter--;
    }
  }
}

  float JobStateList::getRatio(){
    int length = 0;
    if(firstNode!=NULL){
      failures = firstNode->getFailure();
      length = firstNode->getLength();
    } else { failures=0;}
    
    if(failures==0){ratio = 0;}
    else{ ratio = (float)failures/(float)length;}
    return ratio;
  }
  
  int JobStateList::getLength(){
    int length = 0;
    if(firstNode!=NULL){length = firstNode->getLength();}
    return length;
  }
  
  int JobStateList::getFailures(){
    int totalFailure = 0;
    if(firstNode!=NULL){totalFailure = firstNode->getFailure();}
    return totalFailure;
  }





JobsMetrics::JobsMetrics():enabled(false),proc(NULL) {
  fail_ratio = 0;
  job_counter = 0;
  job_fail_counter = 0;
  std::memset(jobs_processed, 0, sizeof(jobs_processed));
  std::memset(jobs_in_state, 0, sizeof(jobs_in_state));
  std::memset(jobs_processed_changed, 0, sizeof(jobs_processed_changed));
  std::memset(jobs_in_state_changed, 0, sizeof(jobs_in_state_changed));
  std::memset(jobs_state_old_new, 0, sizeof(jobs_state_old_new));
  std::memset(jobs_state_old_new_changed, 0, sizeof(jobs_state_old_new_changed));
  std::memset(jobs_rate, 0, sizeof(jobs_rate));
  std::memset(jobs_rate_changed, 0, sizeof(jobs_rate_changed));

  fail_ratio_changed = false;

  time_lastupdate = time(NULL);

  jobstatelist= new JobStateList(100);

}

JobsMetrics::~JobsMetrics() {
}

void JobsMetrics::SetEnabled(bool val) {
  enabled = val;
}

void JobsMetrics::SetConfig(const char* fname) {
  config_filename = fname;
}
  
void JobsMetrics::SetGmetricPath(const char* path) {
  tool_path = path;
}


  void JobsMetrics::ReportJobStateChange(const GMConfig& config,  GMJobRef i, job_state_t old_state,  job_state_t new_state) {
  Glib::RecMutex::Lock lock_(lock);


  std::string job_id = i->job_id;

  /*
    ## - processingjobs -- the number of jobs currently being processed by ARC (jobs
    ##                     between PREPARING and FINISHING states) -- TO-DO
    ## - failedjobs -- the ratio of failed jobs to all jobs
    ## - jobstates -- number of jobs in different A-REX internal stages
  */
  

  /*Only hold 1 for failed or 0 for non-failed job for 100 latest jobs */
  /*does not make sense to initialize it here, it will then be reset for each job-state-change call i.e. each time reportjobstatechange is called*/

  jobstatelist->setFailure(i->CheckFailure(config));
  fail_ratio = (double)jobstatelist->getRatio();
  fail_ratio_changed = true;

  //actual states (jobstates)
  if(old_state < JOB_STATE_UNDEFINED) {
    //++(jobs_processed[old_state]);
    //jobs_processed_changed[old_state] = true;
    --(jobs_in_state[old_state]);
    jobs_in_state_changed[old_state] = true;
  };
  if(new_state < JOB_STATE_UNDEFINED) {
    ++(jobs_in_state[new_state]);
    jobs_in_state_changed[new_state] = true;
  };

  Sync();
}

bool JobsMetrics::CheckRunMetrics(void) {
  if(!proc) return true;
  if(proc->Running()) return false;
  int run_result = proc->Result();
  if(run_result != 0) {
   logger.msg(Arc::ERROR,": Metrics tool returned error code %i: %s",run_result,proc_stderr);
  };
  delete proc;
  proc = NULL;
  return true;
}

void JobsMetrics::Sync(void) {
  if(!enabled) return; // not configured
  Glib::RecMutex::Lock lock_(lock);
  if(!CheckRunMetrics()) return;
  // Run gmetric to report one change at a time
  //since only one process can be started from Sync(), only 1 histogram can be sent at a time, therefore return for each call;
  //Sync is therefore called multiple times until there are not more histograms that have changed

  if(fail_ratio_changed){
    if(RunMetrics(
		  std::string("AREX-JOBS-FAIL-RATE"),
		  Arc::tostring(fail_ratio), "double", "fail/all"
		  )) {
      fail_ratio_changed = false;
      return;
    };
  }
  

  for(int state = 0; state < JOB_STATE_UNDEFINED; ++state) {
    if(jobs_in_state_changed[state]) {
      if(RunMetrics(
          std::string("AREX-JOBS-IN_STATE-") + Arc::tostring(state) + "-" + GMJob::get_state_name(static_cast<job_state_t>(state)),
          Arc::tostring(jobs_in_state[state]), "int32", "jobs"
		    )) {
        jobs_in_state_changed[state] = false;
        return;
      };
    };
  };


}

 
bool JobsMetrics::RunMetrics(const std::string name, const std::string& value, const std::string unit_type, const std::string unit) {
  if(proc) return false;
  std::list<std::string> cmd;
  if(tool_path.empty()) {
    logger.msg(Arc::ERROR,"gmetric_bin_path empty in arc.conf (should never happen the default value should be used)");
    return false;
  } else {
    cmd.push_back(tool_path);
  };
  if(!config_filename.empty()) {
    cmd.push_back("-c");
    cmd.push_back(config_filename);
  };
  cmd.push_back("-n");
  cmd.push_back(name);
  cmd.push_back("-g");
  cmd.push_back("arc_jobs");
  cmd.push_back("-v");
  cmd.push_back(value);
  cmd.push_back("-t");//unit-type
  cmd.push_back(unit_type);
  cmd.push_back("-u");//unit
  cmd.push_back(unit);
  
  proc = new Arc::Run(cmd);
  proc->AssignStderr(proc_stderr);
  proc->AssignKicker(&RunMetricsKicker, this);
  if(!(proc->Start())) {
    delete proc;
    proc = NULL;
    return false;
  };
  return true;
}

void JobsMetrics::SyncAsync(void* arg) {
  if(arg) {
    JobsMetrics& it = *reinterpret_cast<JobsMetrics*>(arg);
    Glib::RecMutex::Lock lock_(it.lock);
    if(it.proc) {
      // Continue only if no failure in previous call.
      // Otherwise it can cause storm of failed calls.
      if(it.proc->Result() == 0) {
        it.Sync();
      };
    };
  };
}

void JobsMetrics::RunMetricsKicker(void* arg) {
  // Currently it is not allowed to start new external process
  // from inside process licker (todo: redesign).
  // So do it asynchronously from another thread.
  Arc::CreateThreadFunction(&SyncAsync, arg);
}

} // namespace ARex
