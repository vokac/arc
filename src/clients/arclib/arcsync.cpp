// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <list>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <arc/ArcLocation.h>
#include <arc/client/Endpoint.h>
#include <arc/DateTime.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/UserConfig.h>
#include <arc/client/EndpointRetriever.h>
#include <arc/client/Job.h>

class JobSynchronizer : public Arc::EndpointConsumer<Arc::ServiceEndpoint> {
public:
  JobSynchronizer(
    const Arc::UserConfig& uc,
    const std::list<Arc::ServiceEndpoint>& services,
    const std::list<std::string>& preferredInterfaceNames = std::list<std::string>(),
    const std::list<std::string>& capabilityFilter = std::list<std::string>(1, Arc::ComputingInfoEndpoint::ComputingInfoCapability)
  ) : uc(uc), ser(uc, Arc::EndpointQueryOptions<Arc::ServiceEndpoint>(true, capabilityFilter)),
      jlr(uc, Arc::EndpointQueryOptions<Arc::Job>(preferredInterfaceNames))
  {
    ser.addConsumer(*this);
    jlr.addConsumer(jobs);
    
    for (std::list<Arc::ServiceEndpoint>::const_iterator it = services.begin(); it != services.end(); it++) {
      if (Arc::RegistryEndpoint::isRegistry(*it)) {
        ser.addEndpoint(Arc::RegistryEndpoint(*it));
      } else {
        jlr.addEndpoint(Arc::ComputingInfoEndpoint(*it));
      }
    }
  }

  void wait() {
    ser.wait();
    jlr.wait();
  }

  void addEndpoint(const Arc::ServiceEndpoint& service) {
    if (Arc::ComputingInfoEndpoint::isComputingInfo(service)) {
      jlr.addEndpoint(Arc::ComputingInfoEndpoint(service));
    }
  }

  bool writeJobs(bool truncate) {
    bool jobsWritten = false;
    bool jobsReported = false;
    // Write extracted job info to joblist
    if (truncate) {
      if ( (jobsWritten = Arc::Job::WriteJobsToTruncatedFile(uc.JobListFile(), jobs)) ) {
        for (std::list<Arc::Job>::const_iterator it = jobs.begin();
             it != jobs.end(); it++) {
          if (!jobsReported) {
            std::cout << Arc::IString("Found the following jobs:")<<std::endl;
            jobsReported = true;
          }
          if (!it->Name.empty()) {
            std::cout << it->Name << " (" << it->JobID.fullstr() << ")" << std::endl;
          }
          else {
            std::cout << it->JobID.fullstr() << std::endl;
          }
        }
        std::cout << Arc::IString("Total number of jobs found: ") << jobs.size() << std::endl;
      }
    }
    else {
      std::list<const Arc::Job*> newJobs;
      if ( (jobsWritten = Arc::Job::WriteJobsToFile(uc.JobListFile(), jobs, newJobs)) ) {
        for (std::list<const Arc::Job*>::const_iterator it = newJobs.begin();
             it != newJobs.end(); it++) {
          if (!jobsReported) {
            std::cout << Arc::IString("Found the following new jobs:")<<std::endl;
            jobsReported = true;
          }
          if (!(*it)->Name.empty()) {
            std::cout << (*it)->Name << " (" << (*it)->JobID.fullstr() << ")" << std::endl;
          }
          else {
            std::cout << (*it)->JobID.fullstr() << std::endl;
          }
        }
        std::cout << Arc::IString("Total number of new jobs found: ") << newJobs.size() << std::endl;
      }
    }

    if (!jobsWritten) {
      std::cout << Arc::IString("ERROR: Failed to lock job list file %s", uc.JobListFile()) << std::endl;
      std::cout << Arc::IString("Please try again later, or manually clean up lock file") << std::endl;
      return false;
    }

    return true;
  }

private:
  const Arc::UserConfig& uc;
  Arc::ServiceEndpointRetriever ser;
  Arc::JobListRetriever jlr;
  Arc::EndpointContainer<Arc::Job> jobs;
};

#include "utils.h"

#ifdef TEST
#define RUNSYNC(X) test_arcsync_##X
#else
#define RUNSYNC(X) X
#endif
int RUNSYNC(main)(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::Logger logger(Arc::Logger::getRootLogger(), "arcsync");
  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  ClientOptions opt(ClientOptions::CO_SYNC, " ",
                    istring("The arcsync command synchronizes your "
                            "local job list with the information at\n"
                            "the given resources or index servers."));

  std::list<std::string> params = opt.Parse(argc, argv);

  if (opt.showversion) {
    std::cout << Arc::IString("%s version %s", "arcsync", VERSION)
              << std::endl;
    return 0;
  }

  // If debug is specified as argument, it should be set before loading the configuration.
  if (!opt.debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(opt.debug));

  if (opt.show_plugins) {
    std::list<std::string> types;
    types.push_back("HED:JobListRetrieverPlugin");
    showplugins("arcsync", types, logger);
    return 0;
  }

  Arc::UserConfig usercfg(opt.conffile, opt.joblist);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }

  if (opt.debug.empty() && !usercfg.Verbosity().empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(usercfg.Verbosity()));

  //sanity check
  if (!opt.forcesync) {
    std::cout << Arc::IString("Synchronizing the local list of active jobs with the information in the\n"
                              "information system can result in some inconsistencies. Very recently submitted\n"
                              "jobs might not yet be present, whereas jobs very recently scheduled for\n"
                              "deletion can still be present."
                              ) << std::endl;
    std::cout << Arc::IString("Are you sure you want to synchronize your local job list?") << " ["
              << Arc::IString("y") << "/" << Arc::IString("n") << "] ";
    std::string response;
    std::cin >> response;
    if (Arc::lower(response) != std::string(Arc::FindTrans("y"))) {
      std::cout << Arc::IString("Cancelling synchronization request") << std::endl;
      return 0;
    }
  }

  std::list<Arc::ServiceEndpoint> endpoints = getServicesFromUserConfigAndCommandLine(usercfg, opt.indexurls, opt.clusters);

  if (endpoints.empty()) {
    logger.msg(Arc::ERROR, "No services specified. Please configure default services in the client configuration,"
                           "or specify a cluster or index (-c or -g options, see arcsync -h).");
    return 1;
  }


  std::list<std::string> preferredInterfaceNames;
  if (usercfg.PreferredInfoInterface().empty()) {
    preferredInterfaceNames.push_back("org.nordugrid.ldapglue2");
    preferredInterfaceNames.push_back("org.ogf.emies");
  } else {
    preferredInterfaceNames.push_back(usercfg.PreferredInfoInterface());
  }

  JobSynchronizer js(usercfg, endpoints, preferredInterfaceNames);
  js.wait();
  _exit(js.writeJobs(opt.truncate) == false); // true -> 0, false -> 1.
}
