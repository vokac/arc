#include <arc/URL.h>
#include <arc/message/MCC.h>

#include "CREAMClient.h"
#include "JobControllerCREAM.h"

namespace Arc {

  Logger JobControllerCREAM::logger(JobController::logger, "CREAM");

  JobControllerCREAM::JobControllerCREAM(Arc::Config *cfg)
    : JobController(cfg, "CREAM") {}

  JobControllerCREAM::~JobControllerCREAM() {}

  ACC *JobControllerCREAM::Instance(Config *cfg, ChainContext *) {
    return new JobControllerCREAM(cfg);
  }

  void JobControllerCREAM::GetJobInformation() {
    for (std::list<Job>::iterator iter = JobStore.begin(); iter != JobStore.end(); iter++) {
      MCCConfig cfg;
      PathIterator pi(iter->JobID.Path(), true);
      URL url(iter->JobID);
      url.ChangePath(*pi);
      Cream::CREAMClient gLiteClient(url, cfg);
      iter->State = gLiteClient.stat(pi.Rest());
    }
  }

  bool JobControllerCREAM::GetThisJob(Job ThisJob, std::string downloaddir) {

    logger.msg(DEBUG, "Downloading job: %s", ThisJob.InfoEndpoint.str());
    bool SuccesfulDownload = true;

    Arc::DataHandle source(ThisJob.InfoEndpoint);    
    if(source){

      std::list<std::string> downloadthese = GetDownloadFiles(source);

      std::cout<<"List of downloadable files:"<<std::endl;      
      std::cout<<"Number of files: "<< downloadthese.size()<<std::endl;      

      //loop over files
      for(std::list<std::string>::iterator i = downloadthese.begin();
	  i != downloadthese.end(); i++) {
	std::cout << *i <<std::endl;	
	std::string src = ThisJob.InfoEndpoint.str() + "/"+ *i;
	std::string path_temp = ThisJob.JobID.Path(); 
	size_t slash = path_temp.find_last_of("/");
	std::string dst;
	if(downloaddir.empty())
	  dst = path_temp.substr(slash+1) + "/" + *i;
	else
	  dst = downloaddir + "/" + *i;
	bool GotThisFile = CopyFile(src, dst);
	if(!GotThisFile)
	  SuccesfulDownload = false;
      }
    }
    else
      logger.msg(ERROR, "Failed dowloading job: %s. "
		 "Could not get data handle.", ThisJob.InfoEndpoint.str());
  }

  bool JobControllerCREAM::CleanThisJob(Job ThisJob, bool force){
    bool cleaned = true;
    
    MCCConfig cfg;
    PathIterator pi(ThisJob.JobID.Path(), true);
    URL url(ThisJob.JobID);
    url.ChangePath(*pi);
    Cream::CREAMClient gLiteClient(url, cfg);
    gLiteClient.purge(pi.Rest());
    
    return cleaned;
    
  }

  bool JobControllerCREAM::CancelThisJob(Job ThisJob){

    bool cancelled = true;
    
    MCCConfig cfg;
    PathIterator pi(ThisJob.JobID.Path(), true);
    URL url(ThisJob.JobID);
    url.ChangePath(*pi);
    Cream::CREAMClient gLiteClient(url, cfg);
    gLiteClient.cancel(pi.Rest());
    
    return cancelled;
    
  }

  URL JobControllerCREAM::GetFileUrlThisJob(Job ThisJob, std::string whichfile){};

  std::list<std::string>
  JobControllerCREAM::GetDownloadFiles(Arc::DataHandle& dir,
				       std::string dirname) {

    std::cout << "JobControllerCREAM::GetDownloadFiles" << std::endl;

    std::list<std::string> files;

    std::list<FileInfo> outputfiles;
    dir->ListFiles(outputfiles, true);

    std::cout << "ListFiles done" << std::endl;

    for(std::list<Arc::FileInfo>::iterator i = outputfiles.begin();
	i != outputfiles.end(); i++) {
      if(i->GetType() == 0 || i->GetType() == 1) {
	std::cout<<"adding file "<< i->GetName() << std::endl;
	if(!dirname.empty())
	  files.push_back(dirname + "/" + i->GetName());
	else
	  files.push_back(i->GetName());
      }
      else if(i->GetType() == 2) {
	std::cout<<"Found directory" << i->GetName() << std::endl;	
	Arc::DataHandle tmpdir(dir->str() + "/" + i->GetName());
	std::list<std::string> morefiles = GetDownloadFiles(tmpdir,
							    i->GetName());
	for(std::list<std::string>::iterator j = morefiles.begin();
	    j != morefiles.end(); j++)
	  if(!dirname.empty())
	    files.push_back(dirname + "/"+ *j);
	  else
	    files.push_back(*j);
      }
    }
    return files;
  }

} // namespace Arc
