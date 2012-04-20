// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>

#include <algorithm>

#include <arc/ArcConfig.h>
#include <arc/FileLock.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/XMLNode.h>
#include <arc/client/JobController.h>
#include <arc/data/DataHandle.h>
#include <arc/data/DataMover.h>
#include <arc/data/FileCache.h>

#include "Job.h"

#define JXMLTOSTRING(NAME) \
    if (job[ #NAME ]) {\
      NAME = (std::string)job[ #NAME ];\
    }

#define JXMLSTRINGTO(TYPE, NAME) \
    if (job[ #NAME ]) {\
      TYPE temp##TYPE##NAME;\
      if (stringto((std::string)job[ #NAME ], temp##TYPE##NAME)) {\
        NAME = temp##TYPE##NAME;\
      }\
    }

#define JXMLTOTIME(NAME) \
    if (job[ #NAME ]) {\
      Time temp##NAME((std::string)job[ #NAME ]);\
      if (temp##NAME.GetTime() != -1) {\
        NAME = temp##NAME;\
      }\
    }

#define JXMLTOSTRINGLIST(NAME) \
    NAME.clear();\
    for (XMLNode n = job[ #NAME ]; n; ++n) {\
      NAME.push_back((std::string)n);\
    }

#define STRINGTOXML(NAME) \
    if (!(NAME).empty()) {\
      node.NewChild( #NAME ) = NAME;\
    }

#define URLTOXML(NAME) \
    if (NAME) {\
      node.NewChild( #NAME ) = NAME.fullstr();\
    }

#define INTTOXML(NAME) \
    if (NAME != -1) {\
      node.NewChild( #NAME ) = tostring(NAME);\
    }

#define TIMETOSTRING(NAME) \
    if (NAME != -1) {\
      node.NewChild( #NAME ) = (NAME).str(UTCTime);\
    }

#define PERIODTOSTRING(NAME) \
    if (NAME != -1) {\
      node.NewChild( #NAME ) = (std::string)NAME;\
    }

#define STRINGLISTTOXML(NAME) \
    for (std::list<std::string>::const_iterator it = NAME.begin();\
         it != NAME.end(); it++) {\
      node.NewChild( #NAME ) = *it;\
    }

namespace Arc {

  Logger Job::logger(Logger::getRootLogger(), "Job");

  JobControllerLoader Job::loader;
  
  DataHandle* Job::data_source = NULL;
  DataHandle* Job::data_destination = NULL;

  Job::Job()
    : ExitCode(-1),
      WaitingPosition(-1),
      RequestedTotalWallTime(-1),
      RequestedTotalCPUTime(-1),
      RequestedSlots(-1),
      UsedTotalWallTime(-1),
      UsedTotalCPUTime(-1),
      UsedMainMemory(-1),
      LocalSubmissionTime(-1),
      SubmissionTime(-1),
      ComputingManagerSubmissionTime(-1),
      StartTime(-1),
      ComputingManagerEndTime(-1),
      EndTime(-1),
      WorkingAreaEraseTime(-1),
      ProxyExpirationTime(-1),
      CreationTime(-1),
      Validity(-1),
      VirtualMachine(false),
      jc(NULL) {}

  Job::~Job() {}

  Job::Job(const Job& j)
    : ExitCode(-1),
      WaitingPosition(-1),
      RequestedTotalWallTime(-1),
      RequestedTotalCPUTime(-1),
      RequestedSlots(-1),
      UsedTotalWallTime(-1),
      UsedTotalCPUTime(-1),
      UsedMainMemory(-1),
      LocalSubmissionTime(-1),
      SubmissionTime(-1),
      ComputingManagerSubmissionTime(-1),
      StartTime(-1),
      ComputingManagerEndTime(-1),
      EndTime(-1),
      WorkingAreaEraseTime(-1),
      ProxyExpirationTime(-1),
      CreationTime(-1),
      Validity(-1),
      VirtualMachine(false),
      jc(NULL) {
    *this = j;
  }

  Job::Job(XMLNode j)
    : ExitCode(-1),
      WaitingPosition(-1),
      RequestedTotalWallTime(-1),
      RequestedTotalCPUTime(-1),
      RequestedSlots(-1),
      UsedTotalWallTime(-1),
      UsedTotalCPUTime(-1),
      UsedMainMemory(-1),
      LocalSubmissionTime(-1),
      SubmissionTime(-1),
      ComputingManagerSubmissionTime(-1),
      StartTime(-1),
      ComputingManagerEndTime(-1),
      EndTime(-1),
      WorkingAreaEraseTime(-1),
      ProxyExpirationTime(-1),
      CreationTime(-1),
      Validity(-1),
      VirtualMachine(false) {
    *this = j;
  }

  Job& Job::operator=(const Job& j) {
    jc = j.jc;

    JobID = j.JobID;
    Cluster = j.Cluster;
    InterfaceName = j.InterfaceName;

    Name = j.Name;
    Type = j.Type;
    IDFromEndpoint = j.IDFromEndpoint;
    LocalIDFromManager = j.LocalIDFromManager;
    JobDescription = j.JobDescription;
    JobDescriptionDocument = j.JobDescriptionDocument;
    State = j.State;
    RestartState = j.RestartState;
    ExitCode = j.ExitCode;
    ComputingManagerExitCode = j.ComputingManagerExitCode;
    Error = j.Error;
    WaitingPosition = j.WaitingPosition;
    UserDomain = j.UserDomain;
    Owner = j.Owner;
    LocalOwner = j.LocalOwner;
    RequestedTotalWallTime = j.RequestedTotalWallTime;
    RequestedTotalCPUTime = j.RequestedTotalCPUTime;
    RequestedSlots = j.RequestedSlots;
    RequestedApplicationEnvironment = j.RequestedApplicationEnvironment;
    StdIn = j.StdIn;
    StdOut = j.StdOut;
    StdErr = j.StdErr;
    LogDir = j.LogDir;
    ExecutionNode = j.ExecutionNode;
    Queue = j.Queue;
    UsedTotalWallTime = j.UsedTotalWallTime;
    UsedTotalCPUTime = j.UsedTotalCPUTime;
    UsedMainMemory = j.UsedMainMemory;
    RequestedApplicationEnvironment = j.RequestedApplicationEnvironment;
    RequestedSlots = j.RequestedSlots;
    LocalSubmissionTime = j.LocalSubmissionTime;
    SubmissionTime = j.SubmissionTime;
    ComputingManagerSubmissionTime = j.ComputingManagerSubmissionTime;
    StartTime = j.StartTime;
    ComputingManagerEndTime = j.ComputingManagerEndTime;
    EndTime = j.EndTime;
    WorkingAreaEraseTime = j.WorkingAreaEraseTime;
    ProxyExpirationTime = j.ProxyExpirationTime;
    SubmissionHost = j.SubmissionHost;
    SubmissionClientName = j.SubmissionClientName;
    CreationTime = j.CreationTime;
    Validity = j.Validity;
    OtherMessages = j.OtherMessages;

    ActivityOldID = j.ActivityOldID;
    LocalInputFiles = j.LocalInputFiles;

    VirtualMachine = j.VirtualMachine;
    UsedCPUType = j.UsedCPUType;
    UsedOSFamily = j.UsedOSFamily;
    UsedPlatform = j.UsedPlatform;

    return *this;
  }

  int Job::operator==(const Job& other) { return JobID == other.JobID; }

  Job& Job::operator=(XMLNode job) {
    jc = NULL;

    // Information specific to how job is stored in jobs list
    if (job["JobID"]) {
      JobID = URL((std::string)job["JobID"]);
    } else if (job["IDFromEndpoint"]) { // Backwardscompatibility: Pre 2.0.0 format.
      JobID = URL((std::string)job["IDFromEndpoint"]);
    }
    if (job["IDFromEndpoint"]
        /* If this is pre 2.0.0 format then JobID element would not
         * exist, and the usage of IDFromEndpoint element corresponded
         * to the current usage of the JobID element. Therefore only set
         * the IDFromEndpoint member if the JobID _does_ exist.
         */
        && job["JobID"]
        ) {
      IDFromEndpoint = (std::string)job["IDFromEndpoint"];
    }

    JXMLTOSTRING(Name)
    JXMLTOSTRING(Cluster)
    if (job["InterfaceName"]) {
      JXMLTOSTRING(InterfaceName)
    }
    else if (job["Flavour"]) {
      if      ((std::string)job["Flavour"] == "ARC0")  InterfaceName = "org.nordugrid.gridftpjob";
      else if ((std::string)job["Flavour"] == "BES")   InterfaceName = "org.ogf.bes";
      else if ((std::string)job["Flavour"] == "ARC1")  InterfaceName = "org.nordugrid.xbes";
      else if ((std::string)job["Flavour"] == "EMIES") InterfaceName = "org.ogf.emies";
      else if ((std::string)job["Flavour"] == "TEST")  InterfaceName = "org.nordugrid.test";
    }

    if (job["InfoEndpoint"] && job["Flavour"] && (std::string)job["Flavour"] == "ARC0") {
      IDFromEndpoint = (std::string)job["InfoEndpoint"];
    }

    if (job["JobDescription"]) {
      const std::string sjobdesc = job["JobDescription"];
      if (job["JobDescriptionDocument"] || job["State"] ||
          !job["LocalSubmissionTime"]) {
        // If the 'JobDescriptionDocument' or 'State' element is set assume that the 'JobDescription' element is the GLUE2 one.
        // Default is to assume it is the GLUE2 one.
        JobDescription = sjobdesc;
      }
      else {
        // If the 'LocalSubmissionTime' element is set assume that the 'JobDescription' element contains the actual job description.
        JobDescriptionDocument = sjobdesc;
      }
    }

    JXMLTOSTRING(JobDescriptionDocument)
    JXMLTOTIME(LocalSubmissionTime)

    if (job["Associations"]["ActivityOldID"]) {
      ActivityOldID.clear();
      for (XMLNode n = job["Associations"]["ActivityOldID"]; n; ++n) {
        ActivityOldID.push_back((std::string)n);
      }
    }
    else if (job["OldJobID"]) { // Included for backwards compatibility.
      ActivityOldID.clear();
      for (XMLNode n = job["OldJobID"]; n; ++n) {
        ActivityOldID.push_back((std::string)n);
      }
    }

    if (job["Associations"]["LocalInputFile"]) {
      LocalInputFiles.clear();
      for (XMLNode n = job["Associations"]["LocalInputFile"]; n; ++n) {
        if (n["Source"] && n["CheckSum"]) {
          LocalInputFiles[(std::string)n["Source"]] = (std::string)n["CheckSum"];
        }
      }
    }
    else if (job["LocalInputFiles"]["File"]) { // Included for backwards compatibility.
      LocalInputFiles.clear();
      for (XMLNode n = job["LocalInputFiles"]["File"]; n; ++n) {
        if (n["Source"] && n["CheckSum"]) {
          LocalInputFiles[(std::string)n["Source"]] = (std::string)n["CheckSum"];
        }
      }
    }

    // Pick generic GLUE2 information
    Update(job);

    return *this;
  }

  void Job::Update(XMLNode job) {

    JXMLTOSTRING(Type)

    // TODO: find out how to treat IDFromEndpoint in case of pure GLUE2
    
    JXMLTOSTRING(LocalIDFromManager)

    /* Earlier the 'JobDescription' element in a XMLNode representing a Job
     * object contained the actual job description, but in GLUE2 the name
     * 'JobDescription' specifies the job description language which was used to
     * describe the job. Due to the name clash we must guess what is meant when
     * parsing the 'JobDescription' element.
     */

    //  TODO: same for JobDescription

    // Parse libarcclient special state format.
    if (job["State"]["General"] && job["State"]["Specific"]) {
      State.state = (std::string)job["State"]["Specific"];
      State.type = JobState::GetStateType((std::string)job["State"]["General"]);
    }
    // Only use the first state. ACC modules should set the state them selves.
    else if (job["State"] && job["State"].Size() == 0) {
      State.state = (std::string)job["State"];
      State.type = JobState::OTHER;
    }
    if (job["RestartState"]["General"] && job["RestartState"]["Specific"]) {
      RestartState.state = (std::string)job["RestartState"]["Specific"];
      RestartState.type = JobState::GetStateType((std::string)job["RestartState"]["General"]);
    }
    // Only use the first state. ACC modules should set the state them selves.
    else if (job["RestartState"] && job["RestartState"].Size() == 0) {
      RestartState.state = (std::string)job["RestartState"];
      RestartState.type = JobState::OTHER;
    }

    JXMLSTRINGTO(int, ExitCode)
    JXMLTOSTRING(ComputingManagerExitCode)
    JXMLTOSTRINGLIST(Error)
    JXMLSTRINGTO(int, WaitingPosition)
    JXMLTOSTRING(UserDomain)
    JXMLTOSTRING(Owner)
    JXMLTOSTRING(LocalOwner)
    JXMLSTRINGTO(long, RequestedTotalWallTime)
    JXMLSTRINGTO(long, RequestedTotalCPUTime)
    JXMLSTRINGTO(int, RequestedSlots)
    JXMLTOSTRINGLIST(RequestedApplicationEnvironment)
    JXMLTOSTRING(StdIn)
    JXMLTOSTRING(StdOut)
    JXMLTOSTRING(StdErr)
    JXMLTOSTRING(LogDir)
    JXMLTOSTRINGLIST(ExecutionNode)
    JXMLTOSTRING(Queue)
    JXMLSTRINGTO(long, UsedTotalWallTime)
    JXMLSTRINGTO(long, UsedTotalCPUTime)
    JXMLSTRINGTO(int, UsedMainMemory)
    JXMLTOTIME(SubmissionTime)
    JXMLTOTIME(ComputingManagerSubmissionTime)
    JXMLTOTIME(StartTime)
    JXMLTOTIME(ComputingManagerEndTime)
    JXMLTOTIME(EndTime)
    JXMLTOTIME(WorkingAreaEraseTime)
    JXMLTOTIME(ProxyExpirationTime)
    JXMLTOSTRING(SubmissionHost)
    JXMLTOSTRING(SubmissionClientName)
    JXMLTOSTRINGLIST(OtherMessages)

  }

  void Job::ToXML(XMLNode node) const {
    URLTOXML(JobID)
    STRINGTOXML(Name)
    URLTOXML(Cluster)
    STRINGTOXML(InterfaceName)
    STRINGTOXML(Type)
    STRINGTOXML(IDFromEndpoint)
    STRINGTOXML(LocalIDFromManager)
    STRINGTOXML(JobDescription)
    STRINGTOXML(JobDescriptionDocument)
    if (State) {
      node.NewChild("State");
      node["State"].NewChild("Specific") = State();
      node["State"].NewChild("General") = State.GetGeneralState();
    }
    if (RestartState) {
      node.NewChild("RestartState");
      node["RestartState"].NewChild("Specific") = RestartState();
      node["RestartState"].NewChild("General") = RestartState.GetGeneralState();
    }
    INTTOXML(ExitCode)
    STRINGTOXML(ComputingManagerExitCode)
    STRINGLISTTOXML(Error)
    INTTOXML(WaitingPosition)
    STRINGTOXML(UserDomain)
    STRINGTOXML(Owner)
    STRINGTOXML(LocalOwner)
    PERIODTOSTRING(RequestedTotalWallTime)
    PERIODTOSTRING(RequestedTotalCPUTime)
    INTTOXML(RequestedSlots)
    STRINGLISTTOXML(RequestedApplicationEnvironment)
    STRINGTOXML(StdIn)
    STRINGTOXML(StdOut)
    STRINGTOXML(StdErr)
    STRINGTOXML(LogDir)
    STRINGLISTTOXML(ExecutionNode)
    STRINGTOXML(Queue)
    PERIODTOSTRING(UsedTotalWallTime)
    PERIODTOSTRING(UsedTotalCPUTime)
    INTTOXML(UsedMainMemory)
    TIMETOSTRING(LocalSubmissionTime)
    TIMETOSTRING(SubmissionTime)
    TIMETOSTRING(ComputingManagerSubmissionTime)
    TIMETOSTRING(StartTime)
    TIMETOSTRING(ComputingManagerEndTime)
    TIMETOSTRING(EndTime)
    TIMETOSTRING(WorkingAreaEraseTime)
    TIMETOSTRING(ProxyExpirationTime)
    STRINGTOXML(SubmissionHost)
    STRINGTOXML(SubmissionClientName)
    STRINGLISTTOXML(OtherMessages)

    if ((ActivityOldID.size() > 0 || LocalInputFiles.size() > 0) && !node["Associations"]) {
      node.NewChild("Associations");
    }

    for (std::list<std::string>::const_iterator it = ActivityOldID.begin();
         it != ActivityOldID.end(); it++) {
      node["Associations"].NewChild("ActivityOldID") = *it;
    }

    for (std::map<std::string, std::string>::const_iterator it = LocalInputFiles.begin();
         it != LocalInputFiles.end(); it++) {
      XMLNode lif = node["Associations"].NewChild("LocalInputFile");
      lif.NewChild("Source") = it->first;
      lif.NewChild("CheckSum") = it->second;
    }
  }

  void Job::SaveToStream(std::ostream& out, bool longlist) const {
    out << IString("Job: %s", JobID.fullstr()) << std::endl;
    if (!Name.empty())
      out << IString(" Name: %s", Name) << std::endl;
    if (!State().empty())
      out << IString(" State: %s (%s)", State.GetGeneralState(), State())
                << std::endl;
    if (State == JobState::QUEUING && WaitingPosition != -1) {
      out << IString(" Waiting Position: %d", WaitingPosition) << std::endl;
    }

    if (ExitCode != -1)
      out << IString(" Exit Code: %d", ExitCode) << std::endl;
    if (!Error.empty()) {
      for (std::list<std::string>::const_iterator it = Error.begin();
           it != Error.end(); it++)
        out << IString(" Job Error: %s", *it) << std::endl;
    }

    if (longlist) {
      if (!Owner.empty())
        out << IString(" Owner: %s", Owner) << std::endl;
      if (!OtherMessages.empty())
        for (std::list<std::string>::const_iterator it = OtherMessages.begin();
             it != OtherMessages.end(); it++)
          out << IString(" Other Messages: %s", *it)
                    << std::endl;
      if (!Queue.empty())
        out << IString(" Queue: %s", Queue) << std::endl;
      if (RequestedSlots != -1)
        out << IString(" Requested Slots: %d", RequestedSlots) << std::endl;
      if (WaitingPosition != -1)
        out << IString(" Waiting Position: %d", WaitingPosition)
                  << std::endl;
      if (!StdIn.empty())
        out << IString(" Stdin: %s", StdIn) << std::endl;
      if (!StdOut.empty())
        out << IString(" Stdout: %s", StdOut) << std::endl;
      if (!StdErr.empty())
        out << IString(" Stderr: %s", StdErr) << std::endl;
      if (!LogDir.empty())
        out << IString(" Grid Manager Log Directory: %s", LogDir)
                  << std::endl;
      if (SubmissionTime != -1)
        out << IString(" Submitted: %s",
                             (std::string)SubmissionTime) << std::endl;
      if (EndTime != -1)
        out << IString(" End Time: %s", (std::string)EndTime)
                  << std::endl;
      if (!SubmissionHost.empty())
        out << IString(" Submitted from: %s", SubmissionHost)
                  << std::endl;
      if (!SubmissionClientName.empty())
        out << IString(" Submitting client: %s",
                             SubmissionClientName) << std::endl;
      if (RequestedTotalCPUTime != -1)
        out << IString(" Requested CPU Time: %s",
                             RequestedTotalCPUTime.istr())
                  << std::endl;
      if (UsedTotalCPUTime != -1)
        out << IString(" Used CPU Time: %s",
                             UsedTotalCPUTime.istr()) << std::endl;
      if (UsedTotalWallTime != -1)
        out << IString(" Used Wall Time: %s",
                             UsedTotalWallTime.istr()) << std::endl;
      if (UsedMainMemory != -1)
        out << IString(" Used Memory: %d", UsedMainMemory)
                  << std::endl;
      if (WorkingAreaEraseTime != -1)
        out << IString((State == JobState::DELETED) ?
                             istring(" Results were deleted: %s") :
                             istring(" Results must be retrieved before: %s"),
                             (std::string)WorkingAreaEraseTime)
                  << std::endl;
      if (ProxyExpirationTime != -1)
        out << IString(" Proxy valid until: %s",
                             (std::string)ProxyExpirationTime)
                  << std::endl;
      if (CreationTime != -1)
        out << IString(" Entry valid from: %s",
                             (std::string)CreationTime) << std::endl;
      if (Validity != -1)
        out << IString(" Entry valid for: %s",
                             Validity.istr()) << std::endl;

      if (!ActivityOldID.empty()) {
        out << IString(" Old job IDs:") << std::endl;
        for (std::list<std::string>::const_iterator it = ActivityOldID.begin();
             it != ActivityOldID.end(); ++it) {
          out << "  " << *it << std::endl;
        }
      }

      out << IString(" Cluster: %s", Cluster.fullstr()) << std::endl;
      out << IString(" Management Interface: %s", InterfaceName) << std::endl;
    }

    out << std::endl;
  } // end Print

  bool Job::GetURLToResource(ResourceType resource, URL& url) const { return jc ? jc->GetURLToJobResource(*this, resource, url) : false; }

  bool Job::ListFilesRecursive(const UserConfig& uc, const URL& dir, std::list<std::string>& files, const std::string& prefix) {
    std::list<FileInfo> outputfiles;

    DataHandle handle(dir, uc);
    if (!handle) {
      logger.msg(INFO, "Unable to list files at %s", dir.str());
      return false;
    }
    if(!handle->List(outputfiles, (Arc::DataPoint::DataPointInfoType)
                                  (DataPoint::INFO_TYPE_NAME | DataPoint::INFO_TYPE_TYPE))) {
      logger.msg(INFO, "Unable to list files at %s", dir.str());
      return false;
    }

    for (std::list<FileInfo>::iterator i = outputfiles.begin();
         i != outputfiles.end(); i++) {
      if (i->GetName() == ".." || i->GetName() == ".") {
        continue;
      }

      if (i->GetType() == FileInfo::file_type_unknown ||
          i->GetType() == FileInfo::file_type_file) {
        files.push_back(prefix + i->GetName());
      }
      else if (i->GetType() == FileInfo::file_type_dir) {
        std::string path = dir.Path();
        if (path[path.size() - 1] != '/') {
          path += "/";
        }
        URL tmpdir(dir);
        tmpdir.ChangePath(path + i->GetName());

        std::string dirname = i->GetName();
        if (dirname[dirname.size() - 1] != '/') {
          dirname += "/";
        }
        if (!ListFilesRecursive(uc, tmpdir, files, dirname)) {
          return false;
        }
      }
    }

    return true;
  }

  bool Job::CopyJobFile(const UserConfig& uc, const URL& src, const URL& dst) {
    DataMover mover;
    mover.retry(true);
    mover.secure(false);
    mover.passive(true);
    mover.verbose(false);

    logger.msg(VERBOSE, "Now copying (from -> to)");
    logger.msg(VERBOSE, " %s -> %s", src.str(), dst.str());

    URL src_(src);
    URL dst_(dst);
    src_.AddOption("checksum=no");
    dst_.AddOption("checksum=no");

    if ((!data_source) || (!*data_source) ||
        (!(*data_source)->SetURL(src_))) {
      if(data_source) delete data_source;
      data_source = new DataHandle(src_, uc);
    }
    DataHandle& source = *data_source;
    if (!source) {
      logger.msg(ERROR, "Unable to initialise connection to source: %s", src.str());
      return false;
    }

    if ((!data_destination) || (!*data_destination) ||
        (!(*data_destination)->SetURL(dst_))) {
      if(data_destination) delete data_destination;
      data_destination = new DataHandle(dst_, uc);
    }
    DataHandle& destination = *data_destination;
    if (!destination) {
      logger.msg(ERROR, "Unable to initialise connection to destination: %s",
                 dst.str());
      return false;
    }

    // Set desired number of retries. Also resets any lost
    // tries from previous files.
    source->SetTries((src.Protocol() == "file")?1:3);
    destination->SetTries((dst.Protocol() == "file")?1:3);

    // Turn off all features we do not need
    source->SetAdditionalChecks(false);
    destination->SetAdditionalChecks(false);

    FileCache cache;
    DataStatus res =
      mover.Transfer(*source, *destination, cache, URLMap(), 0, 0, 0,
                     uc.Timeout());
    if (!res.Passed()) {
      if (!res.GetDesc().empty())
        logger.msg(ERROR, "File download failed: %s - %s", std::string(res), res.GetDesc());
      else
        logger.msg(ERROR, "File download failed: %s", std::string(res));
      // Reset connection because one can't be sure how failure
      // affects server and/or connection state.
      // TODO: Investigate/define DMC behavior in such case.
      delete data_source;
      data_source = NULL;
      delete data_destination;
      data_destination = NULL;
      return false;
    }

    return true;
  }

  bool Job::ReadAllJobsFromFile(const std::string& filename, std::list<Job>& jobs, unsigned nTries, unsigned tryInterval) {
    jobs.clear();
    Config jobstorage;

    FileLock lock(filename);
    bool acquired = false;
    for (int tries = (int)nTries; tries > 0; --tries) {
      acquired = lock.acquire();
      if (acquired) {
        if (!jobstorage.ReadFromFile(filename)) {
          lock.release();
          return false;
        }
        lock.release();
        break;
      }

      if (tries == 6) {
        logger.msg(VERBOSE, "Waiting for lock on job list file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    if (!acquired) {
      return false;
    }

    XMLNodeList xmljobs = jobstorage.Path("Job");
    for (XMLNodeList::iterator xit = xmljobs.begin(); xit != xmljobs.end(); ++xit) {
      jobs.push_back(*xit);
    }

    return true;
  }

  bool Job::ReadJobsFromFile(const std::string& filename, std::list<Job>& jobs, std::list<std::string>& jobIdentifiers, bool all, const std::list<std::string>& endpoints, const std::list<std::string>& rEndpoints, unsigned nTries, unsigned tryInterval) {
    if (!ReadAllJobsFromFile(filename, jobs, nTries, tryInterval)) { return false; }

    std::list<std::string> jobIdentifiersCopy = jobIdentifiers;
    for (std::list<Arc::Job>::iterator itJ = jobs.begin();
         itJ != jobs.end();) {
      // Check if the job (itJ) is selected by the job identifies, either by job ID or Name.
      std::list<std::string>::iterator itJIdentifier = jobIdentifiers.begin();
      for (;itJIdentifier != jobIdentifiers.end(); ++itJIdentifier) {
        if ((!itJ->Name.empty() && itJ->Name == *itJIdentifier) ||
            (itJ->JobID.fullstr() == URL(*itJIdentifier).fullstr())) {
          break;
        }
      }
      if (itJIdentifier != jobIdentifiers.end()) {
        // Job explicitly specified. Remove id from the copy list, in order to keep track of used identifiers.
        std::list<std::string>::iterator itJIdentifierCopy = std::find(jobIdentifiersCopy.begin(), jobIdentifiersCopy.end(), *itJIdentifier);
        if (itJIdentifierCopy != jobIdentifiersCopy.end()) {
          jobIdentifiersCopy.erase(itJIdentifierCopy);
        }
        ++itJ;
        continue;
      }

      if (!all) {
        // Check if the job (itJ) is selected by endpoints.
        std::list<std::string>::const_iterator itC = endpoints.begin();
        for (; itC != endpoints.end(); ++itC) {
          if (itJ->Cluster.StringMatches(*itC)) {
            break;
          }
        }
        if (itC != endpoints.end()) {
          // Cluster on which job reside is explicitly specified.
          ++itJ;
          continue;
        }

        // Job is not selected - remove it.
        itJ = jobs.erase(itJ);
      }
      else {
        ++itJ;
      }
    }

    // Filter jobs on rejected clusters.
    for (std::list<std::string>::const_iterator itC = rEndpoints.begin();
         itC != rEndpoints.end(); ++itC) {
      for (std::list<Arc::Job>::iterator itJ = jobs.begin(); itJ != jobs.end();) {
        if (itJ->Cluster.StringMatches(*itC)) {
          itJ = jobs.erase(itJ);
        }
        else {
          ++itJ;
        }
      }
    }

    jobIdentifiers = jobIdentifiersCopy;

    return true;
  }

  bool Job::WriteJobsToTruncatedFile(const std::string& filename, const std::list<Job>& jobs, unsigned nTries, unsigned tryInterval) {
    Config jobfile;
    std::map<std::string, XMLNode> jobIDXMLMap;
    for (std::list<Job>::const_iterator it = jobs.begin();
         it != jobs.end(); it++) {
      std::map<std::string, XMLNode>::iterator itJobXML = jobIDXMLMap.find(it->JobID.fullstr());
      if (itJobXML == jobIDXMLMap.end()) {
        XMLNode xJob = jobfile.NewChild("Job");
        it->ToXML(xJob);
        jobIDXMLMap[it->JobID.fullstr()] = xJob;
      }
      else {
        itJobXML->second.Replace(XMLNode(NS(), "Job"));
        it->ToXML(itJobXML->second);
      }
    }

    FileLock lock(filename);
    for (int tries = (int)nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        if (!jobfile.SaveToFile(filename)) {
          lock.release();
          return false;
        }
        lock.release();
        return true;
      }

      if (tries == 6) {
        logger.msg(WARNING, "Waiting for lock on job list file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    return false;
  }

  bool Job::WriteJobsToFile(const std::string& filename, const std::list<Job>& jobs, unsigned nTries, unsigned tryInterval) {
    std::list<const Job*> newJobs;
    return WriteJobsToFile(filename, jobs, newJobs, nTries, tryInterval);
  }

  bool Job::WriteJobsToFile(const std::string& filename, const std::list<Job>& jobs, std::list<const Job*>& newJobs, unsigned nTries, unsigned tryInterval) {
    FileLock lock(filename);
    for (int tries = (int)nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        Config jobfile;
        jobfile.ReadFromFile(filename);

        // Use std::map to store job IDs to be searched for duplicates.
        std::map<std::string, XMLNode> jobIDXMLMap;
        for (Arc::XMLNode j = jobfile["Job"]; j; ++j) {
          if (!((std::string)j["JobID"]).empty()) {
            jobIDXMLMap[(std::string)j["JobID"]] = j;
          }
        }

        std::map<std::string, const Job*> newJobsMap;
        for (std::list<Job>::const_iterator it = jobs.begin(); it != jobs.end(); ++it) {
          std::map<std::string, XMLNode>::iterator itJobXML = jobIDXMLMap.find(it->JobID.fullstr());
          if (itJobXML == jobIDXMLMap.end()) {
            XMLNode xJob = jobfile.NewChild("Job");
            it->ToXML(xJob);
            jobIDXMLMap[it->JobID.fullstr()] = xJob;
            newJobsMap[it->JobID.fullstr()] = &(*it);
          }
          else {
            // Duplicate found, replace it.
            itJobXML->second.Replace(XMLNode(NS(), "Job"));
            it->ToXML(itJobXML->second);

            // Only add to newJobsMap if this is a new job, i.e. not previous present in jobfile.
            std::map<std::string, const Job*>::iterator itNewJobsMap = newJobsMap.find(it->JobID.fullstr());
            if (itNewJobsMap != newJobsMap.end()) {
              itNewJobsMap->second = &(*it);
            }
          }
        }

        // Add pointers to new Job objects to the newJobs list.
        for (std::map<std::string, const Job*>::const_iterator it = newJobsMap.begin();
             it != newJobsMap.end(); ++it) {
          newJobs.push_back(it->second);
        }

        if (!jobfile.SaveToFile(filename)) {
          lock.release();
          return false;
        }
        lock.release();
        return true;
      }

      if (tries == 6) {
        logger.msg(WARNING, "Waiting for lock on job list file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    return false;
  }

  bool Job::RemoveJobsFromFile(const std::string& filename, const std::list<URL>& jobids, unsigned nTries, unsigned tryInterval) {
    if (jobids.empty()) {
      return true;
    }

    FileLock lock(filename);
    for (int tries = nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        Config jobstorage;
        if (!jobstorage.ReadFromFile(filename)) {
          lock.release();
          return false;
        }

        XMLNodeList xmlJobs = jobstorage.Path("Job");
        for (std::list<URL>::const_iterator it = jobids.begin(); it != jobids.end(); ++it) {
          for (XMLNodeList::iterator xJIt = xmlJobs.begin(); xJIt != xmlJobs.end(); ++xJIt) {
            if ((*xJIt)["JobID"] == it->fullstr() ||
                (*xJIt)["IDFromEndpoint"] == it->fullstr() // Included for backwards compatibility.
                ) {
              xJIt->Destroy(); // Do not break, since for some reason there might be multiple identical jobs in the file.
            }
          }
        }

        if (!jobstorage.SaveToFile(filename)) {
          lock.release();
          return false;
        }
        lock.release();
        return true;
      }

      if (tries == 6) {
        logger.msg(VERBOSE, "Waiting for lock on job list file %s", filename);
      }
      Glib::usleep(tryInterval);
    }

    return false;
  }

  bool Job::ReadJobIDsFromFile(const std::string& filename, std::list<std::string>& jobids, unsigned nTries, unsigned tryInterval) {
    if (!Glib::file_test(filename, Glib::FILE_TEST_IS_REGULAR)) return false;

    FileLock lock(filename);
    for (int tries = (int)nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        std::ifstream is(filename.c_str());
        if (!is.good()) {
          is.close();
          lock.release();
          return false;
        }
        std::string line;
        while (std::getline(is, line)) {
          line = Arc::trim(line, " \t");
          if (!line.empty() && line[0] != '#') {
            jobids.push_back(line);
          }
        }
        is.close();
        lock.release();
        return true;
      }

      if (tries == 6) {
        logger.msg(WARNING, "Waiting for lock on file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    return false;
  }

  bool Job::WriteJobIDToFile(const URL& jobid, const std::string& filename, unsigned nTries, unsigned tryInterval) {
    if (Glib::file_test(filename, Glib::FILE_TEST_IS_DIR)) return false;

    FileLock lock(filename);
    for (int tries = (int)nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        std::ofstream os(filename.c_str(), std::ios::app);
        if (!os.good()) {
          os.close();
          lock.release();
          return false;
        }
        os << jobid.fullstr() << std::endl;
        bool good = os.good();
        os.close();
        lock.release();
        return good;
      }

      if (tries == 6) {
        logger.msg(WARNING, "Waiting for lock on file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    return false;
  }

  bool Job::WriteJobIDsToFile(const std::list<URL>& jobids, const std::string& filename, unsigned nTries, unsigned tryInterval) {
    if (Glib::file_test(filename, Glib::FILE_TEST_IS_DIR)) return false;
    FileLock lock(filename);
    for (int tries = (int)nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        std::ofstream os(filename.c_str(), std::ios::app);
        if (!os.good()) {
          os.close();
          lock.release();
          return false;
        }
        for (std::list<URL>::const_iterator it = jobids.begin();
             it != jobids.end(); ++it) {
          os << it->fullstr() << std::endl;
        }

        bool good = os.good();
        os.close();
        lock.release();
        return good;
      }

      if (tries == 6) {
        logger.msg(WARNING, "Waiting for lock on file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    return false;
  }

  bool Job::WriteJobIDsToFile(const std::list<Job>& jobs, const std::string& filename, unsigned nTries, unsigned tryInterval) {
    if (Glib::file_test(filename, Glib::FILE_TEST_IS_DIR)) return false;

    FileLock lock(filename);
    for (int tries = (int)nTries; tries > 0; --tries) {
      if (lock.acquire()) {
        std::ofstream os(filename.c_str(), std::ios::app);
        if (!os.good()) {
          os.close();
          lock.release();
          return false;
        }
        for (std::list<Job>::const_iterator it = jobs.begin();
             it != jobs.end(); ++it) {
          os << it->JobID.fullstr() << std::endl;
        }

        bool good = os.good();
        os.close();
        lock.release();
        return good;
      }

      if (tries == 6) {
        logger.msg(WARNING, "Waiting for lock on file %s", filename);
      }

      Glib::usleep(tryInterval);
    }

    return false;
  }

} // namespace Arc
