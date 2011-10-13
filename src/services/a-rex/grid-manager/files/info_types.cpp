#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream>
#include <limits.h>
#include <errno.h>

#include <arc/StringConv.h>
#include "../misc/canonical_dir.h"
#include "../misc/escaped.h"
#include "info_types.h"

#if defined __GNUC__ && __GNUC__ >= 3

#include <limits>
#define istream_readline(__f,__s,__n) {      \
   __f.get(__s,__n,__f.widen('\n'));         \
   if(__f.fail()) __f.clear();               \
   __f.ignore(std::numeric_limits<std::streamsize>::max(), __f.widen('\n')); \
}

#else

#define istream_readline(__f,__s,__n) {      \
   __f.get(__s,__n,'\n');         \
   if(__f.fail()) __f.clear();               \
   __f.ignore(INT_MAX,'\n'); \
}

#endif

static Arc::Logger& logger = Arc::Logger::getRootLogger();

static inline void write_str(int f,const char* buf, std::string::size_type len) {
  for(;len > 0;) {
    ssize_t l = write(f,buf,len);
    if((l < 0) && (errno != EINTR)) break;
    len -= l; buf += l;
  };
}

static inline void write_chr(int f,const char c) {
  for(;;) {
    ssize_t l = write(f,&c,1);
    if((l < 0) && (errno != EINTR)) break;
    if(l == 1) break;
  };
}

void output_escaped_string(std::ostream &o,const std::string &str) {
  std::string::size_type n,nn;
  for(nn=0;;) {
//    if((n=str.find(' ',nn)) == std::string::npos) break;
    if((n=str.find_first_of(" \\\r\n",nn)) == std::string::npos) break;
    o.write(str.data()+nn,n-nn);
    o.put('\\');
    o.put(*(str.data()+n));
    nn=n+1;
  };
  o.write(str.data()+nn,str.length()-nn);
}

void output_escaped_string(int h,const std::string &str) {
  std::string::size_type n,nn;
  for(nn=0;;) {
//    if((n=str.find(' ',nn)) == std::string::npos) break;
    if((n=str.find_first_of(" \\\r\n",nn)) == std::string::npos) break;
    write_str(h,str.data()+nn,n-nn);
    write_chr(h,'\\');
    write_chr(h,*(str.data()+n));
    nn=n+1;
  };
  write_str(h,str.data()+nn,str.length()-nn);
}

std::ostream &operator<< (std::ostream &o,const FileData &fd) {
  output_escaped_string(o,fd.pfn);
  o.put(' ');
  output_escaped_string(o,fd.lfn);
  return o;
}

std::istream &operator>> (std::istream &i,FileData &fd) {
  char buf[1024];
  istream_readline(i,buf,sizeof(buf));
  fd.pfn.resize(0); fd.lfn.resize(0);
  int n=input_escaped_string(buf,fd.pfn);
  input_escaped_string(buf+n,fd.lfn);
  if((fd.pfn.length() == 0) && (fd.lfn.length() == 0)) return i; /* empty st */
  if(canonical_dir(fd.pfn) != 0) {
    logger.msg(Arc::ERROR,"Wrong directory in %s",buf);
    fd.pfn.resize(0); fd.lfn.resize(0);
  };
  return i;
}

FileData::FileData(void) {
}

FileData::FileData(const std::string& pfn_s,const std::string& lfn_s) {
  if(!pfn_s.empty()) { pfn=pfn_s; } else { pfn.resize(0); };
  if(!lfn_s.empty()) { lfn=lfn_s; } else { lfn.resize(0); };
}

FileData& FileData::operator= (const char *str) {
  pfn.resize(0); lfn.resize(0);
  int n=input_escaped_string(str,pfn);
  input_escaped_string(str+n,lfn);
  return *this;
}

