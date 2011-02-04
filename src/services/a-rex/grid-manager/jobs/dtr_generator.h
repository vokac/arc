#ifndef DTR_GENERATOR_H_
#define DTR_GENERATOR_H_

#include <arc/data-staging/DTR.h>
#include <arc/data-staging/Scheduler.h>

#include "job.h"


// Temporary solution till DTRGenerator becomes non-static
class DTRGeneratorCallback: public DataStaging::DTRCallback {
 public:
  virtual void receive_dtr(DataStaging::DTR dtr);
};

/**
 * A-REX implementation of DTR Generator. All members and methods
 * are static and no instance can be created.
 * TODO: make it non-static.
 */
class DTRGenerator {
 private:
  /** Active DTRs. Map of job id to DTR id. */
  static std::multimap<std::string, std::string> active_dtrs;
  /** Jobs where all DTRs are finished. Map of job id to failure reason (empty if success) */
  static std::map<std::string, std::string> finished_jobs;
  /** Lock for lists */
  static Arc::SimpleCondition lock;

  // Event lists
  /** DTRs received */
  static std::list<DataStaging::DTR> dtrs_received;
  /** Jobs received */
  static std::list<JobDescription> jobs_received;
  /** Jobs cancelled. List of Job IDs. */
  static std::list<std::string> jobs_cancelled;
  /** Lock for events */
  static Arc::SimpleCondition event_lock;

  /** Condition to wait on when stopping Generator */
  static Arc::SimpleCondition run_condition;
  /** State of Generator */
  static DataStaging::ProcessState generator_state;
  /** Job users. Map of UID to JobUser pointer, used to map a DTR or job to a JobUser. */
  static std::map<uid_t, const JobUser*> jobusers;
  /** logger */
  static Arc::Logger logger;
  /** Associated scheduler */
  static DataStaging::Scheduler scheduler;

  static DTRGeneratorCallback receive_dtr;
  
  /** Private constructors */
  DTRGenerator() {};
  DTRGenerator(const DTRGenerator& generator) {};

  /** run main thread */
  static void main_thread(void* arg);

  /** Process a received DTR */
  static bool processReceivedDTR(DataStaging::DTR& dtr);
  /** Process a received job */
  static bool processReceivedJob(const JobDescription& job);
  /** Process a cancelled job */
  static bool processCancelledJob(const std::string& jobid);

  // Utility method copied from downloader
  /** Check that user-uploadable file exists */
  static int user_file_exists(FileData &dt,const std::string& session_dir,std::list<std::string>* have_files,std::string* error = NULL);

 public:
  /**
   * Start up Generator.
   * @param user JobUsers for this Generator.
   */
  static bool start(const JobUsers& users);

  /**
   * Stop Generator
   */
  static bool stop();

  /**
   * Callback called when DTR is finished. This DTR is marked done in the
   * DTR list and if all DTRs for the job have completed, the job is marked
   * as done.
   * @param dtr DTR object sent back from the Scheduler
   */
  static void receiveDTR(DataStaging::DTR dtr);

  /**
   * A-REX sends data transfer requests to the data staging system through
   * this method. It reads the job.id.input/output files, forms DTRs and
   * sends them to the Scheduler.
   * @param job Job description object.
   */
  static void receiveJob(const JobDescription& job);

  /**
   * This method is used by A-REX to cancel on-going DTRs. A cancel request
   * is made for each DTR in the job and the method returns. The Scheduler
   * asychronously deals with cancelling the DTRs.
   * @param job The job which is being cancelled
   */
  static void cancelJob(const JobDescription& job);

  /**
   * Query status of DTRs in job. If all DTRs are finished, returns true,
   * otherwise returns false. If true is returned, the JobDescription should
   * be checked for whether the staging was successful or not by checking
   * GetFailure().
   * @param job Description of job to query. Can be modified to add a failure
   * reason.
   * @return True if all DTRs in the job are finished, false otherwise.
   */
  static bool queryJobFinished(JobDescription& job);

  /**
   * Utility method to check that all files the user was supposed to
   * upload with the job are ready.
   * @param job Job description, failures will be reported directly in
   * this object.
   * @return 0 if file exists, 1 if it is not a proper file or other error,
   * 2 if the file not there yet
   */
  static int checkUploadedFiles(JobDescription& job);
};

#endif /* DTR_GENERATOR_H_ */
