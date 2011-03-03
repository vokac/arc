// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <signal.h>
#endif

#ifdef HAVE_GLIBMM_GETENV
#include <glibmm/miscutils.h>
#else
#include <stdlib.h>
#endif

#ifdef HAVE_GLIBMM_SETENV
#include <glibmm/miscutils.h>
#else
#include <stdlib.h>
#endif

#ifdef HAVE_GLIBMM_UNSETENV
#include <glibmm/miscutils.h>
#else
#include <stdlib.h>
#endif

#include <glibmm/module.h>
#include <glibmm/fileutils.h>

#include <string.h>

#include <arc/ArcLocation.h>
#include <arc/StringConv.h>
#include <arc/Thread.h>

#include "Utils.h"

#ifndef BUFLEN
#define BUFLEN 1024
#endif

namespace Arc {

#ifndef WIN32
  class SIGPIPEIngore {
  private:
    void (*sighandler_)(int);
  public:
    SIGPIPEIngore(void);
    ~SIGPIPEIngore(void);
  };

  SIGPIPEIngore::SIGPIPEIngore(void) {
    sighandler_ = signal(SIGPIPE,SIG_IGN);
  }

  SIGPIPEIngore::~SIGPIPEIngore(void) {
    signal(SIGPIPE,sighandler_);
  }

  static SIGPIPEIngore sigpipe_ignore;
#endif

  // The purpose of this mutex is to 'solve' problem with
  // some third party libraries which use environment variables
  // as input arguments :(
  static Glib::Mutex& env_write_lock(void) {
    static Glib::Mutex* mutex = new Glib::Mutex;
    return *mutex;
  }

  // And this mutex is needed because it seems like none if
  // underlying functions provide proper thread protection  
  // of environment variables
  static Glib::Mutex& env_read_lock(void) {
    static Glib::Mutex* mutex = new Glib::Mutex;
    return *mutex;
  }

  std::string GetEnv(const std::string& var) {
    Glib::Mutex::Lock env_lock(env_read_lock());
#ifdef HAVE_GLIBMM_GETENV
    return Glib::getenv(var);
#else
    char* val = getenv(var.c_str());
    return val ? val : "";
#endif
  }

  std::string GetEnv(const std::string& var, bool &found) {
    Glib::Mutex::Lock env_lock(env_read_lock());
#ifdef HAVE_GLIBMM_GETENV
    return Glib::getenv(var, found);
#else
    char* val = getenv(var.c_str());
    found = (val != NULL);
    return val ? val : "";
#endif
  }

  bool SetEnv(const std::string& var, const std::string& value, bool overwrite) {
    env_write_lock().lock();
    bool r = SetEnvNonLock(var, value, overwrite);
    env_write_lock().unlock();
    return r;
  }

  void UnsetEnv(const std::string& var) {
    env_write_lock().lock();
    UnsetEnvNonLock(var);
    env_write_lock().unlock();
  }

  void EnvLockAcquire(void) {
    env_write_lock().lock();
  }

  void EnvLockRelease(void) {
    env_write_lock().unlock();
  }

  bool SetEnvNonLock(const std::string& var, const std::string& value, bool overwrite) {
    Glib::Mutex::Lock env_lock(env_read_lock());
#ifdef HAVE_GLIBMM_SETENV
    return Glib::setenv(var, value, overwrite);
#else
#ifdef HAVE_SETENV
    return (setenv(var.c_str(), value.c_str(), overwrite) == 0);
#else
    return (putenv(strdup((var + "=" + value).c_str())) == 0);
#endif
#endif
  }

  void UnsetEnvNonLock(const std::string& var) {
    Glib::Mutex::Lock env_lock(env_read_lock());
#ifdef HAVE_GLIBMM_UNSETENV
    Glib::unsetenv(var);
#else
#ifdef HAVE_UNSETENV
    unsetenv(var.c_str());
#else
    putenv(strdup(var.c_str()));
#endif
#endif
  }

  std::string StrError(int errnum) {
#ifdef HAVE_STRERROR_R
    char errbuf[BUFLEN];
#ifdef STRERROR_R_CHAR_P
    return strerror_r(errnum, errbuf, sizeof(errbuf));
#else
    if (strerror_r(errnum, errbuf, sizeof(errbuf)) == 0)
      return errbuf;
    else
      return "Unknown error " + tostring(errnum);
#endif
#else
    return strerror(errnum);
#endif
  }
  
  static Glib::Mutex persistent_libraries_lock;
  static std::list<std::string> persistent_libraries_list;

  bool PersistentLibraryInit(const std::string& name) {
    // Library is made persistent by loading intermediate
    // module which depends on that library. So passed name
    // is name of that module. Modules usually reside in 
    // ARC_LOCATION/lib/arc. This approach is needed because
    // on some platforms shared libraries can't be dlopen'ed.
    std::string arc_lib_path = ArcLocation::Get();
    if(!arc_lib_path.empty()) 
      arc_lib_path = arc_lib_path + G_DIR_SEPARATOR_S + PKGLIBSUBDIR;
    std::string libpath = Glib::build_filename(arc_lib_path,"lib"+name+"."+G_MODULE_SUFFIX);

    persistent_libraries_lock.lock();
    for(std::list<std::string>::iterator l = persistent_libraries_list.begin();
            l != persistent_libraries_list.end();++l) {
      if(*l == libpath) {
        persistent_libraries_lock.unlock();
        return true;
      };
    };
    persistent_libraries_lock.unlock();
    Glib::Module *module = new Glib::Module(libpath);
    if(module && (*module)) {
      persistent_libraries_lock.lock();
      persistent_libraries_list.push_back(libpath);
      persistent_libraries_lock.unlock();
      return true;
    };
    if(module) delete module;
    return false;
  }

} // namespace Arc