bool FileData::operator== (const FileData& data) {
  // pfn may contain leading slash. It must be striped
  // before comparison.
  const char* pfn_ = pfn.c_str();
  if(pfn_[0] == '/') ++pfn_;
  const char* dpfn_ = data.pfn.c_str();
  if(dpfn_[0] == '/') ++dpfn_;
  return (strcmp(pfn_,dpfn_) == 0);
  // return (pfn == data.pfn);
}

bool FileData::operator== (const char *name) {
  if(name == NULL) return false;
  if(name[0] == '/') ++name;
  const char* pfn_ = pfn.c_str();
  if(pfn_[0] == '/') ++pfn_;
  return (strcmp(pfn_,name) == 0);
}

bool FileData::has_lfn(void) {
  return (lfn.find(':') != std::string::npos);
}


static char StateToShortcut(const std::string& state) {
  if(state == "ACCEPTED") return 'a'; // not supported
  if(state == "PREPARING") return 'b';
  if(state == "SUBMIT") return 's'; // not supported
  if(state == "INLRMS") return 'q';
  if(state == "FINISHING") return 'f';
  if(state == "FINISHED") return 'e';
  if(state == "DELETED") return 'd';
  if(state == "CANCELING") return 'c';
  return ' ';
}

JobLocalDescription& JobLocalDescription::operator=(const Arc::JobDescription& arc_job_desc)
{
  action = "request";
  std::map<std::string, std::string>::const_iterator act_i = arc_job_desc.OtherAttributes.find("nordugrid:xrsl;action");
  if(act_i != arc_job_desc.OtherAttributes.end()) action = act_i->second;
  std::map<std::string, std::string>::const_iterator jid_i = arc_job_desc.OtherAttributes.find("nordugrid:xrsl;jobid");
  if(jid_i != arc_job_desc.OtherAttributes.end()) jobid = jid_i->second;
  
  dryrun = arc_job_desc.Application.DryRun;

  projectnames.clear();
  projectnames.push_back(arc_job_desc.Identification.JobVOName);

  jobname = arc_job_desc.Identification.JobName;
  downloads = 0;
  uploads = 0;
  rtes = 0;
  outputdata.clear();
  inputdata.clear();
  rte.clear();
  transfershare="_default";

  const std::list<Arc::Software>& sw = arc_job_desc.Resources.RunTimeEnvironment.getSoftwareList();
  for (std::list<Arc::Software>::const_iterator itSW = sw.begin(); itSW != sw.end(); ++itSW, ++rtes)
    rte.push_back(std::string(*itSW));

  for (std::list<Arc::FileType>::const_iterator file = arc_job_desc.Files.begin();
       file != arc_job_desc.Files.end(); ++file) {
    std::string fname = file->Name;
    if(fname.empty()) continue; // Can handle only named files
    if(fname[0] != '/') fname = "/"+fname; // Just for safety
    // Because ARC job description does not keep enough information
    // about initial JSDL description we have to make some guesses here.
    if(!file->Source.empty()) { // input file
      // Only one source per file supported
      inputdata.push_back(FileData(fname, ""));
      if (file->Source.front() &&
          file->Source.front().Protocol() != "file") {
        inputdata.back().lfn = file->Source.front().fullstr();
        ++downloads;
      }

      if (inputdata.back().has_lfn()) {
        Arc::URL u(inputdata.back().lfn);

        if (file->IsExecutable ||
            file->Name == arc_job_desc.Application.Executable.Name) {
          u.AddOption("exec", "yes", true);
        }
        inputdata.back().lfn = u.fullstr();
      }
      else if (file->FileSize != -1) {
        inputdata.back().lfn = Arc::tostring(file->FileSize);
        if (!file->Checksum.empty()) { // Only set checksum if FileSize is also set.
          inputdata.back().lfn += "."+file->Checksum;
        }
      }
    }
    if (!file->Target.empty()) { // output file
      FileData fdata(fname, file->Target.front().fullstr());
      outputdata.push_back(fdata);
      ++uploads;

      if (outputdata.back().has_lfn()) {
        Arc::URL u(outputdata.back().lfn);
        outputdata.back().lfn = u.fullstr();
      }
    }
    if (file->KeepData) {
      // user downloadable file
      FileData fdata(fname, "");
      outputdata.push_back(fdata);
    }
  }

  // Order of the following calls matters!
  arguments.clear();
  arguments = arc_job_desc.Application.Executable.Argument;
  arguments.push_front(arc_job_desc.Application.Executable.Name);

  stdin_ = arc_job_desc.Application.Input;
  stdout_ = arc_job_desc.Application.Output;
  stderr_ = arc_job_desc.Application.Error;

  if (arc_job_desc.Resources.DiskSpaceRequirement.DiskSpace > -1)
    diskspace = (unsigned long long int)(arc_job_desc.Resources.DiskSpaceRequirement.DiskSpace*1024*1024);

  processtime = arc_job_desc.Application.ProcessingStartTime;

  const int lifetimeTemp = (int)arc_job_desc.Resources.SessionLifeTime.GetPeriod();
  if (lifetimeTemp > 0) lifetime = lifetimeTemp;

  activityid = arc_job_desc.Identification.ActivityOldId;

  stdlog = arc_job_desc.Application.LogDir;

  jobreport.clear();
  for (std::list<Arc::URL>::const_iterator it = arc_job_desc.Application.RemoteLogging.begin();
       it != arc_job_desc.Application.RemoteLogging.end(); it++) {
    jobreport.push_back(it->str());
  }

  notify.clear();
  {
    int n = 0;
    for (std::list<Arc::NotificationType>::const_iterator it = arc_job_desc.Application.Notification.begin();
         it != arc_job_desc.Application.Notification.end(); it++) {
      if (n >= 3) break; // Only 3 instances are allowed.
      std::string states;
      for (std::list<std::string>::const_iterator s = it->States.begin();
           s != it->States.end(); ++s) {
        char state = StateToShortcut(*s);
        if(state == ' ') continue;
        states+=state;
      }
      if(states.empty()) continue;
      if(it->Email.empty()) continue;
      if (!notify.empty()) notify += " ";
      notify += states + " " + it->Email;
      ++n;
    }
  }

  if (!arc_job_desc.Resources.QueueName.empty()) {
    queue = arc_job_desc.Resources.QueueName;
  }

  if (!arc_job_desc.Application.CredentialService.empty() &&
      arc_job_desc.Application.CredentialService.front())
    credentialserver = arc_job_desc.Application.CredentialService.front().fullstr();

  if (arc_job_desc.Application.Rerun > -1)
    reruns = arc_job_desc.Application.Rerun;

  if ( arc_job_desc.Application.Priority <= 100 && arc_job_desc.Application.Priority > 0 )
    priority = arc_job_desc.Application.Priority;

  return *this;
}

const char* const JobLocalDescription::transfersharedefault = "_default";

bool LRMSResult::set(const char* s) {
  // 1. Empty string = exit code 0
  if(s == NULL) s="";
  for(;*s;++s) { if(!isspace(*s)) break; };
  if(!*s) { code_=0; description_=""; };
  // Try to read first word as number
  char* e;
  code_=strtol(s,&e,0);
  if((!*e) || (isspace(*e))) {
    for(;*e;++e) { if(!isspace(*e)) break; };
    description_=e;
    return true;
  };
  // If there is no number that means some "uncoded" failure
  code_=-1;
  description_=s;
  return true;
}

std::istream& operator>>(std::istream& i,LRMSResult &r) {
  char buf[1025]; // description must have reasonable size
  if(i.eof()) { buf[0]=0; } else { istream_readline(i,buf,sizeof(buf)); };
  r=buf;
  return i;
}

std::ostream& operator<<(std::ostream& o,const LRMSResult &r) {
  o<<r.code()<<" "<<r.description();
  return o;
}

