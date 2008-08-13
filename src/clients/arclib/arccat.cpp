#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <list>
#include <string>

#include <arc/ArcLocation.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/OptionParser.h>
#include <arc/client/JobSupervisor.h>
#include <arc/client/UserConfig.h>


static Arc::Logger logger(Arc::Logger::getRootLogger(), "arccat");

void arccat(const std::list<std::string>& jobs,
	    const std::list<std::string>& clusterselect,
	    const std::list<std::string>& clusterreject,
	    const std::list<std::string>& status,
	    const std::string joblist,
	    const bool show_stdout,
	    const bool show_stderr,
	    const bool show_gmlog,
	    const int timeout){

  Arc::UserConfig uc;
  if (!uc)
    return;

  Arc::JobSupervisor JobMaster(jobs, clusterselect, clusterreject,
			       joblist.empty() ? uc.JobsFile() : joblist);

  std::list<Arc::JobController*> TheJobControllers = JobMaster.GetJobControllers();
  
  if(TheJobControllers.empty())
    return;
  std::cout<<"Number of JobControllers: "<<TheJobControllers.size() <<std::endl; 
  
  std::list<Arc::JobController*>::iterator iter;
  
  std::string whichfile;
  if(show_gmlog)
    whichfile = "gmlog";
  else if(show_stderr)
    whichfile = "stderr";
  else
    whichfile = "stdout";

  //This may benefit from being threaded
  for(iter = TheJobControllers.begin(); iter != TheJobControllers.end(); iter++){
    (*iter)->Cat(jobs,
		 clusterselect,
		 clusterreject,
		 status,
		 whichfile,
		 timeout);
  }

}

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::LogStream logcerr(std::cerr);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  Arc::OptionParser options(istring("[jobid]"), "", istring(
			      "Argument to -c has format:\n"
			      "GRID:URL e.g.\n"
			      "ARC0:ldap://grid.tsl.uu.se:2135/nordugrid-cluster-name=grid.tsl.uu.se,Mds-Vo-name=local,o=grid"));

  bool all = false;
  options.AddOption('a', "all",
		    istring("all jobs"),
		    all);

  std::string joblist;
  options.AddOption('j', "joblist",
		    istring("file containing a list of jobids"),
		    istring("filename"),
		    joblist);

  std::list<std::string> clustertemp;
  options.AddOption('c', "cluster",
		    istring("explicity select or reject a specific cluster"),
		    istring("[-]name"),
		    clustertemp);

  std::list<std::string> status;
  options.AddOption('s', "status",
		    istring("only select jobs whose status is statusstr"),
		    istring("statusstr"),
		    status);
  bool show_stdout = true;
  options.AddOption('o', "stdout",
		    istring("show stdout of the job (default)"),
		    show_stdout);
  bool show_stderr = false;
  options.AddOption('e', "stderr",
		    istring("show stderr of the job"),
		    show_stderr);
  /*
  bool follow = false;
  options.AddOption('f', "follow",
		    istring("show tail of requested file and follow it's changes"),
		    follow);
  */
  bool show_gmlog = false;
  options.AddOption('l', "gmlog",
		    istring("show the grid manager's error log of the job"),
		    show_gmlog); 
  int timeout = 20;
  options.AddOption('t', "timeout", istring("timeout in seconds (default 20)"),
		    istring("seconds"), timeout);

  std::string debug;
  options.AddOption('d', "debug",
		    istring("FATAL, ERROR, WARNING, INFO, DEBUG or VERBOSE"),
		    istring("debuglevel"), debug);

  bool version = false;
  options.AddOption('v', "version", istring("print version information"),
		    version);

  std::list<std::string> params = options.Parse(argc, argv);

  //sort clustertemp into clusterselect and clusterreject
  std::list<std::string> clusterselect;
  std::list<std::string> clusterreject;
  for (std::list<std::string>::iterator it = clustertemp.begin();
       it != clustertemp.end(); it++)
    if ((*it).find_first_of('-') == 0)
      clusterreject.push_back(*it);
    else
      clusterselect.push_back(*it);

  if (!debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(debug));

  if (version) {
    std::cout << Arc::IString("%s version %s", "arccat", VERSION) << std::endl;
    return 0;
  }

  if (params.empty() && all==false) {
    logger.msg(Arc::ERROR,
	       "No valid jobids given");
    return 1;
  }

  arccat(params, clusterselect, clusterreject, status,
	 joblist, show_stdout, show_stderr, show_gmlog, timeout);

  return 0;
}
